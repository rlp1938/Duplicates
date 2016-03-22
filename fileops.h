/*
 * fileops.h
 * Copyright 2016 Bob Parker <rlp1938@gmail.com>
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
#include <dirent.h>
#include <fcntl.h>
#include <stdarg.h>
#include <getopt.h>
#include <ctype.h>
#include <limits.h>
#include <linux/limits.h>
#include <libgen.h>
#include <errno.h>

#define _GNU_SOURCE 1

typedef struct fdata {
	char *from;
	char *to;
}fdata;

FILE *dofopen(const char *fn, const char *fmode);
void dofclose(FILE *fp);
// man (3) dofopen, dfclose

size_t dofread(const char *fn, void *fro, size_t nbytes, FILE *fpi);
void dofwrite(const char *fn, const void *fro, size_t nbytes,
				FILE *fpi);
// man (3) dofread, dofwrite.

int fileexists(const char *path);
int direxists(const char *path);
// man (3) fileexists, man () direxists

int doopen(const char *fn, const char *mode);
void doclose(int fd);
int is_in_list(const char *what, const char **list);

void doread(int fd, size_t bcount, char *result);
void dowrite(int fd, char *writebuf);
int dostat(const char *fn, struct stat *sb, int fatal);
void do_mkdir(const char *head_dir, const char *newdir);
fdata dorealloc(fdata indata, int change);
void *docalloc(size_t nmemb, size_t size, const char *func);

fdata readtextfile(const char *filename, off_t extra, int fatal);
fdata readfile(const char *filename, off_t extra, int fatal);
fdata readpseudofile(const char *path, off_t extra);
void writefile(const char *to_write, const char *from, const char *to,
				const char *mode);
size_t count_file_bytes(const char *path);

fdata mem2str(char *pfrom, char *pto);
int getans(const char *prompt, const char *choices);
int isrunning(char **proglist);
char *gettmpfn(void);
char **readcfg(const char *relpath);
char *get_realpath_home(const char *relpath);
void comment_text_to_space(char *from, const char *to);
int count_cfg_data_lines(char *from, char *to);
void set_cfg_lines(char **lines, int numlines, char *from, char *to);
int get_number_from_sysfile(const char *path);

#endif
