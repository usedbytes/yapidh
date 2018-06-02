/*
 * servod.c Multiple Servo Driver for the RaspberryPi
 * Copyright (c) 2013 Richard Hirst <richardghirst@gmail.com>
 *
 * This program provides very similar functionality to servoblaster, except
 * that rather than implementing it as a kernel module, servod implements
 * the functionality as a usr space daemon.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* TODO: Separate idle timeout handling from genuine set-to-zero requests */
/* TODO: Add ability to specify time frame over which an adjustment should be made */
/* TODO: Add servoctl utility to set and query servo positions, etc */
/* TODO: Add slow-start option */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <getopt.h>
#include <math.h>
#include <bcm_host.h>

#include "mailbox.h"

#define DMY	255	// Used to represent an invalid P1 pin, or unmapped servo

#define NUM_P1PINS	40
#define NUM_P5PINS	8

#define MAX_SERVOS	32	/* Only 21 really, but this lets you map servo IDs
				 * to P1 pins, if you want to
				 */
#define MAX_MEMORY_USAGE	(16*1024*1024)	/* Somewhat arbitrary limit of 16MB */

#define DEFAULT_CYCLE_TIME_US	20000
#define DEFAULT_STEP_TIME_US	10
#define DEFAULT_SERVO_MIN_US	500
#define DEFAULT_SERVO_MAX_US	2500

#define DEVFILE			"/dev/servoblaster"
#define CFGFILE			"/dev/servoblaster-cfg"

#define PAGE_SIZE		4096
#define PAGE_SHIFT		12

#define DMA_CHAN_SIZE		0x100
#define DMA_CHAN_MIN		0
#define DMA_CHAN_MAX		14
#define DMA_CHAN_DEFAULT	14

#define DMA_BASE_OFFSET		0x00007000
#define DMA_LEN			DMA_CHAN_SIZE * (DMA_CHAN_MAX+1)
#define PWM_BASE_OFFSET		0x0020C000
#define PWM_LEN			0x28
#define CLK_BASE_OFFSET	        0x00101000
#define CLK_LEN			0xA8
#define GPIO_BASE_OFFSET	0x00200000
#define GPIO_LEN		0x100
#define PCM_BASE_OFFSET		0x00203000
#define PCM_LEN			0x24

#define DMA_VIRT_BASE		(periph_virt_base + DMA_BASE_OFFSET)
#define PWM_VIRT_BASE		(periph_virt_base + PWM_BASE_OFFSET)
#define CLK_VIRT_BASE		(periph_virt_base + CLK_BASE_OFFSET)
#define GPIO_VIRT_BASE		(periph_virt_base + GPIO_BASE_OFFSET)
#define PCM_VIRT_BASE		(periph_virt_base + PCM_BASE_OFFSET)

#define PWM_PHYS_BASE		(periph_phys_base + PWM_BASE_OFFSET)
#define PCM_PHYS_BASE		(periph_phys_base + PCM_BASE_OFFSET)
#define GPIO_PHYS_BASE		(periph_phys_base + GPIO_BASE_OFFSET)

#define DMA_NO_WIDE_BURSTS	(1<<26)
#define DMA_WAIT_RESP		(1<<3)
#define DMA_D_DREQ		(1<<6)
#define DMA_PER_MAP(x)		((x)<<16)
#define DMA_END			(1<<1)
#define DMA_RESET		(1<<31)
#define DMA_INT			(1<<2)
#define DMA_SRC_IGNORE		(1<<11)
#define DMA_TDMODE		(1<<1)

#define DMA_CS			(0x00/4)
#define DMA_CONBLK_AD		(0x04/4)
#define DMA_SOURCE_AD		(0x0c/4)
#define DMA_DEBUG		(0x20/4)

#define GPIO_FSEL0		(0x00/4)
#define GPIO_SET0		(0x1c/4)
#define GPIO_CLR0		(0x28/4)
#define GPIO_LEV0		(0x34/4)
#define GPIO_PULLEN		(0x94/4)
#define GPIO_PULLCLK		(0x98/4)

