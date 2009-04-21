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

#include <stdio.h>
#include <unistd.h>
#include "apk_defines.h"
#include "apk_applet.h"
#include "apk_package.h"
#include "apk_database.h"
#include "apk_state.h"

struct info_ctx {
	int (*action)(struct info_ctx *ctx, struct apk_database *db,
		      int argc, char **argv);
	void (*subaction)(struct apk_package *pkg);
};

static int info_list(struct info_ctx *ctx, struct apk_database *db,
		     int argc, char **argv)
{
	struct apk_package *pkg;

	list_for_each_entry(pkg, &db->installed.packages, installed_pkgs_list) {
		printf("%s", pkg->name->name);
		if (apk_verbosity > 0)
			printf("-%s", pkg->version);
		if (apk_verbosity > 1)
			printf("- %s", pkg->description);
		printf("\n");
	}
	return 0;
}

static int info_exists(struct info_ctx *ctx, struct apk_database *db,
		       int argc, char **argv)
{
	struct apk_name *name;
	int i, j;

	for (i = 0; i < argc; i++) {
		name = apk_db_query_name(db, APK_BLOB_STR(argv[i]));
		if (name == NULL)
			return 1;

		for (j = 0; j < name->pkgs->num; j++) {
			if (apk_pkg_get_state(name->pkgs->item[j]) == APK_PKG_INSTALLED)
				break;
		}
		if (j >= name->pkgs->num)
			return 2;
	}

	return 0;
}

static int info_who_owns(struct info_ctx *ctx, struct apk_database *db,
			 int argc, char **argv)
{
	struct apk_package *pkg;
	struct apk_dependency_array *deps = NULL;
	struct apk_dependency dep;
	int i;

	for (i = 0; i < argc; i++) {
		pkg = apk_db_get_file_owner(db, APK_BLOB_STR(argv[i]));
		if (pkg == NULL)
			continue;

		if (apk_verbosity < 1) {
			dep = (struct apk_dependency) {
				.name = pkg->name,
				.result_mask = APK_DEPMASK_REQUIRE,
			};
			apk_deps_add(&deps, &dep);
		} else {
			printf("%s is owned by %s-%s\n", argv[i],
			       pkg->name->name, pkg->version);
		}
	}
	if (apk_verbosity < 1 && deps != NULL) {
		struct apk_ostream *os;

		os = apk_ostream_to_fd(STDOUT_FILENO);
		apk_deps_write(deps, os);
		os->write(os, "\n", 1);
		os->close(os);

		free(deps);
	}

	return 0;
}

static int info_package(struct info_ctx *ctx, struct apk_database *db,
			int argc, char **argv)
{
	struct apk_name *name;
	int i, j;

	for (i = 0; i < argc; i++) {
		name = apk_db_query_name(db, APK_BLOB_STR(argv[i]));
		if (name == NULL) {
			apk_error("Not found: %s", name);
			return 1;
		}
		for (j = 0; j < name->pkgs->num; j++) {
			struct apk_package *pkg = name->pkgs->item[j];
			if (apk_pkg_get_state(pkg) == APK_PKG_INSTALLED)
				ctx->subaction(pkg);
		}
	}
	return 0;
}

static void info_print_contents(struct apk_package *pkg)
{
	struct apk_db_dir_instance *diri;
	struct apk_db_file *file;
	struct hlist_node *dc, *dn, *fc, *fn;

	if (apk_verbosity == 1)
		printf("%s-%s contains:\n", pkg->name->name, pkg->version);

	hlist_for_each_entry_safe(diri, dc, dn, &pkg->owned_dirs,
				  pkg_dirs_list) {
		hlist_for_each_entry_safe(file, fc, fn, &diri->owned_files,
					  diri_files_list) {
			if (apk_verbosity > 1)
				printf("%s: ", pkg->name->name);
			printf("%s/%s\n", diri->dir->dirname, file->filename);
		}
	}
	puts("");
}

static void info_print_depends(struct apk_package *pkg)
{
	int i;
	char *separator = apk_verbosity > 1 ? " " : "\n";
	if (apk_verbosity == 1) 
		printf("%s-%s depends on:\n", pkg->name->name, pkg->version);
	if (pkg->depends == NULL)
		return;
	if (apk_verbosity > 1)
		printf("%s: ", pkg->name->name);
	for (i = 0; i < pkg->depends->num; i++) {
		printf("%s%s", pkg->depends->item[i].name->name, separator);
	}
	puts("");
}

static void info_print_required_by(struct apk_package *pkg)
{
	int i, j, k;
	char *separator = apk_verbosity > 1 ? " " : "\n";

	if (apk_verbosity == 1)
		printf("%s-%s is required by:\n", pkg->name->name, pkg->version);
	if (pkg->name->rdepends == NULL)
		return;
	if (apk_verbosity > 1)
		printf("%s: ", pkg->name->name);
	for (i = 0; i < pkg->name->rdepends->num; i++) {
		struct apk_name *name0 = pkg->name->rdepends->item[i];

		/* Check only the package that is installed, and that
		 * it actually has this package as dependency. */
		if (name0->pkgs == NULL)
			continue;
		for (j = 0; j < name0->pkgs->num; j++) {
			struct apk_package *pkg0 = name0->pkgs->item[j];

			if (apk_pkg_get_state(pkg0) != APK_PKG_INSTALLED ||
			    pkg0->depends == NULL)
				continue;
			for (k = 0; k < pkg0->depends->num; k++) {
				if (pkg0->depends->item[k].name != pkg->name)
					continue;
				printf("%s-%s%s", pkg0->name->name,
				       pkg0->version, separator);
				break;
			}
		}
	}
	puts("");
}

static int info_parse(void *ctx, int optch, int optindex, const char *optarg)
{
	struct info_ctx *ictx = (struct info_ctx *) ctx;

	switch (optch) {
	case 'e':
		ictx->action = info_exists;
		break;
	case 'W':
		ictx->action = info_who_owns;
		break;
	case 'L':
		ictx->action = info_package;
		ictx->subaction = info_print_contents;
		break;
	case 'R':
		ictx->action = info_package;
		ictx->subaction = info_print_depends;
		break;
	case 'r':
		ictx->action = info_package;
		ictx->subaction = info_print_required_by;
		break;
	default:
		return -1;
	}
	return 0;
}

static int info_main(void *ctx, int argc, char **argv)
{
	struct info_ctx *ictx = (struct info_ctx *) ctx;
	struct apk_database db;
	int r;

	if (apk_db_open(&db, apk_root, APK_OPENF_READ + APK_OPENF_EMPTY_REPOS) < 0)
		return -1;

	if (ictx->action != NULL)
		r = ictx->action(ictx, &db, argc, argv);
	else
		r = info_list(ictx, &db, argc, argv);

	apk_db_close(&db);
	return r;
}

static struct option info_options[] = {
	{ "contents",	no_argument,		NULL, 'L' },
	{ "installed",	no_argument,		NULL, 'e' },
	{ "who-owns",	no_argument,		NULL, 'W' },
	{ "depends",	no_argument,		NULL, 'R' },
	{ "rdepends",	no_argument,		NULL, 'r' },
};

static struct apk_applet apk_info = {
	.name = "info",
	.usage = "",
	.context_size = sizeof(struct info_ctx),
	.num_options = ARRAY_SIZE(info_options),
	.options = info_options,
	.parse = info_parse,
	.main = info_main,
};

APK_DEFINE_APPLET(apk_info);

