/* fileops.c
 *
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

#include "fileops.h"

fdata readtextfile(const char *filename, off_t extra, int fatal)
{	// checks that file terminates with '\n' and if not appends it.
	fdata txtdat = readfile(filename, extra + 1, fatal);
	char *lastbyte = txtdat.to - extra - 1;
	if (*lastbyte == '\0') *lastbyte = '\n';
	return txtdat;
} // readtextfile()

fdata readfile(const char *filename, off_t extra, int fatal)
{
    FILE *fpi;
    size_t bytesread;
    fdata data = {0};
    struct stat sb;
    if (dostat(filename, &sb, fatal) == -1)	return data;
    fpi = dofopen(filename, "r");
    data.from = docalloc(sb.st_size + extra, sizeof(char), "readfile");
	bytesread = dofread(filename, data.from, sb.st_size, fpi);
	dofclose (fpi);
    data.to = data.from + bytesread + extra;
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
	int ofd;
	if (strcmp("-", to_write) == 0) {
		ofd = 1;	// stdout
	} else {
		ofd = doopen(to_write, mode);
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

int isrunning(char **proglist){
	/* Iterate over /proc and see if anything in proglist is running.
	 * proglist must be a NULL terminated list of program names.
	*/
	int result = 0;
	DIR *prdir = opendir("/proc");
	struct dirent *ditem;
	char buf[NAME_MAX];
	while ((ditem = readdir(prdir))) {
		if (ditem->d_type != DT_DIR) continue;
		int pidi = strtol(ditem->d_name, NULL, 10);
		// next line takes care of ".", ".." and all alpha named files.
		if (pidi < 100) continue;	// and also init() time pids
		sprintf(buf, "/proc/%s/comm", ditem->d_name);
		/* NB readfile() is useless here because stat() returns crap
		 * for these pseudo files
		*/
		FILE *fpi = fopen(buf, "r");
		if (fpi) {	// it maybe dissappeared already
			char readbuf[NAME_MAX];
			int res = fscanf(fpi, "%s", readbuf);
			res++;	// stop gcc bitching
			int i = 0;
			while(proglist[i]) {
				if (strcmp(proglist[i], readbuf) == 0) result = 1;
				i++;
			}
			dofclose(fpi);
		} // while(proglist[i])
		if (result) break;
	} // while(readdir())
	closedir(prdir);
	return result;
} // isrunning()

char *gettmpfn(const char *aname)
{
	char tfn[NAME_MAX];
	char *user = getenv("LOGNAME");
	if (!user) {
		user = getenv("USER");
		if (!user) {
			fputs("Unable to get user or login name.\n", stderr);
			exit(EXIT_FAILURE);
		}
	}
	pid_t myid = getpid();
	sprintf(tfn, "/tmp/%s%i%s", user, myid, aname);
	return dostrdup(tfn);
} // gettmpfn()

char **readcfg(const char *relpath)
{	// reads a config file; somevar=somevalue + comments '#'
	char *rpath = get_realpath_home(relpath);
	fdata rcdat = readtextfile(rpath, 0, 1);
	comment_text_to_space(rcdat.from, rcdat.to);
	int lcount = count_cfg_data_lines(rcdat.from, rcdat.to);
	char **retval = docalloc(lcount+1, sizeof(char*), "readcfg");
	set_cfg_lines(retval, lcount, rcdat.from, rcdat.to);
	free(rcdat.from);
	return retval;
} // readcfg()

/* I can not use readfile() on the pseudo files in /proc and /sys
because stat() gives them a size of 0.
*/

fdata readpseudofile(const char *path, off_t extra)
{
	fdata mydata;
	size_t fsize = count_file_bytes(path);
	FILE *fpi = dofopen(path, "r");
	mydata.from = docalloc(fsize + extra, 1, "readpseudofile()");
	dofread(path, mydata.from, fsize, fpi);
	mydata.to = mydata.from + (fsize + extra);
	dofclose(fpi);
	return mydata;
} // readpseudofile()

int dostat(const char *fn, struct stat *sb, int fatal)
{
	int res = stat(fn, sb);
	if (res == -1) {
		if (fatal){
            perror(fn);
            exit(EXIT_FAILURE);
		} else {
			return -1;
		}
	} // if(res...)
	return 0;
} // dostat()

