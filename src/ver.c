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

struct ver_ctx {
	int (*action)(int argc, char **argv);
};

static int res2char(int res)
{
	switch (res) {
	case APK_VERSION_LESS:
		return '<';
	case APK_VERSION_GREATER:
		return '>';
	case APK_VERSION_EQUAL:
		return '=';
	default:
		return '?';
	}
}

static int ver_test(int argc, char **argv)
{
	int r;

	if (argc != 2)
		return 1;

	r = apk_version_compare(argv[0], argv[1]);
	printf("%c\n", res2char(r));
	return 0;
}

static int ver_validate(int argc, char **argv)
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
	
static int ver_parse(void *ctx, int opt, int optindex, const char *optarg)
{
	struct ver_ctx *ictx = (struct ver_ctx *) ctx;
	switch (opt) {
	case 't':
		ictx->action = ver_test;
		break;
	case 'c':
		ictx->action = ver_validate;
		break;
	default:
		return -1;
	}
	return 0;
}

static void ver_print_package_status(struct apk_package *pkg)
{
	struct apk_name *name;
	struct apk_package *latest, *tmp;
	int i, r;

	name = pkg->name;
	latest = pkg;
	for (i = 0; i < name->pkgs->num; i++) {
		tmp = name->pkgs->item[i];
		if (tmp->name != name || 
		    apk_pkg_get_state(tmp) == APK_PKG_INSTALLED)
			continue;
		r = apk_pkg_version_compare(tmp, latest);
		if (r == APK_VERSION_GREATER)
			latest = tmp;
	}
	r = apk_pkg_version_compare(pkg, latest);
	printf("%s-%-40s%s %s\n", name->name, pkg->version, apk_version_op_string(r), latest->version);
}

static int ver_main(void *ctx, int argc, char **argv)
{
	struct ver_ctx *ictx = (struct ver_ctx *) ctx;
	struct apk_database db;
	struct apk_package *pkg;
	struct apk_name *name;
	int i, j, ret = 0;


	if (ictx->action != NULL)
		return ictx->action(argc, argv);

	if (apk_db_open(&db, apk_root, APK_OPENF_READ) < 0)
		return -1;

	if (argc == 0) {
		list_for_each_entry(pkg, &db.installed.packages,
				    installed_pkgs_list) {
			ver_print_package_status(pkg);
		}
		goto ver_exit;
	}

	for (i = 0; i < argc; i++) {
		name = apk_db_query_name(&db, APK_BLOB_STR(argv[i]));
		if (name == NULL) {
			apk_error("Not found: %s", name);
			ret = 1;
			goto ver_exit;
		}
		for (j = 0; j < name->pkgs->num; j++) {
			struct apk_package *pkg = name->pkgs->item[j];
			if (apk_pkg_get_state(pkg) == APK_PKG_INSTALLED)
				ver_print_package_status(pkg);
		}
	}

ver_exit:
	apk_db_close(&db);
	return ret;
}

static struct option ver_options[] = {
	{ "test", 	no_argument,	NULL, 't' },
	{ "check", 	no_argument,	NULL, 'c' },
};

static struct apk_applet apk_ver = {
	.name = "version",
	.usage = "[-t|--test version1 version2] [-c|--check]",
	.context_size = sizeof(struct ver_ctx),
	.num_options = ARRAY_SIZE(ver_options),
	.options = ver_options,
	.parse = ver_parse,
	.main = ver_main,
};

APK_DEFINE_APPLET(apk_ver);

