#ifndef XMC_CURL_H
#define XMC_CURL_H

#include <stdio.h>


extern void
init_uploader (const char *url);

extern void
free_uploader (void);

extern int
upload_file (FILE *fh);

#endif /* XMC_CURL_H */
