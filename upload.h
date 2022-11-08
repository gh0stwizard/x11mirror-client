#ifndef XMC_CURL_H
#define XMC_CURL_H


extern void
init_uploader (const char *url);


extern void
free_uploader (void);


extern int
upload_file (const char * filename);


#endif /* XMC_CURL_H */