void *docalloc(size_t nmemb, size_t size, const char *func)
{
	char *from = calloc(nmemb, size);
	if (!from) {
		fprintf(stderr, "Failed to get memory in %s\n", func);
		exit(EXIT_FAILURE);
	}
	return from;
} // docalloc()

size_t dofread(const char *fn, void *fro, size_t nbytes, FILE *fpi)
{	// fread() with error handling
	size_t wasread = fread(fro, 1, nbytes, fpi);
	if (wasread != nbytes) {
		fprintf(stderr, "For file %s, expected to read %lu bytes"
		" but got %lu\n", fn, nbytes, wasread);
		exit(EXIT_FAILURE);
	}
	return wasread;
} // dofread()

char *get_realpath_home(const char *relpath)
{
	static char rpath[PATH_MAX];
	char *home=getenv("HOME");
	if (!home) {
		perror("HOME");
		exit(EXIT_FAILURE);
	}
	strcpy(rpath, home);
	if (rpath[strlen(rpath)-1] != '/') {
		strcat(rpath, "/");
	}
	strcat(rpath, relpath);
	return rpath;
} // get_realpath_home()

void comment_text_to_space(char *from, const char *to)
{	// comments begin with '#' to end of line
	char *cp = from;
	int incomment = 0;
	while (cp < to) {
		switch (*cp) {
			case '#':
				incomment = 1;
				*cp = ' ';
				break;
			case '\n':
				incomment = 0;
				break;
			default:
				if (incomment) *cp = ' ';
				break;
		} // switch()
		cp++;
	} // while()
} // comment_text_to_space()

int count_cfg_data_lines(char *from, char *to)
{	// requires all commented text be converted to ' ' first.
	char *dl = from;
	int count = 0;
	char *eol;
	while (dl < to) {
		while(isspace(*dl)) dl++;
		count++;
		eol = memchr(dl, '\n', to - dl);
		if (!eol) eol = to;	// file without terminating '\n'
		dl = eol;
	}
	return count - 1;
} // count_cfg_data_lines()

void set_cfg_lines(char **lines, int numlines, char *from, char *to)
{	// lines must have been preconfigured to suit the data
	char *bol = from;
	int idx = 0;
	while (idx < numlines) {
		while (isspace(*bol)) bol++;
		char *eol = memchr(bol, '\n', to - bol);
		char *line_end = eol;
		while (isspace(*eol)) {
			*eol = '\0';
			eol--;
		}
		size_t len = strlen(bol);
		const size_t minlen = 3;	// minimum valid is eg x=3
		if (len < minlen || len > NAME_MAX - 1) {
			fprintf(stderr, "Invalid string length in config file"
			" processing %lu", len);
			exit(EXIT_FAILURE);
		}
		lines[idx] = strdup(bol);
		idx++;
		bol = line_end+1;
	} // while()
} // set_cfg_lines()

size_t count_file_bytes(const char *path)
{
	size_t count = 0;
	FILE *fpi = dofopen(path, "r");
	int ch;
	errno = 0;
	while ((ch = fgetc(fpi)) != EOF) {
		count++;
	}
	if (errno) {
		perror(path);
		exit(EXIT_FAILURE);
	}
	dofclose(fpi);
	sync();	// required??? I don't know.
	return count;
} // count_file_bytes()


void do_mkdir(const char *head_dir, const char *newdir)
{
	struct stat sb;
	dostat(head_dir, &sb, 1);	// die if head_dir is not.
	mode_t m = sb.st_mode;
	char buf[PATH_MAX];
	if (strlen(head_dir) + strlen(newdir) > PATH_MAX - 2) {
		fputs("Insane lengths input for head_dir and newdir\n", stderr);
		exit(EXIT_FAILURE);
	}
	sprintf(buf, "%s/%s", head_dir, newdir);
	if (mkdir(buf, m) == -1) {
		fprintf(stderr, "Failed to create dir: %s\n", buf);
		perror(buf);
		exit(EXIT_FAILURE);
	}
} // do_mkdir()

