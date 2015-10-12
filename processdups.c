/*
 * processdups.c
 *
 * Copyright 2015 Bob Parker <rlp1938@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <ctype.h>
#include "config.h"

char *pathend = "!*END*!";

struct filedata {
    char *from; // start of file content
    char *to;   // last byte of data + 1
};

struct hashrecord {
	char thesum[33];
	ino_t ino;	// 64 bit unsigned.
	dev_t dev;	// 64 bit unsigned.
	char ftyp;
	char path[PATH_MAX];
	char *line;
};

static void help_print(int forced);
static FILE *dofopen(const char *path, const char *mode);
static struct hashrecord parse_line(char *line);
static struct filedata *readfile(char *path, int fatal);
static struct filedata *mkstructdata(char *from, char *to);
static int get_user_input(struct hashrecord *hrlist, const int hrmax,
							const char *currenthash);
static void dounlink(const char *path, int fatal);
static void dolink(const char *oldpath, const char *newpath, int fatal);
static void dosymlink(const char *oldpath, const char *newpath,
						int fatal);

static char *helptext =
  "\n\tUsage: processdups [option] duplicates-list\n \n\tOptions:\n"
  "\t-h outputs this help message.\n"
  "\t-l causes deleted paths to be re-instated as hard links.\n"
  "\tNB this an interactive program which tells you what it wants\n"
  "\t\ton the way.\n"
  ;
static int linkthem;


int main(int argc, char **argv)
{
	int opt;
	struct stat sb;
	struct filedata *fdat;
	char *dupsfile;
	char *from, *to, *line1, *writefrom;
	struct hashrecord hrlist[30];
	int hrindex, hrtotal;
	char currenthash[33];
	char *line2;
	struct hashrecord hr;


	// set defaults
	linkthem = 0;

	while((opt = getopt(argc, argv, ":hl")) != -1) {
		switch(opt){
		case 'h':
			help_print(0);
		break;
		case 'l':
			linkthem = 1;
		break;
		case ':':
			fprintf(stderr, "Option %c requires an argument\n",optopt);
			help_print(1);
		break;
		case '?':
			fprintf(stderr, "Illegal option: %c\n",optopt);
			help_print(1);
		break;
		} //switch()
	}//while()
	// now process the non-option arguments

	// 1.Check that argv[???] exists.
	if (!(argv[optind])) {
		fprintf(stderr, "No duplicates file provided\n");
		help_print(1);
	}

	// 2. Check that the file exists.
	if ((stat(argv[optind], &sb)) == -1){
		perror(argv[optind]);
		help_print(EXIT_FAILURE);
	}

	// 3. Check that this is a file
	if (!(S_ISREG(sb.st_mode))) {
		fprintf(stderr, "Not a file: %s\n", argv[optind]);
		help_print(EXIT_FAILURE);
	}

	// read file into memory
	dupsfile = argv[optind];
	fdat = readfile(dupsfile, 1);
	from = fdat->from;
	to = fdat->to;
	writefrom = from;
	// turn the entire amorphous mess into an array of C strings
	{
		char *cp = from;
		while (cp < to) {
			if (*cp == '\n') *cp = '\0';
			cp++;
		}
	}

	line1 = writefrom;
	// give user chance to quit here.
	hrindex = hrtotal = 0;
	hr = parse_line(line1);
	strcpy(currenthash, hr.thesum);
	hrlist[hrindex] = hr;
	hrindex++;
	line2 = line1 + strlen(line1) +1;	// at beginning next line.
	while (line2 < to) {
		hr = parse_line(line2);
		if (strcmp(currenthash, hr.thesum) != 0 ) {
			// re-init the process
			if (get_user_input(hrlist, hrindex, currenthash) == -1)
				goto done;
			hrindex = 0;
			strcpy(currenthash, hr.thesum);
			writefrom = line2;
		}
		hrlist[hrindex] = hr;
		hrindex++;
		line2 += strlen(line2) + 1;
	} // while(line2 ...)
	if (hrindex) {
		get_user_input(hrlist, hrindex, currenthash);
	}

done:

	if (writefrom != from) {
		FILE *fpo = dofopen(dupsfile, "w");
		line1 = writefrom;
		while(line1 < to) {
			fprintf(fpo, "%s\n", line1);
			line1 += strlen(line1);
			line1++;	// looking at next line
		}
		fclose(fpo);
	}
	return 0;
}

void help_print(int forced){
    fputs(helptext, stderr);
    exit(forced);
} // help_print()

struct filedata *mkstructdata(char *from, char *to)
{
	// create and initialise this struct in 1 place only
	struct filedata *dp = malloc(sizeof(struct filedata));
	if (!(dp)){
		perror("malloc failure making 'struct filedata'");
		exit(EXIT_FAILURE);
	}
	dp->from = from;
	dp->to = to;
	return dp;
} // mkstructdata()

struct filedata *readfile(char *path, int fatal)
{
    /*
     * open a file and read contents into memory.
     * if fatal is 0 and the file does not exist return NULL
     * but if it's non-zero abort and do that for all other
     * errors.
    */
    struct stat sb;
    FILE *fpi;
    off_t bytes;
    char *from, *to;

    if ((stat(path, &sb)== -1)){
        if (!(fatal)) return NULL;
			perror(path);
			exit(EXIT_FAILURE);
    }
    if (!(S_ISREG(sb.st_mode) || S_ISLNK(sb.st_mode))) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    fpi = fopen(path, "r");
    if (!(fpi)) {
		perror(path);
        exit(EXIT_FAILURE);
    }
    from = malloc(sb.st_size);
    if(!(from)) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    bytes = fread(from, 1, sb.st_size, fpi);
    if (bytes != sb.st_size){
        perror(path);
        exit(EXIT_FAILURE);
    }
    to = from + bytes;
    fclose(fpi);
    return mkstructdata(from, to);
} // readfile()

