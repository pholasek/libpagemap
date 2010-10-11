FPIC=-fPIC
CFLAGS=-std=c99 -Wall -g
LIBFLAGS=-shared -W1

all: libpagemap.so test pgmap

libpagemap.o:
	$(CC) $(FPIC) $(CFLAGS) -c libpagemap.c
libpagemap.so: libpagemap.o
	$(CC) $(LIBFLAGS),-soname,libpagemap.so.1 -o libpagemap.so.1.0.1 libpagemap.o
	ln -s libpagemap.so.1.0.1 libpagemap.so
test: libpagemap.so
	$(CC) $(CFLAGS) -L. -lpagemap test.c -o test
pgmap: libpagemap.so
	$(CC) $(CFLAGS) -L. -lpagemap pgmap.c -o pgmap
run: pgmap test
	#LD_LIBRARY_PATH=. ./test
	LD_LIBRARY_PATH=. ./pgmap
clean:
	rm -f test libpagemap.so libpagemap.so.1.0.1 libpagemap.o

