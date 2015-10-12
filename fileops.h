/*
 * fileops.h
 * Copyright 2011 Bob Parker <rlp1938@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
*/

#ifndef _FILEOPS_H
#define _FILEOPS_H
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdarg.h>
#include <getopt.h>
#include <ctype.h>
#include <limits.h>
#include <linux/limits.h>
#include <libgen.h>

#define _GNU_SOURCE 1

typedef struct fdata {
	char *from;
	char *to;
}fdata;

fdata readfile(const char *filename, off_t extra, int fatal);
void writefile(const char *to_write, const char *from, const char *to,
				const char *mode);
FILE *dofopen(const char *fn, const char *fmode);
int direxists(const char *path);
fdata mem2str(char *pfrom, char *pto);
int fileexists(const char *path);
void doread(int fd, size_t bcount, char *result);
void dowrite(int fd, char *writebuf);
int getans(const char *prompt, const char *choices);

#endif
