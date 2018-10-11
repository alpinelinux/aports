/*
 * abuild-rmtemp
 * Copyright (c) 2017 Kaarle Ritvanen
 * Distributed under GPL-2
 */

#define _XOPEN_SOURCE 700
#include <err.h>
#include <errno.h>
#include <ftw.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define PREFIX "/var/tmp/abuild."

static void fail() {
	errx(1, "%s", strerror(errno));
}

static int handler(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
	return remove(fpath);
}

int main(int argc, char **argv) {
	if (argc < 2) return 0;

	if (getuid()) {
		argv[0] = "-abuild-rmtemp";
		execv("/usr/bin/abuild-sudo", argv);
	}

	if (strncmp(argv[1], PREFIX, strlen(PREFIX)) || \
		strchr(argv[1] + strlen(PREFIX), '/'))
		errx(1, "Invalid path: %s", argv[1]);

	struct stat s;
	if (lstat(argv[1], &s)) fail();
	struct passwd *p = getpwnam(getenv("USER"));
	if (!p) errx(1, "Incorrect user");
	if (s.st_uid != p->pw_uid) errx(1, "Permission denied");

	if (nftw(argv[1], handler, 512, FTW_DEPTH|FTW_PHYS)) fail();

	return 0;
}
