#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>
#include "common.h"
#include "compression.h"

#ifndef _ZLIB_CHUNK
#define _ZLIB_CHUNK 16384
#endif


extern int
save_gzip_file (mirrorDump* dump, FILE *out, int level)
{
    static int errnum;

    int fd = dup (fileno (out));
    if (fd == -1) {
        perror ("dup");
        exit (EXIT_FAILURE);
    }

    gzFile gz_file = gzdopen (fd, "wb");
    if (gz_file == NULL)
        die ("gzdopen failed.");

    gzrewind (gz_file);
    gzsetparams (gz_file, level, Z_DEFAULT_STRATEGY);

    debug ("gzip: Dumping file header.\n");

    if (gzwrite (gz_file, (char *)&(dump->header), SIZEOF(XWDheader)) == 0)
        die ("gzfwrite failed to write header: %s", gzerror (gz_file, &errnum));

    if (dump->window_name != NULL) {
        if (gzwrite (gz_file, dump->window_name, dump->window_name_size) == 0)
            die ("gzfwrite failed to write window name: %s",
                gzerror (gz_file, &errnum));
    }

    debug ("gzip: Dumping %zu colors.\n", dump->xwdcolors_count);

    if (gzwrite (gz_file, (char *)dump->xwdcolors,
        SIZEOF(XWDColor) * dump->xwdcolors_count) == 0)
        die ("gzfwrite failed to write colors: %s", gzerror (gz_file, &errnum));

    debug ("gzip: Dumping pixmap.  bufsize = %zu\n", dump->image_data_size);

    if (gzwrite (gz_file, dump->image->data, dump->image_data_size) == 0)
        die ("gzfwrite failed to write pixmap: %s", gzerror (gz_file, &errnum));

    gzflush (gz_file, Z_FINISH);
    gzclose_w (gz_file);

    return 0;
}
