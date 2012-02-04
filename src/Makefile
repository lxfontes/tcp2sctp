CC=gcc
CCFLAGS=-Wall -O2 --std=c99 -DDEBUG
LDFLAGS=-lev -lsctp

OBJECTS=evtcp.o evsctp.o tcp2sctp.o
all: tcp2sctp
.c.o:
	$(CC) $(CCFLAGS) -c $< -o $@

tcp2sctp: endian $(OBJECTS)
	$(CC) -o tcp2sctp $(OBJECTS) $(LDFLAGS)
endian: cdiam_endian.c
	$(CC) -o cdiam_endian cdiam_endian.c
	./cdiam_endian > cdiam_endian.h
clean:
	/bin/rm -f $(OBJECTS) tcp2sctp

.PHONY: all endian
