/* apk.c - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2005-2008 Natanael Copa <n@tanael.org>
 * Copyright (C) 2008 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/stat.h>

#include "apk_defines.h"
#include "apk_applet.h"

const char *apk_root = "/";
const char *apk_repository = NULL;

void apk_log(const char *prefix, const char *format, ...)
{
	va_list va;

	if (prefix != NULL)
		fprintf(stderr, prefix);
	va_start(va, format);
	vfprintf(stderr, format, va);
	va_end(va);
	fprintf(stderr, "\n");
}

int usage(void)
{
	struct apk_applet **a, *applet;

	printf("apk-tools " APK_VERSION "\n"
	       "\n"
	       "Usage:\n");

	for (a = &__start_apkapplets; a < &__stop_apkapplets; a++) {
		applet = *a;
		printf("    apk %s %s\n",
		       applet->name, applet->usage);
	}
	printf("\n");

	return 1;
}

static struct apk_applet *find_applet(const char *name)
{
	struct apk_applet **a;

	for (a = &__start_apkapplets; a < &__stop_apkapplets; a++) {
		if (strcmp(name, (*a)->name) == 0)
			return *a;
	}
	
	return NULL;
}

int main(int argc, char **argv)
{
	static struct option generic_options[] = {
		{"root", required_argument, NULL, 'Q' },
		{"repository", required_argument, NULL, 'X' },
		{0, 0, 0, 0},
	};
	struct apk_applet *applet = NULL;
	const char *prog;
	int r;

	umask(0);

	prog = strrchr(argv[0], '/');
	if (prog == NULL)
		prog = argv[0];
	else
		prog++;

	if (strncmp(prog, "apk_", 4) == 0)
		applet = find_applet(prog + 4);

	while ((r = getopt_long(argc, argv, "",
				generic_options, NULL)) != -1) {
		switch (r) {
		case 'Q':
			apk_root = optarg;
			break;
		case 'X':
			apk_repository = optarg;
			break;
		default:
			return usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (applet == NULL) {
		if (argc > 0)
			applet = find_applet(argv[0]);
		if (applet == NULL)
			return usage();

		argc--;
		argv++;
	}

	return applet->main(argc, argv);
}
