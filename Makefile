CC=gcc
CFLAGS=-O0 -g -Wall
LDFLAGS=
OBJ=ontpd.o socket.o

MDEFS := $(shell sh Makefile.defs.sh >Makefile.defs)
include Makefile.defs

ontpd: $(OBJ)
	gcc -o $@ $+  $(LDFLAGS)

clean:
	-rm -f $(OBJ) ontpd core*
	-rm -f Makefile.defs
