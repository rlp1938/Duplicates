/*      fileutil.c
 *
 *	Copyright 2014 Bob Parker <rlp1938@gmail.com>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *	MA 02110-1301, USA.
*/

#include "fileutil.h"

#include <stdlib.h>
#include <string.h>

struct fdata readfile(const char *filename, off_t extra, int fatal)
{
	FILE *fpi;
	off_t bytesread;
	char *from, *to;
	struct fdata data;
	struct stat sb;

	if (stat(filename, &sb) == -1) {
		if (fatal){
			perror(filename);
			exit(EXIT_FAILURE);
		} else {
			data.from = (char *)NULL;
			data.to = (char *)NULL;
			return data;
		}
	}
	fpi = fopen(filename, "r");
	if(!(fpi)) {
		perror(filename);
		exit(EXIT_FAILURE);
	}

	from = malloc(sb.st_size + extra);
	if (!(from)) {
		perror("malloc failure");
		exit(EXIT_FAILURE);
	}

	bytesread = fread(from, 1, sb.st_size, fpi);
	if (bytesread != sb.st_size) {
		perror("fread");
		exit(EXIT_FAILURE);
	}

        fclose (fpi);

	to = from + bytesread + extra;
	// zero the extra space
	memset(from+bytesread, 0, to-(from+bytesread));
	data.from = from;
	data.to = to;
	return data;
} // readfile()

FILE *dofopen(const char *path, const char *mode)
{
	// fopen with error handling
	FILE *fp = fopen(path, mode);
	if(!(fp)){
		perror(path);
		exit(EXIT_FAILURE);
	}
	return fp;
} // dofopen()
