TARGET := a.out
SRC := main.c \
       wave_gen.c \
       step_source.c \
       step_gen.c \
       comm.c

CFLAGS = -Wall -g
CFLAGS += -g -DDEBUG
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

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

-include $(patsubst %.o,%.d,$(OBJS))

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<
	@$(CC) -MM $(CFLAGS) $*.c > $*.d

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: clean all
