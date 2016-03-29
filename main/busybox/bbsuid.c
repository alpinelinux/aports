/*
 * Copyright (C) 2008 Natanael Copa <natanael.copa@gmail.com>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 *
 */

#include <sys/stat.h>
#include <sys/types.h>

#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BBSUID_PATH "/bin/bbsuid"

const static char * applets[] = {
	"/bin/mount",
	"/bin/umount",
	"/usr/bin/crontab",
	"/usr/bin/passwd",
	"/usr/bin/su",
	"/usr/bin/traceroute",
	"/usr/bin/traceroute6",
	"/usr/bin/vlock",
	NULL
};


static const char *applet_from_path(const char *str)
{
	const char *p = strrchr(str, '/');
	if (p == NULL)
		p = str;
	else
		p++;
	return p;
}

static int is_valid_applet(const char *str)
{
	int i;
	for (i = 0; applets[i] != NULL; i++) {
		const char *a = applet_from_path(applets[i]);
		if (strcmp(applet_from_path(str), a) == 0)
			return 1;
	}
	return 0;
}

int exec_busybox(const char *app, int argc, char **argv)
{
	char **newargv = malloc((argc + 2) * sizeof(char *));
	int i;
	newargv[0] = "/bin/busybox";
	newargv[1] = (char *)app;
	for (i = 1; i < argc; i++)
		newargv[i+1] = argv[i];
	newargv[argc+1] = NULL;
	execv(newargv[0], newargv);
	perror(newargv[0]);
	free(newargv);
	return 1;
}

static int install_links(void)
{
	int i, r = 0;
	/* we don't want others than root to install the symlinks */
	if (getuid() != 0)
		errx(1, "Only root can install symlinks");

	for (i = 0; applets[i] != NULL; i++) {
		const char *a = applets[i];
		struct stat st;
		if (lstat(a, &st) == 0 && S_ISLNK(st.st_mode))
			unlink(a);
		if (symlink(BBSUID_PATH, a) < 0)
			r++;
	}

	return r;
}

int main(int argc, char **argv)
{
	const char *app = applet_from_path(argv[0]);

	if (strcmp(app, "bbsuid") == 0) {
		if (argc == 2 && strcmp(argv[1], "--install") == 0)
			return install_links();
		errx(1, "Use --install to install symlinks");
	}

	if (is_valid_applet(app))
		return exec_busybox(app, argc, argv);

	errx(1, "%s is not a valid applet", app);
	return 1;
}

