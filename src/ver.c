/* ver.c - Alpine Package Keeper (APK)
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
#include "apk_defines.h"
#include "apk_applet.h"
#include "apk_database.h"
#include "apk_version.h"
#include "apk_print.h"

struct ver_ctx {
	int (*action)(struct apk_database *db, int argc, char **argv);
	const char *limchars;
};

static int ver_indexes(struct apk_database *db, int argc, char **argv)
{
	struct apk_repository *repo;
	int i;

	for (i = 0; i < db->num_repos; i++) {
		repo = &db->repos[i];

		if (APK_BLOB_IS_NULL(repo->description))
			continue;

		printf("%.*s [%s]\n",
                       (int) repo->description.len,
		       repo->description.ptr,
		       db->repos[i].url);
	}

	return 0;
}

static int ver_test(struct apk_database *db, int argc, char **argv)
{
	int r;

	if (argc != 2)
		return 1;

	r = apk_version_compare(argv[0], argv[1]);
	printf("%s\n", apk_version_op_string(r));
	return 0;
}

static int ver_validate(struct apk_database *db, int argc, char **argv)
{
	int i, r = 0;
	for (i = 0; i < argc; i++) {
		if (!apk_version_validate(APK_BLOB_STR(argv[i]))) {
			if (apk_verbosity > 0)
				printf("%s\n", argv[i]);
			r++;
		}
	}
	return r;
}

static int ver_parse(void *ctx, struct apk_db_options *dbopts,
		     int opt, int optindex, const char *optarg)
{
	struct ver_ctx *ictx = (struct ver_ctx *) ctx;
	switch (opt) {
	case 'I':
		ictx->action = ver_indexes;
		break;
	case 't':
		ictx->action = ver_test;
		break;
	case 'c':
		ictx->action = ver_validate;
		break;
	case 'l':
		ictx->limchars = optarg;
		break;
	default:
		return -1;
	}
	return 0;
}

static void ver_print_package_status(struct apk_package *pkg, const char *limit)
{
	struct apk_name *name;
	struct apk_package *tmp;
	char pkgname[256];
	const char *opstr, *latest = "";
	int i, r = -1;

	name = pkg->name;
	for (i = 0; i < name->pkgs->num; i++) {
		tmp = name->pkgs->item[i];
		if (tmp->name != name || tmp->repos == 0)
			continue;
		r = apk_version_compare(tmp->version, latest);
		if (r == APK_VERSION_GREATER)
			latest = tmp->version;
	}
	r = apk_version_compare(pkg->version, latest);
	opstr = apk_version_op_string(r);
	if ((limit != NULL) && (strchr(limit, *opstr) == NULL))
		return;
	snprintf(pkgname, sizeof(pkgname), "%s-%s", name->name, pkg->version);
	printf("%-40s%s %s\n", pkgname, opstr, latest);
}

static int ver_main(void *ctx, struct apk_database *db, int argc, char **argv)
{
	struct ver_ctx *ictx = (struct ver_ctx *) ctx;
	struct apk_installed_package *ipkg;
	struct apk_name *name;
	int i, j, ret = 0;


	if (ictx->action != NULL)
		return ictx->action(db, argc, argv);

	if (apk_verbosity > 0)
		printf("%-42sAvailable:\n", "Installed:");

	if (argc == 0) {
		list_for_each_entry(ipkg, &db->installed.packages,
				    installed_pkgs_list) {
			ver_print_package_status(ipkg->pkg, ictx->limchars);
		}
		goto ver_exit;
	}

	for (i = 0; i < argc; i++) {
		name = apk_db_query_name(db, APK_BLOB_STR(argv[i]));
		if (name == NULL) {
			apk_error("Not found: %s", name);
			ret = 1;
			goto ver_exit;
		}
		for (j = 0; j < name->pkgs->num; j++) {
			struct apk_package *pkg = name->pkgs->item[j];
			if (pkg->ipkg != NULL)
				ver_print_package_status(pkg, ictx->limchars);
		}
	}

ver_exit:
	return ret;
}

static struct apk_option ver_options[] = {
	{ 'I', "indexes",	"Print description and versions of indexes" },
	{ 't', "test",		"Compare two given versions" },
	{ 'c', "check", 	"Check if the given version string is valid" },
	{ 'l', "limit",		"Limit output to packages whos status matches LIMCHAR",
	  required_argument, "LIMCHAR" },
};

static struct apk_applet apk_ver = {
	.name = "version",
	.help = "Compare package versions (in installed database vs. available)"
		" or do tests on version strings given on command line.",
	.open_flags = APK_OPENF_READ,
	.context_size = sizeof(struct ver_ctx),
	.num_options = ARRAY_SIZE(ver_options),
	.options = ver_options,
	.parse = ver_parse,
	.main = ver_main,
};

APK_DEFINE_APPLET(apk_ver);

