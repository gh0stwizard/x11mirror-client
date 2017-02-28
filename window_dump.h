#ifndef __WINDOW_DUMP_H
#define __WINDOW_DUMP_H

#include <stdio.h>

void
Window_Dump (Display *dpy, int screen, Window window, FILE *out);

void
Pixmap_Dump (Display *dpy, int screen, Window window, Pixmap pixmap, FILE *out);

#endif /* __WINDOW_DUMP_H */
