/*      duplicates.c
 *
 *	Copyright 2016 Bob Parker <rlp1938@gmail.com>
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
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <limits.h>

#include "md5.h"
#include "fileops.h"
#include "firstrun.h"

static int filecount;

static char *prefix;

static const char *pathend = "!*END*!";	// Anyone who puts shit like
										// that in a filename deserves
										// what happens.

struct hashrecord {
	char thesum[33];
	dev_t dev;
	ino_t ino;
	char ftyp;
	char path[PATH_MAX];
};

struct sizerecord {
	size_t filesize;
	dev_t dev;
	ino_t ino;
	char path[PATH_MAX];
	char ftyp;
};


static void help_print(int forced);

static void recursedir(char *headdir, FILE *fpo, char **vlist);
static char *domd5sum(const char *pathname);
static struct hashrecord parse_line(char *line);
static char **mem2strlist(char *from, char *to);
static struct sizerecord parse_size_line(char *line);
static void screenfilelist(const char *filein, const char *fileout,
			int verbosity);
static void screenuniquesizeonly(const char *filein,
				const char *fileout, int verbosity);
static int cmp(const char *path1, const char *path2, FILE *fpo);
static void cluster_output(const char *pathin, const char *pathout);
static void report(const char *path, int verbosity);
static void stripclusterpath(const char *filein, FILE *fpo);

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


int main(int argc, char **argv)
{
	int opt, verbosity, delworks, vlindex;
	char *workfile0;
	char *workfile1;
	char *workfile2;
	char *workfile3;
	char *workfile4;
	char *workfile5;
	char command[FILENAME_MAX];
	FILE *fpo;
	char **vlist;
	struct stat sb;
	// set default values
	verbosity = 0;
	filecount = 0;
	delworks = 1;	// delete workfiles is the default.
	prefix = dostrdup("/usr/local/");

	eol = "\n";	// string in case I ever want to do Microsoft

	while((opt = getopt(argc, argv, ":hvd")) != -1) {
		switch(opt){
		case 'h':
			help_print(0);
		break;
		case 'd':
			delworks = 0;	// don't trash the workfiles at end
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

	// first run ?
	if (checkfirstrun("duplicates")) {
		firstrun("duplicates", "excludes.conf");
		fprintf(stdout, "Configuration file installed: %s"
		"\nat $HOME/.config/duplicates/\n",
					"excludes.conf");
		fputs("Edit this file to match your requirements.\n", stdout);
		exit(EXIT_SUCCESS);
	}

	// get my exclusions file
	char *efl = getconfigfile("duplicates", "excludes.conf");
	fdata fdat = readfile(efl, 0, 1);
	free(efl);
	// turn the exclusions file into an array of strings
	vlist = mem2strlist(fdat.from, fdat.to);
	free(fdat.from);

	// open the output workfile
	fpo = dofopen(workfile0, "w");
	// List the files
	if (verbosity){
		fputs("Generating list of files and symlinks\n",stderr);
	}
	int crossdev = 0;
	int thedev = 0;
	while (argv[optind]) {
		char *topdir = argv[optind];
		if (direxists(topdir) == -1) {
			fprintf(stderr, "%s non-existent or not a dir.\n", topdir);
			exit(EXIT_FAILURE);
		}
		int newdev = getdeviceid(topdir);
		if (newdev == -1) {
			fprintf(stderr, "Error processing %s\n", topdir);
			exit(EXIT_FAILURE);
		}
		if (thedev == 0) {
			thedev = newdev;	// first look
		} else {
			if (newdev != thedev) crossdev = 1;	// new process
		}
		{
			// get rid of trailing '/'
			int len = strlen(topdir);
			if (topdir[len-1] == '/') topdir[len-1] = '\0';
		}
		recursedir(topdir, fpo, vlist);
		optind++;
	} // while(argv[optind])
	dofclose(fpo);

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
	dosystem(command);

	if (verbosity){
		fputs("Screening out non-duplicates and recording md5sum"
		" as needed\n", stderr);
	}

	// pre screening
	if (crossdev) {
		// screen what we have to discard unique file sizes.
		screenuniquesizeonly(workfile1, workfile2, verbosity);
	} else {
		// screen what we have to discard unique file size,
		// check for inodes in common, test if files differ.
		screenfilelist(workfile1, workfile2, verbosity);
	}

	// get rid of duplicated records.
	sprintf(command, "sort -u %s > %s",
						workfile2, workfile3);
	dosystem(command);

	/*
	 * The data in workfile3 is sorted on the hash values so not well
	 * suited for human processing. Next step is to group the clusters
	 * of same hashes sorted by the path that exists on the first
	 * record in each cluster.
	*/
	cluster_output(workfile3, workfile4);
	// sort by cluster
	sprintf(command, "sort %s > %s",
						workfile4, workfile5);
	dosystem(command);

	// send results to stdout
	stripclusterpath(workfile5, stdout);

	// clean up
	if (crossdev == 0) {	// if otherwise it was never opened.
		if (stat("comparison_errors", &sb) == -1) {
			perror("comparison_errors");
		} else if (sb.st_size != 0) {
			fputs(
			"One or more volatile files changed size during the run.\n"
			"You might want to view ./comparison_errors and edit\n "
			" $HOME/.config/duplicates/excludes,"
			" to avoid this problem.\n"
			, stderr);
		} else {
			unlink("comparison_errors");
		}
	}
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
	}


	return 0;
} // main()

