TARGET := a.out
SRC := main.c \
       wave_gen.c \
       step_source.c \
       step_gen.c \
       vcd_backend.c \
       pi_dma/pi_clk.c \
       pi_dma/pi_dma.c \
       pi_dma/pi_gpio.c \
       pi_dma/pi_util.c \
       pi_dma/mailbox.c
OBJS = $(patsubst %.c,%.o,$(SRC))

CFLAGS = -Wall -g -I/opt/vc/include -lbcm_host -L/opt/vc/lib
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
