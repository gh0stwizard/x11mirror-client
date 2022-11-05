#ifndef X11MIRROR_COMMON_H
#define X11MIRROR_COMMON_H

#include <stdio.h>
#include <X11/Xlib.h>

#define warn(...) fprintf (stderr, __VA_ARGS__)

#if defined (_DEBUG)
#define debug(...) fprintf (stderr, __VA_ARGS__)
#else
#define debug(...) do { /* nop */ } while (0)
#endif


extern void
die (const char *fmt, ...);


extern void
debug_window (Display *d, Window w, XWindowAttributes *wa);


extern unsigned long
get_shift (unsigned long mask);


#endif /* X11MIRROR_COMMON_H */
