CC=gcc
CFLAGS=-O0 --static -g -Wall
LDFLAGS=-levent
OBJ=ontpd.o socket.o

ontpd: $(OBJ)
	gcc --static -o $@ $(LDFLAGS) $+ /usr/lib/libevent.a /usr/lib/libc.a /usr/lib/librt.a

clean:
	-rm -f $(OBJ) ontpd core*
