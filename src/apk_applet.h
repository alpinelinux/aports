/* apk_applet.h - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2005-2008 Natanael Copa <n@tanael.org>
 * Copyright (C) 2008 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#ifndef APK_APPLET_H
#define APK_APPLET_H

#include <getopt.h>

extern const char *apk_root;
extern const char *apk_repository;

struct apk_applet {
	const char *name;
	const char *usage;

	int context_size;
	int num_options;
	struct option *options;

	int (*parse)(void *ctx, int optch, int optindex, const char *optarg);
	int (*main)(void *ctx, int argc, char **argv);
};

extern struct apk_applet *__start_apkapplets, *__stop_apkapplets;

#define APK_DEFINE_APPLET(x) \
	static struct apk_applet *__applet_##x \
	__attribute__((__section__("apkapplets"))) __attribute((used)) = &x;

#endif
