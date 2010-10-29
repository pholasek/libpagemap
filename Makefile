#
# libpagemap Makefile
#
CUR	:= 0
REV := 0
AGE := 0

LIBTOOL     := libtool
CMODE		:= --mode=compile
LMODE		:= --mode=link
IMODE		:= --mode=install
CLMODE		:= --mode=clean
UMODE		:= --mode=uninstall
VERSION     := -version-info $(CUR):$(REV):$(AGE)

CC			:= gcc
CFLAGS 		:= -std=c99 -Wall -O2
INSTALL     := install -D --owner=0 --group=0 
LIB64       := lib$(shell [ -d /usr/lib64 ] && echo 64)

USRLIB                  := /usr/$(LIB64)
USRINCLUDE              := /usr/include/
USRBIN                  := /usr/bin/
KMAJOR :=$(shell uname -r | cut -f1 -d.)
KMINOR :=$(shell uname -r | cut -f2 -d.)
KMICRO :=$(shell uname -r | cut -f3 -d.)
KVERSION := $(shell [ $(KMAJOR) -eq 2 ] && [ $(KMINOR) -ge 6 ] && [ $(KMICRO) -ge 25 ] && echo true)

ifneq ($(KVERSION),true) 
	@echo "Your kernel is too old."
	exit 2
endif

all: pgmap

libpagemap.lo: libpagemap.c libpagemap.h
	$(LIBTOOL) $(CMODE) $(CC) $(CFLAGS) -c libpagemap.c

libpagemap.la: libpagemap.lo
	$(LIBTOOL) $(LMODE) $(CC) $(CFLAGS) $(VERSION) -o libpagemap.la libpagemap.lo \
    	 -rpath $(USRLIB)

pgmap.o: pgmap.c
	$(CC) $(CFLAGS) -c pgmap.c 

pgmap: pgmap.o libpagemap.la
	$(LIBTOOL) $(LMODE) $(CC) $(CFLAGS) -o pgmap pgmap.o libpagemap.la

clean:
	$(LIBTOOL) $(CLMODE) rm -f pgmap libpagemap.la *.o *.la *.lo

install: libpagemap.la pgmap
	$(LIBTOOL) $(IMODE) $(INSTALL) libpagemap.la $(USRLIB)
	$(LIBTOOL) --finish $(USRLIB)
	$(LIBTOOL) $(IMODE) $(INSTALL) pgmap $(USRBIN)

uninstall: clean
	$(LIBTOOL) $(UMODE) rm -f $(USRLIB)/libpagemap.la
	rm -f $(USRBIN)pgmap

	