fdata dorealloc(fdata indata, int change)
{	// zeroes the new allocated area also.
	size_t insize = indata.to - indata.from;
	fdata outdata;
	outdata.from = realloc(indata.from, insize + change);
	if (!outdata.from) {
		perror("dorealloc");
		exit(EXIT_FAILURE);
	}
	if (change > 0) memset(outdata.from + insize, '\0', change);
	outdata.to = outdata.from + insize + change;
	return outdata;	// I don't care if the new pointer has been moved.
} // dorealloc()

int get_number_from_sysfile(const char *path)
{	// must be a string of 1 or more decimal digits else error
	fdata thedat = readpseudofile(path, 0);
	char *endptr;
	int res = strtol(thedat.from, &endptr, 10);
	int valid = (*thedat.from != '\0' &&
			(*endptr == '\0' || *endptr == '\n'));
	if (!valid) {
		fputs("Invalid data in sys file\n", stderr);
		fwrite(thedat.from, 1, thedat.to - thedat.from, stderr);
		exit(EXIT_FAILURE);
	}
	free(thedat.from);
	return res;
} // get_number_from_sysfile()

void dofclose(FILE *fp)
{	// fclose() and aborts on error.
	int res = fclose(fp);
	if (res == EOF) {
		perror("fclose()");
		exit(EXIT_FAILURE);
	}
} // dofclose()

void dofwrite(const char *fn, const void *fro, size_t nbytes,
				FILE *fpi)
{	// fwrite + abort on error
	size_t waswritten = fwrite(fro, 1, nbytes, fpi);
	if (waswritten != nbytes) {
		fprintf(stderr, "For file %s, expected to write %lu bytes"
		" but wrote %lu\n", fn, nbytes, waswritten);
		exit(EXIT_FAILURE);
	}
} // dofwrite()

void doclose(int fd)
{	// close() but aborts on error
	if (close(fd) == -1) {
		perror("close()");
		exit(EXIT_FAILURE);
	}
} // doclose()

int is_in_list(const char *what, const char **list)
{	// returns -1 if what is not in list
	int lidx = 0;
	while (list[lidx]) {
		if (strcmp(what, list[lidx]) == 0) {
			return lidx;
		}
		lidx++;
	}
	return -1;
} // is_in_list()

int doopen(const char *fn, const char *mode)
{	// open() with error handling.
	mode_t oflags = S_IRUSR|S_IWUSR;
	const char *opentypes[7] = {"w", "r", "a", "w+", "r+", "a+", NULL };
	int omt = is_in_list(mode, opentypes);
	if (omt == -1) {
		fprintf(stderr, "Invalid open mode, you had %s.\n", mode);
		exit(EXIT_FAILURE);
	}	//                      "w"               "r"
	mode_t omts[] = {O_WRONLY|O_CREAT|O_TRUNC, O_RDONLY,
	/*      "a"							"w+"			"r+"	*/
	O_CREAT|O_APPEND|O_WRONLY, O_RDWR|O_CREAT|O_TRUNC, O_RDWR,
		/* 	"a+"			*/
	O_CREAT|O_APPEND|O_RDWR	};
	int ofd = open(fn, omts[omt], oflags);
		if (ofd == -1) {
			fprintf(stderr, "Open failure on %s\n", fn);
			perror(fn);
			exit(EXIT_FAILURE);
		}
	return ofd;
} // doopen()

char *dostrdup(const char *s)
{
	/*
	 * strdup() with in built error handling
	*/
	char *cp = strdup(s);
	if(!(cp)) {
		perror(s);
		exit(EXIT_FAILURE);
	}
	return cp;
} // dostrdup()

char *getconfigpath(const char *pname)
{
	char userhome[PATH_MAX];
	char *enval = getenv("USER");
	sprintf(userhome, "/home/%s/.config/%s/",
			enval, pname);
	return dostrdup(userhome);
} // getconfigpath()

char *getconfigfile(const char *path, const char *fname)
{
	char *cfgpath = getconfigpath(path);
	char result[PATH_MAX];
	sprintf(result, "%s%s", cfgpath, fname);
	free(cfgpath);
	return dostrdup(result);
} // getconfigfile()
