
/*      duplicates.c
 *
 *	Copyright 2011 Bob Parker <rlp1938@gmail.com>
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


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include "md5.h"
#include "config.h"
#include "fileutil.h"
#include <ctype.h>

static int filecount;

static char *prefix;

static const char *pathend = "!*END*!";	// Anyone who puts shit like
										// that in a filename deserves
										// what happens.

struct filedata {
    char *from; // start of file content
    char *to;   // last byte of data + 1
};

struct hashrecord {
	char thesum[33];
	ino_t ino;
	char ftyp;
	char path[PATH_MAX];
};

static void help_print(int forced);
static char *dostrdup(const char *s);
static void recursedir(char *headdir, FILE *fpo, char **vlist);
static char *domd5sum(const char *pathname);
static struct hashrecord parse_line(char *line);
static void clipeol(char *line);
static char *thepathname(char *line);
static char *getcluster(char *path, int depth);
static void firstrun(void);
static char *getconfigpath(void);
static char **mem2strlist(char *from, char *to);
static void getprefix_from_user(char *sharefile);



static const char *helptext = "\n\tUsage: duplicates [option] dir_to_search\n"
  "\n\tOptions:\n"
  "\t-h outputs this help message.\n"
  "\t-d debug mode, don't discard temporary work files.\n"
  "\t-v each invocation increases verbosity, default is 0\n"
  "\t\t0, emit no progress information.\n"
  "\t\t1, emit information of each new section started.\n"
  "\t\t2, emit pathname of each 100th file during Md5sum generation.\n"
  "\t\t3, the same for every tenth file.\n"
  "\t\t4+, the same for every file.\n"
  "\tYou probably do not want to use this option when you are redirecting\n"
  "\tstderr to a file. Eg 2> errors_file\n"
  ;


static char *eol; // string in case I ever want to do Microsoft
static int clusterdepth;


int main(int argc, char **argv)
{
	int opt, result, verbosity, delworks, vlindex;
	struct stat sb;
	struct fdata fdat;
	char *from, *to;
	char *workfile0;
	char *workfile1;
	char *workfile2;
	char *workfile3;
	char *workfile4;
	char *workfile5;
	char *workfile6;
	char *workfile7;
	char *workfile8;
	char *workfile9;

	char command[FILENAME_MAX];
	FILE *fpo, *fpi, *fpofinal;
	char *line1, *line2, *line3;
	char buf1[PATH_MAX*2], buf2[PATH_MAX], clustername[PATH_MAX];
	struct hashrecord hr1, hr2;
	char *topdir;
	char *fgetsresult;
	char *efl;	// path to excludes list
	char **vlist;

	// set default values
	verbosity = 0;
	filecount = 0;
	fpofinal = stdout;
	clusterdepth = 3;
	delworks = 1;	// delete workfiles is the default
	prefix = dostrdup("/usr/local/");

	eol = "\n";	// string in case I ever want to do Microsoft

	while((opt = getopt(argc, argv, ":hvdc:")) != -1) {
		switch(opt){
		case 'h':
			help_print(0);
		break;
		case 'd':
			delworks = 0;	// don't trash the workfiles at end
		break;
		case 'c':
			clusterdepth = atoi(optarg);
		break;
		case 'v':
			verbosity++;	// 4 levels of verbosity, 0-3. 0 no progress
							// report, 1 print every 100th pathname,
							// 2 every tenth, 3 (and above) print every
							// pathname.
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
		fprintf(stderr, "No directory provided\n");
		help_print(1);
	}
	// generate my workfile names
	sprintf(command, "/tmp/%sduplicates0", getenv("USER"));
	workfile0 = dostrdup(command);
	sprintf(command, "/tmp/%sduplicates1", getenv("USER"));
	workfile1 = dostrdup(command);
	sprintf(command, "/tmp/%sduplicates2", getenv("USER"));
	workfile2 = dostrdup(command);
	sprintf(command, "/tmp/%sduplicates3", getenv("USER"));
	workfile3 = dostrdup(command);
	sprintf(command, "/tmp/%sduplicates4", getenv("USER"));
	workfile4 = dostrdup(command);
	sprintf(command, "/tmp/%sduplicates5", getenv("USER"));
	workfile5 = dostrdup(command);
	sprintf(command, "/tmp/%sduplicates6", getenv("USER"));
	workfile6 = dostrdup(command);
	sprintf(command, "/tmp/%sduplicates7", getenv("USER"));
	workfile7 = dostrdup(command);
	sprintf(command, "/tmp/%sduplicates8", getenv("USER"));
	workfile8 = dostrdup(command);
	sprintf(command, "/tmp/%sduplicates9", getenv("USER"));
	workfile9 = dostrdup(command);

	// get my exclusions file
	efl = getconfigpath();
	fdat = readfile(efl, 1, 0);
	if (!(fdat.from)) {
		firstrun();
		fdat = readfile(efl, 1, 1); // fatal if I dont have it now
	}
	// turn the exclusions file into an array of strings
	vlist = mem2strlist(fdat.from, fdat.to);

	// open the output workfile
	fpo = dofopen(workfile0, "w");
	// Step 0 - list the files
	if (verbosity){
		fputs("Generating list of files and symlinks\n",stderr);
	}

	while (argv[optind]) {
		// Check that the dir exists.
		if ((stat(argv[optind], &sb)) == -1){
			perror(argv[optind]);
			help_print(EXIT_FAILURE);
		}

		// Check that this is a dir
		if (!(S_ISDIR(sb.st_mode))) {
			fprintf(stderr, "Not a directory: %s\n", argv[optind]);
			help_print(EXIT_FAILURE);
		}
		topdir = argv[optind];
		{
			// get rid of trailing '/'
			int len = strlen(topdir);
			if (topdir[len-1] == '/') topdir[len-1] = '\0';
		}
		recursedir(topdir, fpo, vlist);
		optind++;
	} // while(argv[optind])
	fclose(fpo);

	// free the vlist items
	vlindex = 0;
	while(vlist[vlindex]) {
		free(vlist[vlindex]);
		vlindex++;
	}
	free(vlist);

	// Now sort them
	if (verbosity){
		fputs("Sorting list of files\n", stderr);
	}
	if (setenv("LC_ALL", "C", 1) == -1){	// sort bitwise L-R
		perror("LC_ALL=C");
		exit(EXIT_FAILURE);
	}
	sprintf(command, "sort %s > %s",
						workfile0, workfile1);
	result = system(command);
	if(result == -1){
		perror(command);
		exit(EXIT_FAILURE);
	}

	// look through the sorted workfile and discard those that have
	// unique sizes
	fdat = readfile(workfile1, 0, 1);
	from = fdat.from;
	to = fdat.to;
	fpo = dofopen(workfile2, "w");
	line1 = from;
	// replace '\n' with '\0'
	while(line1 < to) {
		line1 = memchr(line1, '\n', PATH_MAX);
		if (line1) *line1 = '\0';
		line1++;	// at next line
	}
	line1 = from;
	while(line1 < to) {
		unsigned long s1, s2;
		s1 = atol(line1);
		line2 = line1 + strlen(line1) +1;
		s2 = atol(line2);
		if (s1 == s2) {
			fprintf(fpo, "%s\n%s\n", line1, line2);
		}
		line1 = line2;	// this will create duplicated records
	} // while()
	fclose(fpo);
	free(from);
	// get rid of duplicated records.
	sprintf(command, "sort -u %s > %s",
						workfile2, workfile3);
	result = system(command);
	if(result == -1){
		perror(command);
		exit(EXIT_FAILURE);
	}

	// Traverse the list of files in workfile1 calculating MD5
	if (verbosity){
		fputs("Calculating md5sums\n", stderr);
	}
	fpi = dofopen(workfile3, "r");
	fpo = dofopen(workfile4, "w");
	while (fgets(buf1, PATH_MAX, fpi)){
		char *p, *cp;
		struct stat sb;
		clipeol(buf1);
		// find the path beginning
		p = buf1;
		while(isdigit(*p)) p++;
		while(*p == ' ') p++;
		cp = strstr(p, pathend);
		*cp = '\0';	// end of the file path
		cp += strlen(pathend) + 1;	// now looking at the file type, s|f
		if (stat(p, &sb) == -1){
			perror(p);	// just report it.
		} else {
			int report;
			char *thesum = domd5sum(p);
			fprintf(fpo, "%s %lu %c %s%s\n", thesum,
				sb.st_ino, *cp, p, pathend);
			// NB in the case of a symlink, sb.st_ino is the inode of
			// the target, not the inode of the link itself.
			filecount++;
			switch(verbosity) {
				case 0:	// do nothing
				case 1:
				report = 0;
				break;
				case 2:	// report every 100th
				report = (filecount % 100 == 0) ? 1:0;
				break;
				case 3:	// report every tenth
				report = (filecount % 10 == 0) ? 1:0;
				break;
				default:	// > 3 report every file
				report = 1;
				break;
			}
			if(report) {
				fprintf(stderr, "Processing: %s\n", buf1);
			}
		}
	}
	fclose(fpi);
	fclose(fpo);

	// Sort the resultant file of sums + paths
	if (verbosity){
		fputs("Sorting on md5sum, inode, path\n", stderr);
	}

	sprintf(command, "sort %s > %s",
						workfile4, workfile5);
	result = system(command);
	if(result == -1){
		perror(command);
		exit(EXIT_FAILURE);
	}

	// Process the sorted work file
	if (verbosity){
		fputs("Discarding pathnames of unique files\n", stderr);
	}
	fdat = readfile(workfile5, 0, 1);
	from = fdat.from;
	to = fdat.to;
	fpo = dofopen(workfile6, "w");
	// turn the mess into a list of C strings
	line1 = from;
	while (line1 < to) {
		line1 = memchr(line1, '\n', PATH_MAX);
		if (line1) *line1 = '\0';
		line1 += strlen(line1) + 1;
	}
	// output the duplicated lines
	line1 = from;
	hr1 = parse_line(line1);
	while (line1 < to){
		line2 = line1 + strlen(line1) + 1;	// looking at next line
		if (!( line2 < to )) break;
		hr2 = parse_line(line2);
		if ( strncmp(hr1.thesum, hr2.thesum, 32) == 0 &&
				hr1.ino != hr2.ino ) {	// this is a duplicated copy
			fprintf(fpo, "%s %lu %c %s%s\n", hr1.thesum, hr1.ino,
							hr1.ftyp, hr1.path, pathend);
			fprintf(fpo, "%s %lu %c %s%s\n", hr2.thesum, hr2.ino,
							hr2.ftyp, hr2.path, pathend);
		}
		line1 = line2;	// a treble will cause 4 output lines etc.
		hr1 = hr2;
	}
	fclose(fpo);

	// Step 4, use sort to get rid of duplicated lines
	if (verbosity){
		fputs("Discarding duplicated pathnames\n", stderr);
	}
	sprintf(command, "sort -u %s > %s",
						workfile6, workfile7);
	result = system(command);
	if(result == -1){
		perror(command);
		exit(EXIT_FAILURE);
	}
	// Group the duplicates by one pathname
	if (verbosity){
		fputs("Setting logical group names on every line\n",
				stderr);
	}
	fpi = dofopen(workfile7, "r");
	fpo = dofopen(workfile8, "w");
	line1 = buf1;
	line2 = buf2;
	fgetsresult = fgets(line1, PATH_MAX, fpi);
	if(!fgetsresult && ferror(fpi)) {
		perror("fgets()");
	}
	clipeol(line1);
	eol = strstr(line1, pathend);
	if (eol) *eol = '\0';
	line3 = dostrdup(thepathname(line1));
	strcpy(clustername, getcluster(line3, clusterdepth));

	while ((fgets(line2, PATH_MAX, fpi))) {
		char *cp;
		// I know that these are duplicated file names.
		clipeol(line2);
		eol = strstr(line2, pathend);
		if (eol) *eol = '\0';
		eol = strstr(line1, pathend);
		if (eol) *eol = '\0';
		fprintf(fpo, "%s %s%s\n", clustername, line1, pathend);
		while (strncmp(line1, line2, 32) == 0) {
			eol = strstr(line2, pathend);
			if(eol) *eol = '\0';
			fprintf(fpo, "%s %s%s\n", clustername, line2, pathend);
			cp = fgets(line2, PATH_MAX, fpi);
			if(!(cp)) break;
			clipeol(line2);
		}

		// line2 does not match on hash so it becomes line1
		line1 = line2;
		if(line3) free(line3);
		line3 = dostrdup(thepathname(line1));
		strcpy(clustername, getcluster(line3, clusterdepth));
	} // while()

	fclose(fpo);
	fclose(fpi);
	// Sort the mess by groupname
	if (verbosity){
		fputs("Sort on logical group names.\n",
				stderr);
	}
	sprintf(command, "sort %s > %s",
						workfile8, workfile9);
	result = system(command);
	if(result == -1){
		perror(command);
		exit(EXIT_FAILURE);
	}

	// Format the results for the user
	if (verbosity){
		fputs("Send the results to stdout.\n",
				stderr);
	}
	fpi = dofopen(workfile9, "r");
	// fpofinal has been dealt with at options processing time.
	while((fgets(buf1, 2 * PATH_MAX, fpi))) {
		line1 = strstr(buf1, pathend);	// sentinel
		line1 += strlen(pathend);
		while(*line1 == ' ') line1++;	// now looking at md5sum
		hr1 = parse_line(line1);
		fprintf(fpofinal, "%s%s %s %lu %c\n",
					hr1.path, pathend, hr1.thesum, hr1.ino, hr1.ftyp);
	} // while()

	sync();
	if (delworks){
		if (verbosity){
			fputs("Deleting workfiles.\n",
				stderr);
		}
		unlink(workfile0);
		unlink(workfile1);
		unlink(workfile2);
		unlink(workfile3);
		unlink(workfile4);
		unlink(workfile5);
		unlink(workfile6);
		unlink(workfile7);
		unlink(workfile8);
		unlink(workfile9);

	}

	return 0;
} // main()

void help_print(int forced){
    fputs(helptext, stderr);
    exit(forced);
} // help_print()


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

void recursedir(char *headdir, FILE *fpo, char **vlist)
{
	/* open the dir at headdir and process according to file type.
	*/
	DIR *dirp;
	struct dirent *de;

	dirp = opendir(headdir);
	if (!(dirp)) {
		perror(headdir);
		exit(EXIT_FAILURE);
	}
	while((de = readdir(dirp))) {
		int index, want;
		if (strcmp(de->d_name, "..") == 0) continue;
		if (strcmp(de->d_name, ".") == 0) continue;

		switch(de->d_type) {
			char newpath[FILENAME_MAX];
			struct stat sb;
			char *ftyp;
			// Nothing to do for these.
			case DT_BLK:
			case DT_CHR:
			case DT_FIFO:
			case DT_SOCK:
			continue;
			break;
			case DT_LNK:
			strcpy(newpath, headdir);
			strcat(newpath, "/");
			strcat(newpath, de->d_name);
			if (stat(newpath, &sb) == -1) {
				perror(newpath);
				break;
			}
			if (sb.st_size == 0) break;	// no interest in 0 length files
			ftyp = "s";
			goto REG_LNK_common;
			case DT_REG:
			strcpy(newpath, headdir);
			strcat(newpath, "/");
			strcat(newpath, de->d_name);
			if (stat(newpath, &sb) == -1) {
				perror(newpath);
				break;
			}
			if (sb.st_size == 0) break;	// no interest in 0 length files
			ftyp = "f";
REG_LNK_common:
			// check our excludes
			want = 1;
			index = 0;
			while (vlist[index]) {
				if (strstr(newpath, vlist[index])) {
					want = 0;
					break;
				}
				index++;
			}
			if(want){
				fprintf(fpo, "%lu %s%s %s\n", sb.st_size, newpath,
						pathend, ftyp );
			}
			break;
			case DT_DIR:
			// recurse using this pathname.
			strcpy(newpath, headdir);
			strcat(newpath, "/");
			strcat(newpath, de->d_name);
			recursedir(newpath, fpo, vlist);
			break;
			// Just report the error but nothing else.
			case DT_UNKNOWN:
			fprintf(stderr, "Unknown type:\n%s/%s\n\n", headdir,
					de->d_name);
			break;

		} // switch()
	} // while
	closedir(dirp);
} // recursedir()

