/*  firstrun.c
 *
 *	Copyright 2015 Bob Parker <rlp1938@gmail.com>
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

#include "firstrun.h"

int checkfirstrun(char *progname)
{
	// construct the user's path to .config
	char upath[PATH_MAX];
	sprintf(upath, "%s/.config/%s/", getenv("HOME"), progname);
	return direxists(upath);
} // checkfirstrun()

void firstrun(char *progname, ...)
{
	/* The purpose of this function is to copy a caller specified set
	 * of files from /usr/local/share/<progname>/ to
	 * $HOME/.config/<progname>/
	 * The assumption is that the user who built the program has not
	 * meddled with --prefix at configure time. Anyone who has the
	 * confidence to do this will no doubt have the ability to make
	 * the necessary copies by hand instead of relying on this feature.
	*/
	va_list ap;
	char upath[NAME_MAX];
	char command[NAME_MAX];
	sprintf(upath, "%s/.config/%s/", getenv("HOME"), progname);
	sprintf(command, "mkdir -p %s", upath);
	dosystem(command);
	char srcpath[80];
	sprintf(srcpath, "/usr/local/share/%s/", progname);
	va_start(ap, progname);	// allow this to work with 0 named files.
	char *cp;
	char fn[NAME_MAX];
	while (1) {
		cp = va_arg(ap, char *);
		if (!cp) break;	// last va must be NULL.
		// file to read
		sprintf(fn, "%s%s", srcpath, cp);
		fdata fdat = readfile(fn, 0, 1);
		sprintf(fn, "%s%s", upath, cp);
		writefile(fn, fdat.from, fdat.to, "w");
		free(fdat.from);
	}
	va_end(ap);
} // firstrun()

void dosystem(const char *cmd)
{
	const int status = system(cmd);

    if (status == -1) {
        fprintf(stderr, "system failed to execute: %s\n", cmd);
        exit(EXIT_FAILURE);
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
        fprintf(stderr, "%s failed with non-zero exit\n", cmd);
        exit(EXIT_FAILURE);
    }

    return;
} // dosystem()