#define GPIO_MODE_IN		0
#define GPIO_MODE_OUT		1

#define PWM_CTL			(0x00/4)
#define PWM_DMAC		(0x08/4)
#define PWM_RNG1		(0x10/4)
#define PWM_FIFO		(0x18/4)

#define PWMCLK_CNTL		40
#define PWMCLK_DIV		41

#define PWMCTL_MODE1		(1<<1)
#define PWMCTL_PWEN1		(1<<0)
#define PWMCTL_CLRF		(1<<6)
#define PWMCTL_USEF1		(1<<5)

#define PWMDMAC_ENAB		(1<<31)
#define PWMDMAC_THRSHLD		((15<<8)|(15<<0))

#define PCM_CS_A		(0x00/4)
#define PCM_FIFO_A		(0x04/4)
#define PCM_MODE_A		(0x08/4)
#define PCM_RXC_A		(0x0c/4)
#define PCM_TXC_A		(0x10/4)
#define PCM_DREQ_A		(0x14/4)
#define PCM_INTEN_A		(0x18/4)
#define PCM_INT_STC_A		(0x1c/4)
#define PCM_GRAY		(0x20/4)

#define PCMCLK_CNTL		38
#define PCMCLK_DIV		39

#define DELAY_VIA_PWM		0
#define DELAY_VIA_PCM		1

#define ROUNDUP(val, blksz)	(((val)+((blksz)-1)) & ~(blksz-1))

typedef struct {
	uint32_t info, src, dst, length,
		 stride, next, pad[2];
} dma_cb_t;

#define BUS_TO_PHYS(x) ((x)&~0xC0000000)

// cycle_time_us is the pulse cycle time per servo, in microseconds.
// Typically it should be 20ms, or 20000us.

// step_time_us is the pulse width increment granularity, again in microseconds.
// Setting step_time_us too low will likely cause problems as the DMA controller
// will use too much memory bandwidth.  10us is a good value, though you
// might be ok setting it as low as 2us.

static int cycle_time_us;
static int step_time_us;

static uint8_t servo2gpio[MAX_SERVOS];
static uint32_t gpiomode[MAX_SERVOS];
static int restore_gpio_modes;

static volatile uint32_t *pwm_reg;
static volatile uint32_t *pcm_reg;
static volatile uint32_t *clk_reg;
static volatile uint32_t *dma_reg;
static volatile uint32_t *gpio_reg;

static int delay_hw = DELAY_VIA_PWM;

static int dma_chan;
static int idle_timeout;
static int invert = 0;
static int num_cbs;
static int num_pages;
static dma_cb_t *cb_base;

static int board_model;
static int gpio_cfg;

static uint32_t periph_phys_base;
static uint32_t periph_virt_base;
static uint32_t dram_phys_base;
static uint32_t mem_flag;

uint32_t phys_fifo_addr, cbinfo;
uint32_t phys_gpclr0;
uint32_t phys_gpset0;

static struct {
	int handle;		/* From mbox_open() */
	uint32_t size;		/* Required size */
	unsigned mem_ref;	/* From mem_alloc() */
	unsigned bus_addr;	/* From mem_lock() */
	uint8_t *virt_addr;	/* From mapmem() */
} mbox;
	
static void gpio_set_mode(uint32_t gpio, uint32_t mode);

static void
udelay(int us)
{
	struct timespec ts = { 0, us * 1000 };

	nanosleep(&ts, NULL);
}

static void
terminate(int dummy)
{
	int i;

	if (dma_reg && mbox.virt_addr) {
		udelay(cycle_time_us);
		dma_reg[DMA_CS] = DMA_RESET;
		udelay(10);
	}
	if (restore_gpio_modes) {
		for (i = 0; i < MAX_SERVOS; i++) {
			if (servo2gpio[i] != DMY)
				gpio_set_mode(servo2gpio[i], gpiomode[i]);
		}
	}
	if (mbox.virt_addr != NULL) {
		unmapmem(mbox.virt_addr, mbox.size);
		mem_unlock(mbox.handle, mbox.mem_ref);
		mem_free(mbox.handle, mbox.mem_ref);
		if (mbox.handle >= 0)
			mbox_close(mbox.handle);
	}

	unlink(DEVFILE);
	unlink(CFGFILE);
	exit(1);
}

