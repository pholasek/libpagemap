#
# libpagemap Makefile
#
MAJOR 		:= 0
MINOR 		:= 1

FPIC 		:= -fPIC
CFLAGS 		:= -std=c99 -Wall -O2 -s
VSCRIPT 	:= versions.ldscript
LIBFLAGS 	:= -shared -W1,--version-script,$(VSCRIPT)
INSTALL     := install -D --owner 0 --group 0
LIB64       := lib$(shell [ -d /lib64 ] && echo 64)

LIB                      := $(DESTDIR)/$(LIB64)/
usr/lib                  := $(DESTDIR)/usr/$(LIB64)/
usr/include              := $(DESTDIR)/usr/include/
usr/bin                  := $(DESTDIR)/usr/bin/

all: libpagemap.so pgmap

libpagemap.o: libpagemap.c libpagemap.h
	$(CC) $(FPIC) $(CFLAGS) -c libpagemap.c

libpagemap.so: libpagemap.o
	$(CC) $(LIBFLAGS),-soname,libpagemap.so.$(MAJOR) -o libpagemap.so.$(MAJOR).$(MINOR) libpagemap.o

pgmap: libpagemap.so
	$(CC) $(CFLAGS) -L. -lpagemap pgmap.c -o pgmap

clean:
	rm -f libpagemap.so libpagemap.so.$(MAJOR).$(MINOR) libpagemap.o pgmap

install:
	