char *domd5sum(const char *pathname)
{
	/* calculate md5sum of file in pathname */

	static char c[33];	// length of md5sum string + 1
	int i, hashsize, bytesread;
    struct md5_ctx ctx;
    unsigned char buffer[1048576];
    unsigned char hash[16];
    char *tmp;
    FILE *fpi;

    fpi = dofopen(pathname, "r");
    c[0] = '\0';

    md5_init_ctx (&ctx);

	while ((bytesread = fread(&buffer, 1, 1048576, fpi)) > 0) {
		if ((bytesread % 64) == 0) {
			md5_process_block (&buffer, bytesread, &ctx);
		} else {
			md5_process_bytes (&buffer, bytesread, &ctx);
		}
	}

	md5_finish_ctx (&ctx, &hash[0]);

	hashsize = 16;
	tmp = &c[0];
	/*TODO just unroll the loop below */
    for (i = 0; i < hashsize; i++) {
           sprintf(tmp, "%.2x", hash[i]);
        tmp += 2;
    }
    c[32] = '\0';
	fclose(fpi);
	return c;
} // domd5sum()

struct hashrecord parse_line(char *line)
{
	/* parse the data contained in line into the struct */
	char *cp;
	struct hashrecord hr;
	/* struct hashrecord {
	char thesum[33];
	ino_t ino;
	char ftyp;
	char path[PATH_MAX];
	}; */
	strncpy(hr.thesum, line, 32);
	hr.thesum[32] = '\0';
	cp = line + 34;	// first char of inode (as string).
	hr.ino = (unsigned long) atol(cp);
	while (*cp != 'f' && *cp != 's') cp++;	// point to file type
	hr.ftyp = *cp;
	cp++;	cp++;	// now looking at the path
	strcpy(hr.path, cp);
	cp = strstr(hr.path, pathend);
	if (cp) *cp = '\0';
	return hr;
} // parse_line()

