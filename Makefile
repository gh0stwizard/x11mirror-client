.PHONY: all devel clean

SOURCES = $(wildcard *.c)
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))
PROGRAM = x11mirror-client

modules = x11 xcomposite xrender xfixes xdamage xext
LDLIBS += $(shell pkg-config --libs $(modules))
LDFLAGS ?=
CFLAGS ?= -Wall -Wextra -std=c99 -pedantic

all: $(PROGRAM)

devel: CFLAGS += -D_DEBUG -g
devel: all

clean:
	$(RM) $(PROGRAM) $(OBJECTS)

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

window_dump.o:	window_dump.c list.h wsutils.h multiVis.h common.h
multiVis.o:	multiVis.c list.h wsutils.h multiVis.h
main.o:		main.c common.h window_dump.h

$(PROGRAM): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(LDLIBS)
