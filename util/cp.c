/* See LICENSE file for copyright and license details. */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>

#include "../fs.h"
#include "../text.h"
#include "../util.h"

bool cp_aflag = false;
bool cp_dflag = false;
bool cp_fflag = false;
bool cp_pflag = false;
bool cp_rflag = false;
bool cp_vflag = false;
int cp_status = EXIT_SUCCESS;

int
cp(const char *s1, const char *s2)
{
	FILE *f1, *f2;
	char *ns1, *ns2;
	long size1, size2;
	struct dirent *d;
	struct stat st;
	struct utimbuf ut;
	char buf[PATH_MAX];
	DIR *dp;
	int r;

	if(cp_vflag)
		printf("'%s' -> '%s'\n", s1, s2);

	if(cp_dflag == true)
		r = lstat(s1, &st);
	else
		r = stat(s1, &st);

	if(r == 0) {
		if(cp_dflag == true && S_ISLNK(st.st_mode)) {
			if(readlink(s1, buf, sizeof(buf) - 1) >= 0) {
				if(cp_fflag == true);
					unlink(s2);
				if(symlink(buf, s2) != 0) {
					weprintf("%s: can't create '%s'\n", argv0, s2);
					cp_status = EXIT_FAILURE;
					return 0;
				}
			}
			goto preserve;
		}
		if(S_ISDIR(st.st_mode)) {
			if (!cp_rflag)
				eprintf("%s: is a directory\n", s1);

			if(!(dp = opendir(s1)))
				eprintf("opendir %s:", s1);

			if (mkdir(s2, st.st_mode) == -1 && errno != EEXIST)
				eprintf("mkdir %s:", s2);

			apathmax(&ns1, &size1);
			apathmax(&ns2, &size2);
			while((d = readdir(dp))) {
				if(strcmp(d->d_name, ".") && strcmp(d->d_name, "..")) {
					r = snprintf(ns1, size1, "%s/%s", s1, d->d_name);
					if(r >= size1 || r < 0) {
						eprintf("%s/%s: filename too long\n",
							s1, d->d_name);
					}
					r = snprintf(ns2, size2, "%s/%s", s2, d->d_name);
					if(r >= size2 || r < 0) {
						eprintf("%s/%s: filename too long\n",
							s2, d->d_name);
					}
					fnck(ns1, ns2, cp);
				}
			}
			closedir(dp);
			free(ns1);
			free(ns2);
			goto preserve;
		}
	}

	if(cp_aflag == true) {
		if(S_ISBLK(st.st_mode) || S_ISCHR(st.st_mode) ||
		   S_ISSOCK(st.st_mode) || S_ISFIFO(st.st_mode)) {
			unlink(s2);
			if(mknod(s2, st.st_mode, st.st_rdev) < 0) {
				weprintf("%s: can't create '%s':", argv0, s2);
				cp_status = EXIT_FAILURE;
				return 0;
			}
			goto preserve;
		}
	}

	if(!(f1 = fopen(s1, "r"))) {
		weprintf("fopen %s:", s1);
		cp_status = EXIT_FAILURE;
		return 0;
	}

	if(!(f2 = fopen(s2, "w"))) {
		if (cp_fflag == true) {
			unlink(s2);
			if(!(f2 = fopen(s2, "w"))) {
				weprintf("fopen %s:", s2);
				cp_status = EXIT_FAILURE;
				return 0;
			}
		} else {
			weprintf("fopen %s:", s2);
			cp_status = EXIT_FAILURE;
			return 0;
		}
	}
	concat(f1, s1, f2, s2);
	fchmod(fileno(f2), st.st_mode);
	fclose(f2);
	fclose(f1);

preserve:
	if(cp_aflag == true || cp_pflag == true) {
		if(!(S_ISLNK(st.st_mode))) {
			/* timestamp */
			ut.actime = st.st_atime;
			ut.modtime = st.st_mtime;
			utime(s2, &ut);
		}
		/* preserve owner ? */
		if(S_ISLNK(st.st_mode))
			r = lchown(s2, st.st_uid, st.st_gid);
		else
			r = chown(s2, st.st_uid, st.st_gid);
		if(r == -1) {
			weprintf("cp: can't preserve ownership of '%s':", s2);
			cp_status = EXIT_FAILURE;
		}
	}
	return 0;
}
