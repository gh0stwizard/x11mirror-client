CURL_CFLAGS = $(shell pkg-config --cflags libcurl)
CURL_LIBS = $(shell pkg-config --libs libcurl)

SOURCES = $(wildcard *.c)
#SOURCES = $(filter-out compression.c, $(wildcard *.c))
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))
PROGRAM = x11mirror-client

X11MODS = x11 xcomposite xrender xfixes xdamage
static_zlib = -Wl,-Bstatic,$(shell pkg-config --libs zlib) -Wl,-Bdynamic
#static_zlib =
LDLIBS += $(static_zlib) $(shell pkg-config --libs $(X11MODS))
LDLIBS += -lm
LDFLAGS ?= 
CFLAGS ?= -Wall -Wextra -std=c99 -pedantic
CFLAGS += -D_XOPEN_SOURCE=500
CFLAGS += -D_NO_ERRORS
#CFLAGS += -D_DEBUG
#CFLAGS += -D_NO_ZLIB
#CFLAGS += -D_NO_DELAY
#CFLAGS += -D_NO_CURL

all: $(PROGRAM)

devel: CFLAGS += -D_DEBUG -g
devel: all

clean:
	$(RM) $(PROGRAM) $(OBJECTS)

%.o: %.c
	$(CC) -c $(CFLAGS) $(CURL_CFLAGS) -o $@ $<

window_dump.o:	window_dump.c list.h wsutils.h multiVis.h common.h
multiVis.o:	multiVis.c list.h wsutils.h multiVis.h
main.o:		main.c common.h window_dump.h compression.h
compression.o:	compression.c window_dump.h common.h compression.h

$(PROGRAM): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(LDLIBS) $(CURL_LIBS)

.PHONY: all devel clean
