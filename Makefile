TARGET := yapidh
SRC := main.c \
       wave_gen.c

SRC += vcd_backend.c

CFLAGS = -Wall -g
LDFLAGS = -lm

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