void help_print(int forced){
    fputs(helptext, stderr);
    exit(forced);
} // help_print()

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
			if (!(S_ISREG(sb.st_mode))) break; // only regular files.
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
				fprintf(fpo, "%.20lu %.16lx %.16lx %s%s %s\n",
						sb.st_size, sb.st_ino, sb.st_dev, newpath,
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
	int i, hashsize;
        size_t bytesread;
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
	dofclose(fpi);
	return c;
} // domd5sum()

struct hashrecord parse_line(char *line)
{
	/* parse the data contained in line into the struct */
	// <md5sum> <inode number> <path><pathend> <f|t>
	char *cp, *eop;
	struct hashrecord hr;
	char buf[PATH_MAX];
	/* struct hashrecord {
	char thesum[33];
	dev_t dev;
	ino_t ino;
	char ftyp;
	char path[PATH_MAX];
	}; */
	// Input record, line.
	// <md5sum> <dev> <inode> <path><pathend> <f|s>
	strcpy(buf, line);
	hr.ftyp = buf[strlen(buf) - 1];
	strncpy(hr.thesum, buf, 32);
	hr.thesum[32] = '\0';
	cp = buf + 33;	// first digit of inode (as string).
	hr.ino = strtoul(cp, NULL, 16);	// inode in hex
	cp += 17;	// inode length + 1
	hr.dev = strtoul(cp, NULL, 16);
	cp += 17;	// dev length + 1
	eop = strstr(cp, pathend);
	if (eop) *eop = '\0';
	strcpy(hr.path, cp);
	return hr;
} // parse_line()

