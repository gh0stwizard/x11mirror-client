CFLAGS ?= -O2
CPPFLAGS = -Wall -Wextra -std=c99 -pedantic -D_XOPEN_SOURCE=500 -D_POSIX_C_SOURCE=199309L
PKG_CONFIG ?= pkg-config
INSTALL ?= install
RM ?= rm -f

MAJOR_VERSION = 0
MINOR_VERSION = 2
PATCH_VERSION = 0
VERSION = $(MAJOR_VERSION).$(MINOR_VERSION).$(PATCH_VERSION)

DESTDIR ?= /usr/local
BINDIR ?= $(DESTDIR)/bin
LIBDIR ?= $(DESTDIR)/lib
INCLUDEDIR ?= $(DESTDIR)/include
DATAROOTDIR ?= $(DESTDIR)/share
MANDIR ?= $(DATAROOTDIR)/man

ifndef WITH_CURL
export WITH_CURL = YES
endif

ifndef WITH_ZLIB
export WITH_ZLIB = YES
endif

ifndef ENABLE_DELAY
export ENABLE_DELAY = YES
endif

ifndef ENABLE_DEBUG
export ENABLE_DEBUG = NO
endif

ifndef ENABLE_ERRORS
export ENABLE_ERRORS = YES
endif

#----------------------------------------------------------#

MODS_X11 ?= x11 xcomposite xrender xfixes xdamage
DEFS_X11 ?= $(shell $(PKG_CONFIG) --cflags $(MODS_X11))
LIBS_X11 ?= $(shell $(PKG_CONFIG) --libs $(MODS_X11))
IGNORE_SOURCES ?= 

ifeq ($(WITH_CURL),YES)
DEFS_CURL ?= -DHAVE_CURL $(shell $(PKG_CONFIG) --cflags libcurl)
LIBS_CURL ?= $(shell $(PKG_CONFIG) --libs libcurl)
LIBS_CURL_STATIC ?= $(shell $(PKG_CONFIG) --static --libs libcurl)
else
IGNORE_SOURCES += upload.c
endif

ifeq ($(WITH_ZLIB),YES)
DEFS_ZLIB ?= -DHAVE_ZLIB $(shell $(PKG_CONFIG) --cflags zlib)
LIBS_ZLIB ?= $(shell $(PKG_CONFIG) --libs zlib)
LIBS_ZLIB_STATIC ?= $(shell $(PKG_CONFIG) --static --libs zlib)
#LIBS_ZLIB_STATIC ?= -Wl,-Bstatic,$(LIBS_ZLIB) -Wl,-Bdynamic
else
IGNORE_SOURCES += compression.c
endif

#----------------------------------------------------------#

DEFS ?= $(DEFS_CURL) $(DEFS_ZLIB) $(DEFS_X11)
DEFS += -DAPP_VERSION=$(VERSION)

LIBS ?= $(LIBS_X11) $(LIBS_ZLIB) $(LIBS_CURL)
LIBS += -lm

SOURCES = $(filter-out $(IGNORE_SOURCES), $(wildcard *.c))
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))
PROGRAM = x11mirror-client

ifeq ($(ENABLE_DELAY),YES)
DEFS += -DUSE_DELAY
endif

ifeq ($(ENABLE_DEBUG),YES)
DEFS += -D_DEBUG
endif

ifeq ($(ENABLE_ERRORS),YES)
DEFS += -DPRINT_ERRORS
endif

#----------------------------------------------------------#

all: $(PROGRAM)

clean:
	$(RM) $(PROGRAM) $(OBJECTS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEFS) -o $@ -c $<

window_dump.o:	window_dump.c list.h wsutils.h multiVis.h common.h
multiVis.o:	multiVis.c list.h wsutils.h multiVis.h
main.o:		main.c common.h window_dump.h compression.h
compression.o:	compression.c window_dump.h common.h compression.h

$(PROGRAM): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(LIBS)

help:
	$(info WITH_CURL=YES|NO - enable/disable curl support, default: YES)
	$(info WITH_ZLIB=YES|NO - enable/disable zlib support, default: YES)
	$(info ENABLE_DELAY=YES|NO - use delay between screenshots, default: YES)
	$(info ENABLE_DEBUG=YES|NO - be verbose and print debug information, default: NO)
	$(info ENABLE_ERRORS=YES|NO - print errors to STDERR, default: YES)

.PHONY: all clean help
