CFLAGS ?= -O2
CPPFLAGS = -Wall -Wextra -std=c99 -pedantic -D_XOPEN_SOURCE=500 -D_POSIX_C_SOURCE=199309L
PKG_CONFIG ?= pkg-config
INSTALL ?= install
RM ?= rm -f
CURL ?= curl
UNZIP ?= unzip

MAJOR_VERSION = 1
MINOR_VERSION = 0
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

ifndef WITH_PNG
export WITH_PNG = YES
endif

ifndef WITH_JPG
export WITH_JPG = YES
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

B64_URL ?= https://sourceforge.net/projects/libb64/files/libb64/libb64/libb64-1.2.1.zip/download
B64_OUT ?= libb64-1.2.1.zip
B64_DIR = $(patsubst %.zip,%,$(B64_OUT))

#----------------------------------------------------------#

MODS_X11 ?= x11 xcomposite xrender xfixes xdamage
DEFS_X11 ?= $(shell $(PKG_CONFIG) --cflags $(MODS_X11))
LIBS_X11 ?= $(shell $(PKG_CONFIG) --libs $(MODS_X11))
IGNORE_SOURCES ?= 

ifeq ($(WITH_CURL),YES)
DEFS_CURL ?= -DHAVE_CURL $(shell $(PKG_CONFIG) --cflags libcurl)
LIBS_CURL ?= $(shell $(PKG_CONFIG) --libs libcurl)
LIBS_CURL_STATIC ?= $(shell $(PKG_CONFIG) --static --libs libcurl)
CURL_HAS_MIME = $(shell $(PKG_CONFIG) --atleast-version=7.56.0 libcurl && echo YES || echo NO)
ifeq ($(CURL_HAS_MIME),NO)
_b64 = b64
_b64clean = b64-clean
DEFS_CURL += -I$(B64_DIR)/include
LIBS_CURL += -Wl,-Bstatic -L$(B64_DIR)/src -lb64 -Wl,-Bdynamic
endif
else
IGNORE_SOURCES += upload.c
endif

ifeq ($(WITH_PNG),YES)
DEFS_PNG ?= -DHAVE_PNG $(shell $(PKG_CONFIG) --cflags libpng)
LIBS_PNG ?= $(shell $(PKG_CONFIG) --libs libpng)
LIBS_PNG_STATIC ?= $(shell $(PKG_CONFIG) --static --libs libpng)
else
IGNORE_SOURCES += topng.c
endif

ifeq ($(WITH_JPG),YES)
DEFS_JPG ?= -DHAVE_JPG $(shell $(PKG_CONFIG) --cflags libjpeg)
LIBS_JPG ?= $(shell $(PKG_CONFIG) --libs libjpeg)
LIBS_JPG_STATIC ?= $(shell $(PKG_CONFIG) --static --libs libjpeg)
else
IGNORE_SOURCES += tojpg.c
endif

#----------------------------------------------------------#

DEFS ?= $(DEFS_CURL) $(DEFS_PNG) $(DEFS_JPG) $(DEFS_X11)
DEFS += -DAPP_VERSION=$(VERSION)

LIBS ?= $(LIBS_X11) $(LIBS_PNG) $(LIBS_JPG) $(LIBS_CURL)
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


$(B64_OUT):
	$(CURL) -L $(B64_URL) -o $(B64_OUT)

$(B64_DIR)/Makefile:
	$(UNZIP) $(B64_OUT)

b64-prep: $(B64_OUT) $(B64_DIR)/Makefile

b64: b64-prep
	$(MAKE) -C $(B64_DIR) all_src

b64-clean:
	$(MAKE) -C $(B64_DIR) clean

clean:
	$(RM) $(PROGRAM) $(OBJECTS)

distclean: clean $(_b64clean)

%.o: %.c
	$(CC) $(CPPFLAGS) $(DEFS) $(CFLAGS) -o $@ -c $<

topng.o:    topng.c topng.h
tojpg.o:    tojpg.c tojpg.h
imgman.o:   imgman.c imgman.h
upload.o:   upload.c upload.h
main.o:     main.c common.h imgman.h

$(PROGRAM): $(_b64) $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(LIBS)

help:
	$(info WITH_CURL=YES|NO - enable/disable curl support, default: YES)
	$(info WITH_PNG=YES|NO - enable/disable libpng support, default: YES)
	$(info WITH_JPG=YES|NO - enable/disable libjpeg-turbo support, default: YES)
	$(info ENABLE_DELAY=YES|NO - use delay between screenshots, default: YES)
	$(info ENABLE_DEBUG=YES|NO - be verbose and print debug information, default: NO)
	$(info ENABLE_ERRORS=YES|NO - print errors to STDERR, default: YES)

.PHONY: all clean help
