TARGET := a.out
SRC := main.c \
       wave_gen.c \
       freq_gen.c \
       stepper_driver.c \
       ../libcomm/comm.c

CFLAGS = -Wall -g -I../libcomm
CFLAGS += -g
LDFLAGS = -lm

#PLATFORM = pi

ifeq ($(PLATFORM),pi)
SRC += pi_platform.c \
       pi_backend.c \
       pi_hw/pi_clk.c \
       pi_hw/pi_dma.c \
       pi_hw/pi_gpio.c \
       pi_hw/pi_util.c \
       pi_hw/mailbox.c
CFLAGS += -I/opt/vc/include
LDFLAGS += -lbcm_host -L/opt/vc/lib
else
SRC += vcd_backend.c
endif

OBJS = $(patsubst %.c,%.o,$(SRC))
DEPS = $(patsubst %.o,%.d,$(OBJS))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

-include $(DEPS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<
	@$(CC) -MM $(CFLAGS) $*.c > $*.d

clean:
	rm -f $(OBJS) $(TARGET) $(DEPS)

.PHONY: clean all
