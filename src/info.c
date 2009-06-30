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
	int subaction_mask;
};

struct info_subaction {
	int mask;
	void (*subaction)(struct apk_package *pkg);
};

static void info_print_depends(struct apk_package *pkg);
static void info_print_url(struct apk_package *pkg);
static void info_print_required_by(struct apk_package *pkg);
static void info_print_size(struct apk_package *pkg);
static void info_print_contents(struct apk_package *pkg);
static void info_print_description(struct apk_package *pkg);

#define APK_INFO_URL		0x01
#define APK_INFO_DEPENDS	0x02
#define APK_INFO_RDEPENDS	0x04
#define APK_INFO_SIZE		0x08
#define APK_INFO_CONTENTS	0x10
#define APK_INFO_DESC		0x20

#define APK_INFO_NUM_SUBACTIONS 6
static struct info_subaction info_package_actions[] = {
	{ APK_INFO_DESC,	info_print_description },
	{ APK_INFO_URL,		info_print_url },
	{ APK_INFO_SIZE,	info_print_size },
	{ APK_INFO_CONTENTS,	info_print_contents },
	{ APK_INFO_DEPENDS,	info_print_depends },
	{ APK_INFO_RDEPENDS,	info_print_required_by },
};

static void verbose_print_pkg(struct apk_package *pkg, int minimal_verbosity)
{
	int verbosity = apk_verbosity;
	if (verbosity < minimal_verbosity)
		verbosity = minimal_verbosity;

	if (pkg == NULL || verbosity < 1)
		return;

	printf("%s", pkg->name->name);
	if (apk_verbosity > 1)
		printf("-%s", pkg->version);
	if (apk_verbosity > 2)
		printf(" - %s", pkg->description);
	printf("\n");
}


static int info_list(struct info_ctx *ctx, struct apk_database *db,
		     int argc, char **argv)
{
	struct apk_package *pkg;

	list_for_each_entry(pkg, &db->installed.packages, installed_pkgs_list)
		verbose_print_pkg(pkg, 1);
	return 0;
}

static int info_exists(struct info_ctx *ctx, struct apk_database *db,
		       int argc, char **argv)
{
	struct apk_name *name;
	int i, j, ret = 0;

	for (i = 0; i < argc; i++) {
		name = apk_db_query_name(db, APK_BLOB_STR(argv[i]));
		if (name == NULL) {
			ret++;
			continue;
		}

		for (j = 0; j < name->pkgs->num; j++) {
			if (apk_pkg_get_state(name->pkgs->item[j]) == APK_PKG_INSTALLED)
				break;
		}
		if (j >= name->pkgs->num) {
			ret++;
		} else
			verbose_print_pkg(name->pkgs->item[j], 0);
	}

	return ret;
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

static void info_subaction(struct info_ctx *ctx, struct apk_package *pkg)
{
	int i;
	for (i = 0; i < APK_INFO_NUM_SUBACTIONS; i++)
		if (info_package_actions[i].mask & ctx->subaction_mask) {
			info_package_actions[i].subaction(pkg);
			puts("");
		}
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
				info_subaction(ctx, pkg);
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
}

static void info_print_url(struct apk_package *pkg)
{
	if (apk_verbosity > 1)
		printf("%s: %s", pkg->name->name, pkg->url);
	else
		printf("%s-%s webpage:\n%s\n", pkg->name->name, pkg->version,
		       pkg->url);
}

static void info_print_size(struct apk_package *pkg)
{
	if (apk_verbosity > 1)
		printf("%s: %zu", pkg->name->name, pkg->installed_size);
	else
		printf("%s-%s installed size:\n%zu\n", pkg->name->name, pkg->version,
		       pkg->installed_size);
}

static void info_print_description(struct apk_package *pkg)
{
	if (apk_verbosity > 1)
		printf("%s: %s", pkg->name->name, pkg->description);
	else
		printf("%s-%s description:\n%s\n", pkg->name->name,
		       pkg->version, pkg->description);
}
static int info_parse(void *ctx, int optch, int optindex, const char *optarg)
{
	struct info_ctx *ictx = (struct info_ctx *) ctx;

	ictx->action = info_package;
	switch (optch) {
	case 'e':
		ictx->action = info_exists;
		break;
	case 'W':
		ictx->action = info_who_owns;
		break;
	case 'w':
		ictx->subaction_mask |= APK_INFO_URL;
		break;
	case 'L':
		ictx->subaction_mask |= APK_INFO_CONTENTS;
		break;
	case 'R':
		ictx->subaction_mask |= APK_INFO_DEPENDS;
		break;
	case 'r':
		ictx->subaction_mask |= APK_INFO_RDEPENDS;
		break;
	case 's':
		ictx->subaction_mask |= APK_INFO_SIZE;
		break;
	case 'd':
		ictx->subaction_mask |= APK_INFO_DESC;
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

static struct apk_option info_options[] = {
	{ 'L', "contents",	"List contents of the PACKAGE" },
	{ 'e', "installed",	"Check if PACKAGE is installed" },
	{ 'W', "who-owns",	"Print the package owning the specified file" },
	{ 'R', "depends",	"List packages that the PACKAGE depends on" },
	{ 'r', "rdepends",	"List all packages depending on PACKAGE" },
	{ 'w', "webpage",	"Show URL for more information about PACKAGE" },
	{ 's', "size",		"Show installed size of PACKAGE" },
	{ 'd', "description",	"Print description for PACKAGE" },
};

static struct apk_applet apk_info = {
	.name = "info",
	.help = "Give detailed information about PACKAGEs.",
	.arguments = "PACKAGE...",
	.context_size = sizeof(struct info_ctx),
	.num_options = ARRAY_SIZE(info_options),
	.options = info_options,
	.parse = info_parse,
	.main = info_main,
};

APK_DEFINE_APPLET(apk_info);