char **mem2strlist(char *from, char *to)
{	/* input is a block of memory comprising data seperated by '\n'
	Operate on the data to make a list of null terminated strings.
	* I'll have enough char * for all lines + NULL terminator.
	* If some lines end up empty then I'll have several terminators.
	*/
	char **vlist;
	char *cp, *tp, *bp;
	int cmnt = 0;
	unsigned count = 0;
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

static struct sizerecord parse_size_line(char *line)
{
	/*
	 * <file size> <dev> <inode> <path><pathend> <s|f>
	*/
	struct sizerecord sr;
	char *cp, *eop;
	char buf[PATH_MAX];

	strcpy(buf, line);	// needed, it gets clobbered
	sr.ftyp = buf[strlen(buf) -1 ];
	cp = buf;
	sr.filesize = strtoul(cp, NULL, 10);
	cp += 21;	// length of size_t + 1
	// now looking at inode
	sr.ino = strtoul(cp, NULL, 16);
	cp += 17;	// length of dev_t + 1
	// looking at dev
	sr.dev = strtoul(cp, NULL, 16);
	cp += 17;	// length of ino_t + 1
	// now looking at path
	eop = strstr(cp, pathend);
	*eop = '\0';
	memset(sr.path, 0, PATH_MAX); // in case I need to use gdb
	strcpy(sr.path, cp);
	return sr;
} // parse_size_line()

static void screenfilelist(const char *filein, const char *fileout,
			int verbosity)
{
	/*
	 * look through the sorted workfile and discard those that have
	 * unique sizes, also all but the last of those that have the same
	 * size and inode number. Of those that have the same size and
	 * different inode numbers, compare the lesser of file size or
	 * 128 kbytes and if the files differ, discard the first presented
	 * record. For what remains calculate md5sums and if the hashes
	 * match record them.
	*/
	FILE *fpo, *fplog;
	char *line1, *line2;
	struct sizerecord sr1, sr2;

	fplog = dofopen("comparison_errors", "w");

	fdata fdat = readfile(filein, 0, 1);
	fpo = dofopen(fileout, "w");
	line1 = fdat.from;
	// replace '\n' with '\0'
	while(line1 < fdat.to) {
		line1 = memchr(line1, '\n', PATH_MAX);
		if (line1) *line1 = '\0';
		line1++;	// at next line
	}
	line1 = fdat.from;
	sr1 = parse_size_line(line1);
	report(sr1.path, verbosity);
	line2 = line1 + strlen(line1) +1;
	while(line2 < fdat.to){
		sr2 = parse_size_line(line2);
		report(sr2.path, verbosity);
			if (sr1.filesize != sr2.filesize) {
				// forget line1 it has a unique size.
				goto re_init;
			}
			if (sr1.ino == sr2.ino) {
				// these two files are linked so forget line1
				goto re_init;
			}
			// here there are two files of same size and differing inode
			// or on different devices, see if the content differs.

			if (cmp(sr1.path, sr2.path, fplog) != 0) {
				/* cmp() is definitive in the negative but not in
				 * the positive if the filesize exceeds 128 k because it
				 * only tests the smaller of filesize or 128 kbyte. If
				 * it looks as if it may be a match then I'll hash sum
				 * them. When there is a cluster of matches we will get
				 * duplicated output records, sort -u is required.
				 * */
				 goto re_init;	// content does not match, skip line1
			} else {
				char hash1[33], hash2[33];
				strcpy(hash1, domd5sum(sr1.path));
				strcpy(hash2, domd5sum(sr2.path));
				if (strncmp(hash1, hash2, 32) == 0) {
					fprintf(fpo, "%s %.16lx %.16lx %s%s %c\n",
							hash1, sr1.ino, sr1.dev, sr1.path, pathend,
							sr1.ftyp);
					fprintf(fpo, "%s %.16lx %.16lx %s%s %c\n",
							hash2, sr2.ino, sr2.dev, sr2.path, pathend,
							sr2.ftyp);
				}
			}
re_init:
		sr1 = sr2;
		line1 = line2;	// handy if I'm in gdb
		line2 += strlen(line2) +1;
	} // while(line2...)
	dofclose(fpo);
	dofclose(fplog);
} // screenfilelist()

int cmp(const char *path1, const char *path2, FILE *fplog)
{
	/* compares byte by byte of path1 and path2 to the smaller of
	 * filesize or 128 kbyte. Returns 0 if all bytes match or
	 * non zero, otherwise.
	 * */
	const int chunk = 131072;	// 128k
	FILE *fp1, *fp2;
	char buf1[chunk], buf2[chunk];
	off_t b1, b2;
	// Not fatal on "No such file... error"
	fp1 = fopen(path1, "r");
	if (!(fp1)) {
		fputs("cmp(1)\n", stderr);
		perror(path1);
		return 1;
	}
	fp2 = fopen(path2, "r");
	if (!(fp2)) {
		fputs("cmp(2)\n", stderr);
		perror(path2);
		dofclose(fp1);
		return 1;
	}

	b1 = fread(buf1, 1, chunk, fp1);
	b2 = fread(buf2, 1, chunk, fp2);
	dofclose(fp1);
	dofclose(fp2);
	if (b1 != b2) {	// record the errors in a log file. Not fatal
		fprintf(fplog, "File length mismatch: %s %lu , %s %lu\n",
				path1, b1, path2, b2);
		return 1;
	}

	return(strncmp(buf1, buf2, b1));// 0 on match, else non zero.
} // cmp()

static void cluster_output(const char *workfilein,
								const char *workfileout)
{
	char clustername[PATH_MAX];
	struct fdata fdat;
	char *line1, *line2;
	struct hashrecord hr1, hr2;
	FILE *fpo;
	fpo = dofopen(workfileout, "w");
	fdat = readfile(workfilein, 0, 1);
	line1 = fdat.from;
	// make C strings
	while(line1 < fdat.to) {
		if(*line1 == '\n') *line1 = '\0';
		line1++;
	}

	line1 = fdat.from;
	hr1 = parse_line(line1);
	strcpy(clustername, hr1.path);
	line2 = line1 + strlen(line1) +1;
	hr2 = parse_line(line2);
	while(line1 < fdat.to) {
		fprintf(fpo, "%s%s %s %.16lx %.16lx %s%s %c\n", clustername,
				pathend, hr1.thesum, hr1.ino, hr1.dev, hr1.path,
				pathend, hr1.ftyp );
		while (strcmp(hr2.thesum, hr1.thesum) == 0) {
			fprintf(fpo, "%s%s %s %.16lx %.16lx %s%s %c\n", clustername, 	pathend, hr2.thesum, hr2.ino, hr2.dev, hr2.path,
				pathend, hr2.ftyp);
			line2 += strlen(line2) +1;
			hr2 = parse_line(line2);
		} // while(strncmp())
		line1 = line2;
		line2 += strlen(line1) + 1;
		if (line2 < fdat.to){
			hr1 = parse_line(line1);
			hr2 = parse_line(line2);
			strcpy(clustername, hr1.path);
		}
	} // while(line1...)
	dofclose(fpo);
} // cluster_output()

static void report(const char *path, int verbosity)
{
	/* list a certain number of files that have been considered,
	 * not the files written out with hashes associated.*/
	 filecount++;
	 switch(verbosity){
		 case 0:
		 case 1:
		 break;
		 case 2:
		 if (filecount % 100 == 0) {
			 fprintf(stderr, "%s\n", path);
		 }
		 break;
		 case 3:
		 if (filecount % 10 == 0) {
			 fprintf(stderr, "%s\n", path);
		 }
		 break;
		 default:	// 4 and greater report all.
		 fprintf(stderr, "%s\n", path);
		 break;
	 }
} // report()

static void screenuniquesizeonly(const char *filein,
				const char *fileout, int verbosity)
{	/*
	 * Look through the sorted workfile and discard those that have
	 * unique sizes. For the others. calculate md5sums and record them.
	*/
	FILE *fpo;
	char *line1, *line2;
	struct sizerecord sr1, sr2;
	struct fdata fdat;

	fdat = readfile(filein, 0, 1);
	fpo = dofopen(fileout, "w");
	line1 = fdat.from;
	// replace '\n' with '\0'
	while(line1 < fdat.to) {
		line1 = memchr(line1, '\n', PATH_MAX);
		if (line1) *line1 = '\0';
		line1++;	// at next line
	}
	line1 = fdat.from;
	sr1 = parse_size_line(line1);
	report(sr1.path, verbosity);
	line2 = line1 + strlen(line1) +1;
	while(line2 < fdat.to){
		char hash1[33], hash2[33];
		sr2 = parse_size_line(line2);
		report(sr2.path, verbosity);
			if (sr1.filesize != sr2.filesize) {
				// forget line1 it has a unique size.
				goto re_init;
			}
		strcpy(hash1, domd5sum(sr1.path));
		strcpy(hash2, domd5sum(sr2.path));
		fprintf(fpo, "%s %.16lx %.16lx %s%s %c\n", hash1, sr1.ino,
					sr1.dev, sr1.path, pathend, sr1.ftyp);
		fprintf(fpo, "%s %.16lx %.16lx %s%s %c\n", hash2, sr2.ino,
					sr2.dev, sr2.path, pathend, sr2.ftyp);
		// this will cause duplicated output lines  so sort -u required.
re_init:
		sr1 = sr2;
		line1 = line2;	// handy if I'm in gdb
		line2 += strlen(line2) +1;
	} // while(line2...)
	dofclose(fpo);
} // screenuniquesizeonly()

static void stripclusterpath(const char *filein, FILE *fpo)
{
	// get rid of the clustername in front of everything
	struct fdata fdat;
	char *cp;

	fdat = readfile(filein, 0, 1);
	// stringify the mess.
	cp = fdat.from;
	while (cp < fdat.to) {
		if (*cp == '\n') *cp = '\0';
		cp++;
	}
	//<clustername><pathend> <md5sum> <inode> <dev> <path><pathend><s|f>
	cp = fdat.from;
	while(cp < fdat.to) {
		cp = strstr(cp, pathend);
		cp += strlen(pathend) +1;	// at md5sum
		fprintf(fpo, "%s\n", cp);
		cp += strlen(cp) +1;	// next line.
	}
} // stripclusterpath()