static void
fatal(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	terminate(0);
}

static uint32_t gpio_get_mode(uint32_t gpio)
{
	uint32_t fsel = gpio_reg[GPIO_FSEL0 + gpio/10];

	return (fsel >> ((gpio % 10) * 3)) & 7;
}

static void
gpio_set_mode(uint32_t gpio, uint32_t mode)
{
	uint32_t fsel = gpio_reg[GPIO_FSEL0 + gpio/10];

	fsel &= ~(7 << ((gpio % 10) * 3));
	fsel |= mode << ((gpio % 10) * 3);
	gpio_reg[GPIO_FSEL0 + gpio/10] = fsel;
}

static void
gpio_set(int gpio, int level)
{
	if (level)
		gpio_reg[GPIO_SET0] = 1 << gpio;
	else
		gpio_reg[GPIO_CLR0] = 1 << gpio;
}

static uint32_t
mem_virt_to_phys(void *virt)
{
	uint32_t offset = (uint8_t *)virt - mbox.virt_addr;

	return mbox.bus_addr + offset;
}

static void *
map_peripheral(uint32_t base, uint32_t len)
{
	int fd = open("/dev/mem", O_RDWR|O_SYNC);
	void * vaddr;

	if (fd < 0)
		fatal("servod: Failed to open /dev/mem: %m\n");
	vaddr = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, base);
	if (vaddr == MAP_FAILED)
		fatal("servod: Failed to map peripheral at 0x%08x: %m\n", base);
	close(fd);

	return vaddr;
}

static void
setup_sighandlers(void)
{
	int i;

	// Catch all signals possible - it is vital we kill the DMA engine
	// on process exit!
	for (i = 0; i < 64; i++) {
		struct sigaction sa;

		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = terminate;
		sigaction(i, &sa, NULL);
	}
}

static void
init_hardware(void)
{
	if (delay_hw == DELAY_VIA_PWM) {
		// Initialise PWM
		pwm_reg[PWM_CTL] = 0;
		udelay(10);
		clk_reg[PWMCLK_CNTL] = 0x5A000006;		// Source=PLLD (500MHz)
		udelay(100);
		clk_reg[PWMCLK_DIV] = 0x5A000000 | (500<<12);	// set pwm div to 500, giving 1MHz
		udelay(100);
		clk_reg[PWMCLK_CNTL] = 0x5A000016;		// Source=PLLD and enable
		udelay(100);
		pwm_reg[PWM_RNG1] = step_time_us;
		udelay(10);
		pwm_reg[PWM_DMAC] = PWMDMAC_ENAB | PWMDMAC_THRSHLD;
		udelay(10);
		pwm_reg[PWM_CTL] = PWMCTL_CLRF;
		udelay(10);
		pwm_reg[PWM_CTL] = PWMCTL_USEF1 | PWMCTL_PWEN1;
		udelay(10);
	} else {
		// Initialise PCM
		pcm_reg[PCM_CS_A] = 1;				// Disable Rx+Tx, Enable PCM block
		udelay(100);
		clk_reg[PCMCLK_CNTL] = 0x5A000006;		// Source=PLLD (500MHz)
		udelay(100);
		clk_reg[PCMCLK_DIV] = 0x5A000000 | (500<<12);	// Set pcm div to 500, giving 1MHz
		udelay(100);
		clk_reg[PCMCLK_CNTL] = 0x5A000016;		// Source=PLLD and enable
		udelay(100);
		pcm_reg[PCM_TXC_A] = 0<<31 | 1<<30 | 0<<20 | 0<<16; // 1 channel, 8 bits
		udelay(100);
		pcm_reg[PCM_MODE_A] = (step_time_us - 1) << 10;
		udelay(100);
		pcm_reg[PCM_CS_A] |= 1<<4 | 1<<3;		// Clear FIFOs
		udelay(100);
		pcm_reg[PCM_DREQ_A] = 64<<24 | 64<<8;		// DMA Req when one slot is free?
		udelay(100);
		pcm_reg[PCM_CS_A] |= 1<<9;			// Enable DMA
		udelay(100);
	}

	// Initialise the DMA
	dma_reg[DMA_CS] = DMA_RESET;
	udelay(10);
	dma_reg[DMA_CS] = DMA_INT | DMA_END;
	dma_reg[DMA_CONBLK_AD] = mem_virt_to_phys(cb_base);
	dma_reg[DMA_DEBUG] = 7; // clear debug error flags
	dma_reg[DMA_CS] = 0x10880001;	// go, mid priority, wait for outstanding writes

	if (delay_hw == DELAY_VIA_PCM) {
		pcm_reg[PCM_CS_A] |= 1<<2;			// Enable Tx
	}
}

