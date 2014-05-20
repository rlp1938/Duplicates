#ifndef _FILEUTIL_H
#define _FILEUTIL_H 1

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>

void *memmem(const void *haystack, size_t haystacklen,
               const void *needle, size_t needlelen);
struct stat sb;

struct fdata {
	char *from;
	char *to;
};
struct fdata readfile(const char *filename, off_t extra, int fatal);
FILE *dofopen(const char *path, const char *mode);

#endif /* fileutil.h */
