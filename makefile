

all : pkg-mirror

CFLAGS = -I/usr/local/include -Wno-writable-strings
LDFLAGS = -L/usr/local/lib -ljansson -lsqlite3 -lcrypto

main.o : main.c
	cc -c $(CFLAGS) main.c

pkg-mirror : main.o
	cc -o pkg-mirror $(LDFLAGS) main.o

clean: 
	rm -rf pkg-mirror
	rm -rf *.o
