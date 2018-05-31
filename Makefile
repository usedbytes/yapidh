TARGET := a.out
SRC := main.c wave_gen.c step_source.c step_gen.c gnuplot_backend.c
OBJS = $(patsubst %.c,%.o,$(SRC))

CFLAGS = -Wall -g
LDFLAGS = -lm

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

-include $(patsubst %.o,%.d,$(OBJS))

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<
	$(CC) -MM $(CFLAGS) $*.c > $*.d

clean:
	rm -f *.o $(TARGET)

.PHONY: clean all
