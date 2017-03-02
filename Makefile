.PHONY: all devel clean

SOURCES = $(wildcard *.c)
#SOURCES = $(filter-out compression.c, $(wildcard *.c))
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))
PROGRAM = x11mirror-client

modules = x11 xcomposite xrender xfixes xdamage
#static_zlib =
static_zlib = -Wl,-Bstatic,$(shell pkg-config --libs zlib) -Wl,-Bdynamic
LDLIBS += $(static_zlib) $(shell pkg-config --libs $(modules))
LDFLAGS ?= 
CFLAGS ?= -Wall -Wextra -std=c99 -pedantic
#CFLAGS += -D_NO_ZLIB

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
