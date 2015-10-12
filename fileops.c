/* fileops.c
 *
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

#include "fileops.h"

fdata readfile(const char *filename, off_t extra, int fatal)
{
    FILE *fpi;
    off_t bytesread;
    char *from, *to;
    fdata data;
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
        perror("malloc failure in readfile()");
        exit(EXIT_FAILURE);
    }

	bytesread = fread(from, 1, sb.st_size, fpi);
	fclose (fpi);
	if (bytesread != sb.st_size) {
		fprintf(stderr, "Size error: expected %lu, got %lu\n",
				sb.st_size, bytesread);
		perror(filename);
		exit(EXIT_FAILURE);
	}
    to = from + bytesread + extra;
    // zero the extra space
    memset(from+bytesread, 0, to-(from+bytesread));
    data.from = from;
    data.to = to;
    return data;

} // readfile()

FILE *dofopen(const char *fn, const char *fmode)
{	// fopen() with error handling.
	FILE *fpx = fopen(fn, fmode);
	if (!fpx) {
		perror(fn);
		exit(EXIT_FAILURE);
	}
	return fpx;
} // dofopen()

void writefile(const char *to_write, const char *from, const char *to,
				const char *mode)
{
/*	FILE *fpo;
	if (strcmp("-", to_write) == 0) {
		fpo = stdout;
	} else {
		fpo = dofopen(to_write, mode);
	}
	size_t numbytes = to - from;
	size_t written = fwrite(from, 1, numbytes, fpo);
	if (numbytes != written) {
		fprintf(stderr, "Expected to write %ld bytes but wrote %ld\n",
					numbytes, written);
		perror(to_write);
		exit(EXIT_FAILURE);
	}
	fclose(fpo);
*/
	/* rewritten using syscalls because the library calls seem to be
	 * completely fucked up. */
	mode_t opmode;
	int oflags = S_IRUSR | S_IWUSR;
	if (strcmp("w", mode) == 0) {
		opmode = O_CREAT | O_TRUNC | O_WRONLY;
	} else if (strcmp("a", mode) == 0) {
		opmode = O_APPEND | O_WRONLY;
	} else {
		fprintf(stderr, "Open mode must be 'w|a', you had %s.\n", mode);
		exit(EXIT_FAILURE);
	}
	int ofd;
	if (strcmp("-", to_write) == 0) {
		ofd = 1;	// stdout
	} else {
		ofd = open(to_write, opmode, oflags);
		if (ofd == -1) {
			fprintf(stderr, "Open failure on %s\n", to_write);
			perror(to_write);
			exit(EXIT_FAILURE);
		}
	}
	ssize_t towrite = to - from;
	ssize_t written = write(ofd, from, towrite);
	if (written != towrite) {
		fprintf(stderr, "Expected to write %li bytes but %li written\n",
				towrite, written);
		perror(to_write);
		exit(EXIT_FAILURE);
	}
	if (ofd != 1) close(ofd);
} // writefile()

int direxists(const char *path)
{	// crude check for existence of a dir.
	struct stat sb;
	if (stat(path, &sb) == -1) return -1;
	if (S_ISDIR(sb.st_mode)) return 0;
	return -1;
} //direxists()

int fileexists(const char *path)
{	// crude check for existence of a file.
	struct stat sb;
	if (stat(path, &sb) == -1) return -1;
	if (S_ISREG(sb.st_mode)) return 0;
	return -1;
} //fileexists()

fdata mem2str(char *pfrom, char *pto)
{
	/*
	 * Checks that last char in memory is '\n', and if not reallocs
	 * the memory area by 1 byte extra and copies '\n' to that byte.
	 * Then replaces all '\n' with '\0' and returns the altered data.
	 *
	 * Usage:
	 * fdata mydat = readfile("file", 0, 1);
	 * mydat = mem2str(mydat.from, mydat.to);
	 * ...
	*/
	char *from = pfrom;
	char *to = pto;
	// check last char is '\n'
	if (*(to - 1) != '\n') {	// grab 1 more byte
		char *old = from;
		from = realloc(from, to - from + 1);
		// been moved?
		if (old != from) {
			to = from + (to - old);	// keep old offset
		}
		*to = '\n';
		to++;	// final offset
	}
	char *cp = from;
	while (cp < to) {
		char *eol = memchr(cp, '\n', to - cp);
		if (eol) {
			*eol = '\0';
			cp = eol;
		}
		cp++;
	}
	fdata retdat;
	retdat.from = from;
	retdat.to = to;
	return retdat;
} // mem2str()

void doread(int fd, size_t bcount, char *result)
{
	char buf[PATH_MAX];
	if (bcount > PATH_MAX) {
		fprintf(stderr, "Requested size %li too big.\n", bcount);
		exit(EXIT_FAILURE);
	}
	ssize_t res = read(fd, buf, bcount);
	if (res == -1) {
		perror(buf);
		exit(EXIT_FAILURE);
	}

	strncpy(result, buf, res);
	result[res] = '\0';
} // doread()

void dowrite(int fd, char *writebuf)
{
	ssize_t towrite = strlen(writebuf);
	ssize_t written = write(fd, writebuf, towrite);
	if (written != towrite) {
		fprintf(stderr, "Expected to write %li bytes but wrote %li\n",
				towrite, written);
		exit(EXIT_FAILURE);
	}
} // dowrite()

int getans(const char *prompt, const char *choices)
{
	/* Prompt the user with prompt then loop showing choices until
	 * the user enters something contained in choices.
	 * Alphabetic choices like "Yn" will be case insensitive.
	*/

	char c;
	char buf[10];
	char promptbuf[NAME_MAX];
	char ucchoices[NAME_MAX];
	memset(ucchoices, 0, NAME_MAX);
	size_t l = strlen(choices);
	size_t i;
	for (i = 0; i < l; i++) {
		ucchoices[i] = choices[i];
	}
	fputs(prompt, stdout);
	sprintf(promptbuf, "Enter one of [%s] :", choices);
	while (1) {
		fputs(promptbuf, stdout);
		char *cp = fgets(buf, 10, stdin);
		if (!cp) {
			perror("fgets");
			exit(EXIT_FAILURE);
		}

		c = toupper(buf[0]);
		if (strchr(choices, c)) break;
	}
	return c;
} // getans()
