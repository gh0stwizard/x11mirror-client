#ifndef __COMMON_H
#define __COMMON_H

#if defined (_DEBUG)
#define debug(...) fprintf (stderr, __VA_ARGS__)
#else
#define debug(...) do { /* nop */ } while (0)
#endif

void die (const char *fmt, ...);

#endif /* __COMMON_H */
