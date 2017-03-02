#ifndef _COMPRESSION_H
#define _COMPRESSION_H

#include "window_dump.h"

extern int
save_gzip_file (mirrorDump *dump, FILE *out, int level);

#endif /* _COMPRESSION_H */
