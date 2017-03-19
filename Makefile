CURL_CFLAGS = $(shell pkg-config --cflags libcurl)
CURL_LIBS = $(shell pkg-config --libs libcurl)

ZLIB_CFLAGS = $(shell pkg-config --cflags zlib)
ZLIB_LIBS = $(shell pkg-config --libs zlib)
ZLIB_LIBS_STATIC = -Wl,-Bstatic,$(ZLIB_LIBS) -Wl,-Bdynamic

X11MODS = x11 xcomposite xrender xfixes xdamage
X11_CFLAGS = $(shell pkg-config --cflags $(X11MODS))
X11_LIBS = $(shell pkg-config --libs $(X11MODS))

LDLIBS ?= $(X11_LIBS) $(ZLIB_LIBS) $(CURL_LIBS)
LDLIBS += -lm
LDFLAGS ?= 

CFLAGS ?= -Wall -Wextra -std=c99 -pedantic \
          $(CURL_CFLAGS) $(ZLIB_CFLAGS) $(X11_CFLAGS)
CFLAGS += -D_XOPEN_SOURCE=500
CFLAGS += -D_NO_ERRORS
#CFLAGS += -D_DEBUG
#CFLAGS += -D_NO_DELAY
#CFLAGS += -D_NO_ZLIB
#CFLAGS += -D_NO_CURL

#IGN_SRC = compression.c upload.c
SOURCES = $(filter-out $(IGN_SRC), $(wildcard *.c))
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))
PROGRAM = x11mirror-client

all: $(PROGRAM)

devel: CFLAGS += -D_DEBUG -g
devel: all

clean:
	$(RM) $(PROGRAM) $(OBJECTS)

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

window_dump.o:	window_dump.c list.h wsutils.h multiVis.h common.h
multiVis.o:	multiVis.c list.h wsutils.h multiVis.h
main.o:		main.c common.h window_dump.h compression.h
compression.o:	compression.c window_dump.h common.h compression.h

$(PROGRAM): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(LDLIBS)

.PHONY: all devel clean
