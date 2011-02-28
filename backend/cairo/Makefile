
CFLAGS=$(shell pkg-config cairo --cflags)
LDFLAGS=$(shell pkg-config cairo --libs)

all: print

print: print.c
	cc -o $@ $(CFLAGS) $^ $(LDFLAGS)

