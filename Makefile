FPIC=-fPIC
CFLAGS=-std=c99 -Wall -g
LIBFLAGS=-shared -W1

all: libpagemap.so test

libpagemap.o:
	$(CC) $(FPIC) $(CFLAGS) -c libpagemap.c
libpagemap.so: libpagemap.o
	$(CC) $(LIBFLAGS),-soname,libpagemap.so.1 -o libpagemap.so.1.0.1 libpagemap.o
	ln -s libpagemap.so.1.0.1 libpagemap.so
test: libpagemap.so
	$(CC) $(CFLAGS) -L. -lpagemap test.c -o test
run: test
	LD_LIBRARY_PATH=. ./test
clean:
	rm -f test libpagemap.so libpagemap.so.1.0.1 libpagemap.o

