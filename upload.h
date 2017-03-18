#ifndef XMC_CURL_H
#define XMC_CURL_H

#include <stdio.h>


extern void
init_uploader (const char *url);

extern int
upload_file (FILE *fh);


#endif /* XMC_CURL_H */
