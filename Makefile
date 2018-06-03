TARGET := a.out
SRC := main.c \
       wave_gen.c \
       step_source.c \
       step_gen.c \
       pi_backend.c \
       pi_hw/pi_clk.c \
       pi_hw/pi_dma.c \
       pi_hw/pi_gpio.c \
       pi_hw/pi_util.c \
       pi_hw/mailbox.c
OBJS = $(patsubst %.c,%.o,$(SRC))

CFLAGS = -Wall -I/opt/vc/include -lbcm_host -L/opt/vc/lib
CFLAGS += -g -DDEBUG
LDFLAGS = -lm

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
