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
#include <string.h>

#include "apk_defines.h"
#include "apk_applet.h"

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

int main(int argc, char **argv)
{
	struct apk_applet **a, *applet;
	char *prog;

	prog = strrchr(argv[0], '/');
	if (prog == NULL)
		prog = argv[0];
	else
		prog++;

	if (strcmp(prog, "apk") == 0) {
		if (argc < 2)
			return usage();
		prog = argv[1];
		argv++;
		argc--;
	} else if (strncmp(prog, "apk_", 4) == 0) {
		prog += 4;
	} else
		return usage();

	for (a = &__start_apkapplets; a < &__stop_apkapplets; a++) {
		applet = *a;
		if (strcmp(prog, applet->name) == 0) {
			argv++;
			argc--;
			return applet->main(argc, argv);
		}
	}

	return usage();
}
