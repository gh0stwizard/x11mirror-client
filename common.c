#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "common.h"


extern void
die (const char *fmt, ...)
{
    va_list args;
    va_start (args, fmt);
    fprintf (stderr, "error: ");
    vfprintf (stderr, fmt, args);
    fprintf (stderr, "\n");
    va_end (args);
    exit (EXIT_FAILURE);
}


extern void
debug_window (Display *d, Window w, XWindowAttributes *wa)
{
#define BACKSTORE(i) ( (i == NotUseful) \
    ? ("Not Useful") \
    : (i == WhenMapped) ? ("When Mapped") : ("Always") )

    (void)wa;
    char *window_name = NULL;

/* TODO: _NET_WM_NAME */
    XFetchName (d, w, &window_name);

    if (window_name != NULL) {
        debug ("  name: %s\n", window_name);
        XFree (window_name);
    }

    debug ("  x: %d y: %d\n", wa->x, wa->y);
    debug ("  width: %d height: %d\n", wa->width, wa->height);
    debug ("  root: 0x%lx\n", wa->root);
    debug ("  backing store: %s\n", BACKSTORE(wa->backing_store));
#undef BACKSTORE
}


extern unsigned long
get_shift (unsigned long mask)
{
    unsigned long shift = 0;
    while (mask) {
        if (mask & 1) break;
        shift++;
        mask >>=1;
    }
    return shift;
}