void clipeol(char *line)
{
	char *cp;
	cp = strchr(line, '\n');
	if(cp) {
		*cp = '\0';
	}
} // clipeol()

char *thepathname(char *line)
{
	// <md5sum> <inode> <ftyp> <pathname><pathend>
	char *cp, *eop;
	cp = line + 32;
	while (*cp == ' ') cp++;	// hash
	while (isdigit(*cp)) cp++;	// inode
	while (*cp == ' ') cp++;	// ftyp
	cp++;	// space after ftyp
	while (*cp == ' ') cp++;	// path
	eop = strstr(cp, pathend);
	if (eop) *eop = '\0';
	return cp;
} // thepathname()

char *getcluster(char *path, int depth)
{
	static char buf[PATH_MAX];
	char *cp;
	int target;
	int count = 0;
	target = depth;
	strcpy(buf, path);
	cp = buf;
	// start our search for '/' after any leading junk that has one
	if (strncmp(cp, "../", 3) == 0) {
		cp += 3;
	} else if (strncmp(cp, "./", 2) == 0) {
		cp += 2;
	} else if (*cp == '/') {
		cp++;
	}
	while ((cp = strchr(cp, '/')) && (count < target)) {
		count++;
		cp++;
	}
	if (cp) *cp = '\0';
	strcat(buf, pathend);
	return buf;
} // getcluster()