static void
do_status(char *filename)
{
	uint32_t last;
	int status = -1;
	char *p;
	int fd;
	const char *dma_dead = "ERROR: DMA not running\n";

	while (*filename == ' ')
		filename++;
	p = filename + strlen(filename) - 1;
	while (p > filename && (*p == '\n' || *p == '\r' || *p == ' '))
		*p-- = '\0';

	last = dma_reg[DMA_CONBLK_AD];
	udelay(step_time_us*2);
	if (dma_reg[DMA_CONBLK_AD] != last)
		status = 0;
	if ((fd = open(filename, O_WRONLY|O_CREAT, 0666)) >= 0) {
		if (status == 0)
			write(fd, "OK\n", 3);
		else
			write(fd, dma_dead, strlen(dma_dead));
		close(fd);
	} else {
		printf("Failed to open %s for writing: %m\n", filename);
	}
}

/* Determining the board revision is a lot more complicated than it should be
 * (see comments in wiringPi for details).  We will just look at the last two
 * digits of the Revision string and treat '00' and '01' as errors, '02' and
 * '03' as rev 1, and any other hex value as rev 2.  'Pi1 and Pi2 are
 * differentiated by the Hardware being BCM2708 or BCM2709.
 */
static void
get_model_and_revision(void)
{
	char buf[128], revstr[128], modelstr[128];
	char *ptr, *end, *res;
	int board_revision;
	FILE *fp;

	revstr[0] = modelstr[0] = '\0';

	fp = fopen("/proc/cpuinfo", "r");

	if (!fp)
		fatal("Unable to open /proc/cpuinfo: %m\n");

	while ((res = fgets(buf, 128, fp))) {
		if (!strncasecmp("hardware", buf, 8))
			memcpy(modelstr, buf, 128);
		else if (!strncasecmp(buf, "revision", 8))
			memcpy(revstr, buf, 128);
	}
	fclose(fp);

	if (modelstr[0] == '\0')
		fatal("servod: No 'Hardware' record in /proc/cpuinfo\n");
	if (revstr[0] == '\0')
		fatal("servod: No 'Revision' record in /proc/cpuinfo\n");

	if (strstr(modelstr, "BCM2708"))
		board_model = 1;
	else if (strstr(modelstr, "BCM2709") || strstr(modelstr, "BCM2835"))
		board_model = 2;
	else
		fatal("servod: Cannot parse the hardware name string\n");

	/* Revisions documented at http://elinux.org/RPi_HardwareHistory */
	ptr = revstr + strlen(revstr) - 3;
	board_revision = strtol(ptr, &end, 16);
	if (end != ptr + 2)
		fatal("servod: Failed to parse Revision string\n");
	if (board_revision < 1)
		fatal("servod: Invalid board Revision\n");
	else if (board_revision < 4)
		gpio_cfg = 1;
	else if (board_revision < 16)
		gpio_cfg = 2;
	else
		gpio_cfg = 3;

	periph_virt_base = bcm_host_get_peripheral_address();
	dram_phys_base = bcm_host_get_sdram_address();
	periph_phys_base = 0x7e000000;
	/*
	 * See https://github.com/raspberrypi/firmware/wiki/Mailbox-property-interface
	 *
	 * 1:  MEM_FLAG_DISCARDABLE = 1 << 0	// can be resized to 0 at any time. Use for cached data
	 *     MEM_FLAG_NORMAL = 0 << 2		// normal allocating alias. Don't use from ARM
	 * 4:  MEM_FLAG_DIRECT = 1 << 2		// 0xC alias uncached
	 * 8:  MEM_FLAG_COHERENT = 2 << 2	// 0x8 alias. Non-allocating in L2 but coherent
	 *     MEM_FLAG_L1_NONALLOCATING =	// Allocating in L2
	 *       (MEM_FLAG_DIRECT | MEM_FLAG_COHERENT)
	 * 16: MEM_FLAG_ZERO = 1 << 4		// initialise buffer to all zeros
	 * 32: MEM_FLAG_NO_INIT = 1 << 5	// don't initialise (default is initialise to all ones
	 * 64: MEM_FLAG_HINT_PERMALOCK = 1 << 6	// Likely to be locked for long periods of time
	 *
	 */
	if (board_model == 1) {
		mem_flag         = 0x0c;	/* MEM_FLAG_DIRECT | MEM_FLAG_COHERENT */
	} else {
		mem_flag         = 0x04;	/* MEM_FLAG_DIRECT */
	}
}

