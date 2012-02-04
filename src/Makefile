CC=gcc
CCFLAGS=-Wall -O2 --std=c99
LDFLAGS=-lev -lsctp

OBJECTS=evtcp.o evsctp.o tcp2sctp.o
all: tcp2sctp
.c.o:
	$(CC) $(CCFLAGS) -c $< -o $@

tcp2sctp: $(OBJECTS)
	$(CC) -o tcp2sctp $(OBJECTS) $(LDFLAGS)
clean:
	/bin/rm -f $(OBJECTS) tcp2sctp