void firstrun(void)
{
	char line[PATH_MAX], userhome[PATH_MAX];
	struct stat sb;
	FILE *fpi, *fpo;
	char *cfg;

	cfg = getconfigpath();
	strcpy(userhome, cfg);
	if(stat(userhome, &sb) == -1) {
		char *cp;
		char sharefile[PATH_MAX];
		// char *sharefile = "/usr/local/share/duplicates/excludes.conf";
		cp = strstr(userhome, "excludes.conf");
		*cp = '\0';
		if(stat(userhome, &sb) == -1) { // no dir
			if (mkdir(userhome, 0755) == -1) {
				perror(userhome);
				exit(EXIT_FAILURE);
			}
		}
		strcpy(sharefile, prefix);
		if (sharefile[strlen(sharefile)-1] != '/') {
			strcat(sharefile, "/");
		}
		strcat(sharefile, "duplicates/excludes.conf");
		// check that it exists in /usr/local/share/
		if (stat(sharefile, &sb) == -1) {
			strcpy(sharefile, "/usr/share/");
			strcat(sharefile, "duplicates/excludes.conf");
			if (stat(sharefile, &sb)== -1) {
				getprefix_from_user(sharefile);
			}
		}
		fpi = dofopen(sharefile, "r");
		strcat(userhome, "excludes.conf");
		fpo = dofopen(userhome, "w");
		while (fgets(line, PATH_MAX, fpi)){
			fputs(line, fpo);
		}
		fclose(fpi); fclose(fpo);
	}
	fputs(userhome, stderr);
	fputs(" has been created\n"
	"You can edit this file to control what pathnames to exclude\n"
	"from processing by duplicates.\n"
	"Excluded pathnames are any that contain the text in any line\n"
	"of the file, except what is commented by '#'.\n"
	,stderr);
} // firstrun()

