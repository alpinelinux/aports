/* info.c - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2005-2009 Natanael Copa <n@tanael.org>
 * Copyright (C) 2009 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#include <fnmatch.h>
#include <stdio.h>
#include "apk_defines.h"
#include "apk_applet.h"
#include "apk_package.h"
#include "apk_database.h"
#include "apk_state.h"

struct search_ctx {
	int (*match)(struct apk_package *pkg, const char *str);
	int (*print)(struct apk_package *pkg);
	int argc;
	char **argv;
};

static int print_match(struct apk_package *pkg)
{
	printf("%s", pkg->name->name);
	if (apk_verbosity > 0)
		printf("-%s", pkg->version);
	if (apk_verbosity > 1) {
		printf(" - %s", pkg->description);
	}
	printf("\n");

	return 0;
}

static int print_rdepends(struct apk_package *pkg)
{
	struct apk_name *name, *name0;
	struct apk_package *pkg0;
	struct apk_dependency *dep;
	int i, j, k;

	name = pkg->name;

	printf("%s-%s:", pkg->name->name, pkg->version);
	for (i = 0; i < name->rdepends->num; i++) {
		name0 = name->rdepends->item[i];
		for (j = 0; j < name0->pkgs->num; j++) {
			pkg0 = name0->pkgs->item[j];
			for (k = 0; k < pkg0->depends->num; k++) {
				dep = &pkg0->depends->item[k];
				if (name == dep->name &&
				    (apk_version_compare(pkg->version, dep->version)
				      & dep->result_mask)) {
					printf(" %s-%s", pkg0->name->name, pkg0->version);
				}
			}
		}
	}
	printf("\n");

	return 0;
}

static int search_pkgname(struct apk_package *pkg, const char *str)
{
	return fnmatch(str, pkg->name->name, 0) == 0;
}

static int search_desc(struct apk_package *pkg, const char *str)
{
	return  strstr(pkg->name->name, str) != NULL ||
		strstr(pkg->description, str) != NULL;
}

static int search_parse(void *ctx, struct apk_db_options *dbopts,
		        int optch, int optindex, const char *optarg)
{
	struct search_ctx *ictx = (struct search_ctx *) ctx;

	switch (optch) {
	case 'd':
		ictx->match = search_desc;
		break;
	case 'r':
		ictx->print = print_rdepends;
		break;
	default:
		return -1;
	}
	return 0;
}

static int match_packages(apk_hash_item item, void *ctx)
{
	struct search_ctx *ictx = (struct search_ctx *) ctx;
	struct apk_package *pkg = (struct apk_package *) item;
	int i;

	for (i = 0; i < ictx->argc; i++)
		if (ictx->match(pkg, ictx->argv[i]))
			break;
	if (ictx->argc == 0 || i < ictx->argc)
		ictx->print(pkg);

	return 0;
}

static int search_main(void *ctx, struct apk_database *db, int argc, char **argv)
{
	struct search_ctx *ictx = (struct search_ctx *) ctx;
	struct apk_name *name;
	int rc = 0, i, j, slow_search;

	slow_search = ictx->match != NULL || argc == 0;
	if (!slow_search) {
		for (i = 0; i < argc; i++)
			if (strcspn(argv[i], "*?[") != strlen(argv[i])) {
				slow_search = 1;
				break;
			}
	}

	if (ictx->match == NULL)
		ictx->match = search_pkgname;
	if (ictx->print == NULL)
		ictx->print = print_match;
	else if (argc == 0)
		return -1;

	if (slow_search) {
		ictx->argc = argc;
		ictx->argv = argv;
		rc = apk_hash_foreach(&db->available.packages,
				      match_packages, ictx);
	} else {
		for (i = 0; i < argc; i++) {
			name = apk_db_query_name(db, APK_BLOB_STR(argv[i]));
			if (name == NULL)
				continue;
			for (j = 0; j < name->pkgs->num; j++)
				ictx->print(name->pkgs->item[j]);
		}
	}

	return rc;
}

static struct apk_option search_options[] = {
	{ 'd', "description",	"Search also package descriptions" },
	{ 'r', "rdepends",	"Print reverse dependencies of package" },
};

static struct apk_applet apk_search = {
	.name = "search",
	.help = "Search package by PATTERNs or by indexed dependencies.",
	.arguments = "PATTERN",
	.open_flags = APK_OPENF_READ | APK_OPENF_NO_STATE,
	.context_size = sizeof(struct search_ctx),
	.num_options = ARRAY_SIZE(search_options),
	.options = search_options,
	.parse = search_parse,
	.main = search_main,
};

APK_DEFINE_APPLET(apk_search);