int
main(int argc, char **argv)
{
	int i;

	get_model_and_revision();
	//dma_chan = DMA_CHAN_DEFAULT;
	dma_chan = 6;
	idle_timeout = 0;
	cycle_time_us = DEFAULT_CYCLE_TIME_US;
	step_time_us = DEFAULT_STEP_TIME_US;
	servo2gpio[0] = 4;

	num_cbs = 512;
	num_pages =   (num_cbs * sizeof(dma_cb_t) + (PAGE_SIZE - 1)) >> PAGE_SHIFT;

	if (num_pages > MAX_MEMORY_USAGE / PAGE_SIZE) {
		fatal("Using too much memory; reduce cycle-time or increase step-size\n");
	}

	setup_sighandlers();

	dma_reg = map_peripheral(DMA_VIRT_BASE, DMA_LEN);
	dma_reg += dma_chan * DMA_CHAN_SIZE / sizeof(uint32_t);
	pwm_reg = map_peripheral(PWM_VIRT_BASE, PWM_LEN);
	pcm_reg = map_peripheral(PCM_VIRT_BASE, PCM_LEN);
	clk_reg = map_peripheral(CLK_VIRT_BASE, CLK_LEN);
	gpio_reg = map_peripheral(GPIO_VIRT_BASE, GPIO_LEN);

	/* Use the mailbox interface to the VC to ask for physical memory */
	// Use the mailbox interface to request memory from the VideoCore
	// We specifiy (-1) for the handle rather than calling mbox_open()
	// so multiple users can share the resource.
	mbox.handle = -1; // mbox_open();
	mbox.size = num_pages * 4096;
	mbox.mem_ref = mem_alloc(mbox.handle, mbox.size, 4096, mem_flag);
	if (mbox.mem_ref < 0) {
		fatal("Failed to alloc memory from VideoCore\n");
	}
	mbox.bus_addr = mem_lock(mbox.handle, mbox.mem_ref);
	if (mbox.bus_addr == ~0) {
		mem_free(mbox.handle, mbox.size);
		fatal("Failed to lock memory\n");
	}
	mbox.virt_addr = mapmem(BUS_TO_PHYS(mbox.bus_addr), mbox.size);
	memset(mbox.virt_addr, 0, mbox.size);
	cb_base = (dma_cb_t *)(mbox.virt_addr);

	for (i = 0; i < MAX_SERVOS; i++) {
		if (servo2gpio[i] == DMY)
			continue;
		gpiomode[i] = gpio_get_mode(servo2gpio[i]);
		gpio_set(servo2gpio[i], invert ? 1 : 0);
		gpio_set_mode(servo2gpio[i], GPIO_MODE_OUT);
	}
	restore_gpio_modes = 1;

	init_hardware();

	for (;;) { }

	return 0;
}