char *getconfigpath(void)
{
	static char userhome[PATH_MAX];
	char *enval = getenv("USER");
	sprintf(userhome, "/home/%s/.config/duplicates/excludes.conf",
			enval);
	return userhome;
} // getconfigpath()

char **mem2strlist(char *from, char *to)
{	/* input is a block of memory comprising data seperated by '\n'
	Operate on the data to make a list of null terminated strings.
	* I'll have enough char * for all lines + NULL terminator.
	* If some lines end up empty then I'll have several terminators.
	*/
	char **vlist;
	char *cp, *tp, *bp;
	int cmnt = 0;
	int count = 0;
	int i;
	char buf[PATH_MAX];

	cp = from;
	while(cp < to) {
		if (*cp == '\n') count++;
		cp++;
	}
	count++;
	vlist = malloc(count * sizeof(char *));
	memset(vlist, 0, count * sizeof(char *));

	cp = from;
	tp = buf;
	i = 0;
	while (cp < to) {
		switch(*cp) {
			case '#':
			cmnt = 1;
			break;
			case '\n':
			cmnt = 0;
			*tp = '\0';
			if (strlen(buf)){
				bp = buf;	// get rid of leading whitespace
				while(isspace(*bp)) bp++;
				if (strlen(bp)) {
					vlist[i] = dostrdup(bp);
					i++;
				}
			}
			tp = buf;
			break;
			default:
			if(!(cmnt)){
				*tp = *cp;
				tp++;
			}
			break;
		}
		cp++;
	}
	// and what happens if the last char is not '\n'?
	if (tp > buf) {
		*tp = '\0';
		bp = buf;	// get rid of leading whitespace
		while(isspace(*bp)) bp++;
		if (strlen(bp)) {
			vlist[i] = dostrdup(bp);
			i++;
		}
	}
	vlist[i] = (char *)NULL;
	// what I have not yet done is clean white space off the ends
	i = 0;
	while((vlist[i])) {
		cp = vlist[i];
		tp = cp + strlen(vlist[i]) -1;
		while (isspace(*tp)) tp--;
		tp++; // back up to first space char
		*tp = '\0';
		i++;
	}
	return vlist;
} // mem2strlist()

