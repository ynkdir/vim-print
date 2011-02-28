
CFLAGS=$(shell pkg-config pangocairo --cflags)
LDFLAGS=$(shell pkg-config pangocairo --libs)

all: print

print: print.c
	cc -o $@ $(CFLAGS) $^ $(LDFLAGS)