struct hashrecord parse_line(char *line)
{
	// <MD5> <inode> <device> <path><pathend> <f|s>
	char *cp, *eol;
	struct hashrecord hr;
	char buf[PATH_MAX];

	strcpy(buf, line);	// line is a null terminated C string
	hr.ftyp = buf[strlen(buf) -1];
	cp = buf;
	strncpy(hr.thesum, cp, 32);
	hr.thesum[32] = '\0';
	cp += 33;	// looking at inode
	hr.ino = strtoul(cp, NULL, 16);
	cp += 17;	// past inode, looking at device
	hr.dev = strtoul(cp, NULL, 16);
	cp += 17;	// past device, looking at path.
	eol = strstr(cp, pathend);
	*eol = '\0';
	memset(hr.path, 0, PATH_MAX);
	strcpy(hr.path, cp);
	hr.line = line;	// for gdb maybe
	return hr;
} // parse_line()

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

static int get_user_input(struct hashrecord *hrlist, const int hrmax,
							const char *currenthash)
{
	static char action = 'd';
	char *cp;
	int i, choice;
	char ans[10], ians[10];
	int hrindex;
	dev_t masterdev;
	char *fmt1 = "Action to be taken:\n"
	"Delete all but selected path (d)\n"
	"Delete all and hard link them to selected path (l)\n"
	"Delete the entire list (X)\n"
	"Ignore this list (i)\n"
	"Quit this process (q)\n"
	"Default is < %c >";

	fprintf(stdout, "\tMD5SUM: %s\n", currenthash);
	for (hrindex=0; hrindex < hrmax; hrindex++){
		fprintf(stdout,"%d %s %lu %c\n", hrindex, hrlist[hrindex].path,
				hrlist[hrindex].ino, hrlist[hrindex].ftyp);
	}
	fprintf(stdout, fmt1, action);
	if (!(fgets(ans, 10, stdin))) {
		perror("fgets");
	}
	cp = strchr(ans, '\n');
	if (cp) *cp = '\0';
	if (strlen(ans)) action = ans[0];
	while (!(strchr("dlXiq", action))) {
		fputs("invalid choice: ", stdout);
		if (!(fgets(ans, 10, stdin))) {
			perror("fgets");
		}
		cp = strchr(ans, '\n');
		if (cp) *cp = '\0';
		if (strlen(ans)) action = ans[0];
	}
	switch(action) {
		case 'd':
		choice = -9999;
		while( choice < 0 || choice >= hrmax){
			fprintf(stdout, "Path to preserve: %d..%d", 0, hrmax - 1);
			if (!(fgets(ians, 10, stdin))) {
				perror("fgets");
			}
			cp = strchr(ians, '\n');
			*cp = '\0';
			choice = strtol(ians, NULL, 10);
		}
		for (i=0; i<hrmax; i++) {
			if (i != choice) dounlink(hrlist[i].path, 0);
		}
		break;
		case 'l':
		choice = -9999;
		while( choice < 0 || choice >= hrmax){
			fprintf(stdout, "Path to link to the rest: %d..%d",
							0, hrmax - 1);
			if(!(fgets(ians, 10, stdin)))
			cp = strchr(ians, '\n');
			*cp = '\0';
			choice = strtol(ians, NULL, 10);
			masterdev = hrlist[choice].dev;
		}
		for (i=0; i<hrmax; i++) {
			if (i != choice) dounlink(hrlist[i].path, 0);
		}
		sync();
		for (i=0; i<hrmax; i++) {
			if (i != choice) {
				if (masterdev == hrlist[i].dev) {
					dolink(hrlist[choice].path ,hrlist[i].path, 0);
				} else {
					dosymlink(hrlist[choice].path ,hrlist[i].path, 0);
				}
			}
		} // for(i...)
		break;
		case 'X':
		for (i=0; i<hrmax; i++) {
			dounlink(hrlist[i].path, 0);
		}
		break;
		case 'i':
		return 0;
		break;
		case 'q':
		return -1;
		break;
	}
	return 0;
}  // get_user_input()

static void dounlink(const char *path, int fatal)
{
	// unlink() with error handling
	if (unlink(path) == -1) {
		perror(path);
		if (fatal) EXIT_FAILURE;
	}
}

static void dolink(const char *oldpath, const char *newpath, int fatal)
{
	// link() with error handling
	if (link(oldpath, newpath) == -1) {
		perror(newpath);
		if (fatal) EXIT_FAILURE;
	}

} // dolink()

static void dosymlink(const char *oldpath, const char *newpath,
						int fatal)
{
	// symlink() with error handling
	if (symlink(oldpath, newpath) == -1) {
		perror(newpath);
		if (fatal) EXIT_FAILURE;
	}

} // dosymlink()
