#ifndef __WINDOW_DUMP_H
#define __WINDOW_DUMP_H

#include <stdio.h>
#include <X11/XWDFile.h>

typedef struct mirror_dump_s {
    XWDFileHeader   header;
    char            *window_name;
    size_t          window_name_size;
    XWDColor        *xwdcolors;
    size_t          xwdcolors_count;
    char            *image_data;
    size_t          image_data_size;
    XImage          *image;
} mirrorDump;

mirrorDump *
Pixmap_Dump (Display *d, int s, Window w, Pixmap p);

void
Free_Dump (mirrorDump *dump);

void
Save_Dump (mirrorDump *dump, FILE *out_file);

#endif /* __WINDOW_DUMP_H */
