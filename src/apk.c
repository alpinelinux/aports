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
#include <fcntl.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/stat.h>

#include "apk_defines.h"
#include "apk_applet.h"

const char *apk_root;
struct apk_repository_url apk_repository_list;
int apk_verbosity = 1, apk_progress = 0, apk_upgrade = 0;
int apk_clean = 0, apk_force = 0;
int apk_cwd_fd;

void apk_log(const char *prefix, const char *format, ...)
{
	va_list va;

	if (prefix != NULL)
		fprintf(stderr, "%s", prefix);
	va_start(va, format);
	vfprintf(stderr, format, va);
	va_end(va);
	fprintf(stderr, "\n");
}

int version(void)
{
	printf("apk-tools " APK_VERSION "\n");
	return 0;
}
int usage(void)
{
	struct apk_applet **a, *applet;

	version();
	printf("\nUsage:\n");

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

static struct apk_applet *deduce_applet(int argc, char **argv)
{
	struct apk_applet *a;
	const char *prog;
	int i;

	prog = strrchr(argv[0], '/');
	if (prog == NULL)
		prog = argv[0];
	else
		prog++;

	if (strncmp(prog, "apk_", 4) == 0)
		return find_applet(prog + 4);

	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-')
			continue;

		a = find_applet(argv[i]);
		if (a != NULL)
			return a;
	}

	return NULL;
}

static struct apk_repository_url *apk_repository_new(const char *url)
{
	struct apk_repository_url *r = calloc(1, 
			sizeof(struct apk_repository_url));
	if (r) {
		r->url = url;
		list_init(&r->list);
	}
	return r;
}

#define NUM_GENERIC_OPTS 8
static struct option generic_options[32] = {
	{ "root",		required_argument,	NULL, 'p' },
	{ "repository",		required_argument,	NULL, 'X' },
	{ "quiet",		no_argument,		NULL, 'q' },
	{ "verbose",		no_argument,		NULL, 'v' },
	{ "version",		no_argument,		NULL, 'V' },
	{ "progress",		no_argument,		&apk_progress,	1 },
	{ "clean-protected",	no_argument,		&apk_clean,	1 },
	{ "force",		no_argument,		&apk_force,	1 },
};

int main(int argc, char **argv)
{
	struct apk_applet *applet;
	char short_options[256], *sopt;
	struct option *opt;
	int r, optindex;
	void *ctx = NULL;
	struct apk_repository_url *repo = NULL;

	umask(0);
	apk_cwd_fd = open(".", O_RDONLY);
	apk_root = getenv("ROOT");
	list_init(&apk_repository_list.list);

	applet = deduce_applet(argc, argv);
	if (applet != NULL) {
		if (applet->num_options && applet->options) {
			memcpy(&generic_options[NUM_GENERIC_OPTS],
			       applet->options,
			       applet->num_options * sizeof(struct option));
		}

		if (applet->context_size != 0)
			ctx = calloc(1, applet->context_size);
	}

	for (opt = &generic_options[0], sopt = short_options;
	     opt->name != NULL; opt++) {
		if (opt->flag == NULL &&
		    opt->val <= 0xff && isalnum(opt->val)) {
			*(sopt++) = opt->val;
			if (opt->has_arg != no_argument)
				*(sopt++) = ':';
		}
	}

	optindex = 0;
	while ((r = getopt_long(argc, argv, short_options,
				generic_options, &optindex)) != -1) {
		switch (r) {
		case 0:
			break;
		case 'p':
			apk_root = optarg;
			break;
		case 'X':
			repo = apk_repository_new(optarg);
			if (repo)
				list_add(&repo->list, &apk_repository_list.list);
			break;
		case 'q':
			apk_verbosity--;
			break;
		case 'v':
			apk_verbosity++;
			break;
		case 'V':
			return version();
			break;
		default:
			if (applet == NULL || applet->parse == NULL)
				return usage();
			if (applet->parse(ctx, r, optindex - NUM_GENERIC_OPTS,
					  optarg) != 0)
				return usage();
			break;
		}
	}

	if (applet == NULL)
		return usage();

	if (apk_root == NULL)
		apk_root = "/";

	argc -= optind;
	argv += optind;
	if (argc >= 1 && strcmp(argv[0], applet->name) == 0) {
		argc--;
		argv++;
	}

	return applet->main(ctx, argc, argv);
}
