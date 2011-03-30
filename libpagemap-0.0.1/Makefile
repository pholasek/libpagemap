#
# libpagemap Makefile
#

MAJOR 		:= 0
MINOR		:= 0
MICRO 		:= 0
VERSION		:= $(MAJOR).$(MINOR).$(MICRO)
CC			:= gcc
CFLAGS 		:= $(CFLAGS) -std=c99 -Wall
LFLAGS		:= -fPIC
INSTALL     := install -Dp #--owner=0 --group=0 
LIB64       := lib$(shell [ -d /usr/lib64 ] && echo 64)
LNAME 		:= libpagemap.so
SONAME		:= $(LNAME).$(MAJOR)

USRLIB                  := $(DESTDIR)/usr/$(LIB64)
USRINCLUDE              := $(DESTDIR)/usr/include
USRBIN                  := $(DESTDIR)/usr/bin
MANDIR					:= $(DESTDIR)/usr/share/man/man1
#KMAJOR :=$(shell uname -r | cut -f1 -d.)
#KMINOR :=$(shell uname -r | cut -f2 -d.)
#KMICRO :=$(shell uname -r | cut -f3 -d.)
#KVERSION := $(shell [ $(KMAJOR) -eq 2 ] && [ $(KMINOR) -ge 6 ] && [ $(KMICRO) -ge 25 ] && echo true)

#ifneq ($(KVERSION),true) 
#	@echo "Your kernel is too old."
#	exit 2
#endif

all: libpagemap.so pgmap

libpagemap.o: libpagemap.c libpagemap.h
	$(CC) $(CFLAGS) $(LFLAGS) -c libpagemap.c

libpagemap.so: libpagemap.o
	$(CC) $(CFLAGS) -shared -Wl,-soname,$(SONAME) -o $(LNAME).$(VERSION) libpagemap.o -lc
	ln -s $(LNAME).$(VERSION) $(SONAME)

pgmap.o: pgmap.c
	$(CC) $(CFLAGS) -c pgmap.c 

pgmap: pgmap.o libpagemap.so
	$(CC) $(CFLAGS) -o pgmap pgmap.o $(SONAME)

clean:
	rm -f pgmap libpagemap.la *.o *.la *.lo *.so*

install: 
	$(INSTALL) $(LNAME).$(VERSION) $(USRLIB)/$(LNAME).$(VERSION)
	cd $(USRLIB) && ln -s $(LNAME).$(VERSION) $(SONAME) && ln -s $(LNAME).$(VERSION) $(LNAME) && cd -
	$(INSTALL) pgmap $(USRBIN)/pgmap
	$(INSTALL) pgmap.1 $(MANDIR)/pgmap.1
	$(INSTALL) -m 0644 libpagemap.h $(USRINCLUDE)/libpagemap.h
	


	