void getprefix_from_user(char *sharefile)
{
	char ans[FILENAME_MAX];
	struct stat sb;
	char *cp;

	fputs("I cannot find a configuration file 'duplicates/excludes'\n"
	"in either '/usr/share/' or '/usr/local/share/' !\n"
	"It seems that you have installed to a non-standard path by using\n"
	"./configure --prefix=somedir\n"
	,stdout);
	fputs("Please enter the name you used to configure: ", stderr);
	cp = fgets(ans, FILENAME_MAX, stdin);
	if(!(cp)){	// only to stop gcc bitching
		perror("fgets");
	}
	// wipe out trailing '\n'
	cp = ans;
	while (*cp != '\n') cp++;
	*cp = '\0';
	if (ans[strlen(ans)-1] != '/') strcat(ans, "/");
	strcat(ans, "share/duplicates/excludes.conf");
	while (stat(ans, &sb) == -1) {
		fprintf(stderr, "No such file: %s\nPlease try again: ", ans);
		cp = fgets(ans, FILENAME_MAX, stdin);
		if(!(cp)){	// only to stop gcc bitching
			perror("fgets");
		}
		cp = ans;
		while(*cp != '\n') cp++;
		*cp = '\0';
		if (ans[strlen(ans)-1] != '/') strcat(ans, "/");
		strcat(ans, "share/duplicates/excludes.conf");
	}
	strcpy(sharefile, ans);
} // getprefix_from_user()
