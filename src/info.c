/* info.c - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2005-2009 Natanael Copa <n@tanael.org>
 * Copyright (C) 2008-2011 Timo Teräs <timo.teras@iki.fi>
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
#include "apk_print.h"

struct info_ctx {
	struct apk_database *db;
	int (*action)(struct info_ctx *ctx, struct apk_database *db,
		      int argc, char **argv);
	int subaction_mask;
};

/* These need to stay in sync with the function pointer array in
 * info_subaction() */
#define APK_INFO_DESC		0x01
#define APK_INFO_URL		0x02
#define APK_INFO_SIZE		0x04
#define APK_INFO_DEPENDS	0x08
#define APK_INFO_RDEPENDS	0x10
#define APK_INFO_CONTENTS	0x20
#define APK_INFO_TRIGGERS	0x40
#define APK_INFO_INSTALL_IF	0x80
#define APK_INFO_RINSTALL_IF	0x100
#define APK_INFO_REPLACES	0x200

static void verbose_print_pkg(struct apk_package *pkg, int minimal_verbosity)
{
	int verbosity = apk_verbosity;
	if (verbosity < minimal_verbosity)
		verbosity = minimal_verbosity;

	if (pkg == NULL || verbosity < 1)
		return;

	printf("%s", pkg->name->name);
	if (apk_verbosity > 1)
		printf("-" BLOB_FMT, BLOB_PRINTF(*pkg->version));
	if (apk_verbosity > 2)
		printf(" - %s", pkg->description);
	printf("\n");
}


static int info_list(struct info_ctx *ctx, struct apk_database *db,
		     int argc, char **argv)
{
	struct apk_installed_package *ipkg;

	list_for_each_entry(ipkg, &db->installed.packages, installed_pkgs_list)
		verbose_print_pkg(ipkg->pkg, 1);
	return 0;
}

static int info_exists(struct info_ctx *ctx, struct apk_database *db,
		       int argc, char **argv)
{
	struct apk_name *name;
	struct apk_package *pkg = NULL;
	struct apk_dependency dep;
	int i, j, ok = 0;

	for (i = 0; i < argc; i++) {
		apk_blob_t b = APK_BLOB_STR(argv[i]);
		apk_blob_pull_dep(&b, db, &dep);
		name = dep.name;
		if (name == NULL)
			continue;

		for (j = 0; j < name->pkgs->num; j++) {
			pkg = name->pkgs->item[j];
			if (pkg->ipkg != NULL)
				break;
		}
		if (j >= name->pkgs->num)
			continue;

		if (!apk_dep_is_satisfied(&dep, pkg))
			continue;

		verbose_print_pkg(pkg, 0);
		ok++;
	}

	return argc - ok;
}

static int info_who_owns(struct info_ctx *ctx, struct apk_database *db,
			 int argc, char **argv)
{
	struct apk_package *pkg;
	struct apk_dependency_array *deps;
	struct apk_dependency dep;
	int i, r=0;

	apk_dependency_array_init(&deps);
	for (i = 0; i < argc; i++) {
		pkg = apk_db_get_file_owner(db, APK_BLOB_STR(argv[i]));
		if (pkg == NULL) {
			apk_error("%s: Could not find owner package", argv[i]);
			r++;
			continue;
		}

		if (apk_verbosity < 1) {
			dep = (struct apk_dependency) {
				.name = pkg->name,
				.version = apk_blob_atomize(APK_BLOB_NULL),
				.result_mask = APK_DEPMASK_REQUIRE,
			};
			apk_deps_add(&deps, &dep);
		} else {
			printf("%s is owned by " PKG_VER_FMT "\n",
			       argv[i], PKG_VER_PRINTF(pkg));
		}
	}
	if (apk_verbosity < 1 && deps->num != 0) {
		struct apk_ostream *os;

		os = apk_ostream_to_fd(STDOUT_FILENO);
		apk_deps_write(db, deps, os);
		os->write(os, "\n", 1);
		os->close(os);
	}
	apk_dependency_array_free(&deps);

	return r;
}

static void info_print_description(struct apk_database *db, struct apk_package *pkg)
{
	if (apk_verbosity > 1)
		printf("%s: %s", pkg->name->name, pkg->description);
	else
		printf(PKG_VER_FMT " description:\n%s\n",
		       PKG_VER_PRINTF(pkg),
		       pkg->description);
}

static void info_print_url(struct apk_database *db, struct apk_package *pkg)
{
	if (apk_verbosity > 1)
		printf("%s: %s", pkg->name->name, pkg->url);
	else
		printf(PKG_VER_FMT " webpage:\n%s\n",
		       PKG_VER_PRINTF(pkg),
		       pkg->url);
}

static void info_print_size(struct apk_database *db, struct apk_package *pkg)
{
	if (apk_verbosity > 1)
		printf("%s: %zu", pkg->name->name, pkg->installed_size);
	else
		printf(PKG_VER_FMT " installed size:\n%zu\n",
		       PKG_VER_PRINTF(pkg),
		       pkg->installed_size);
}

static void info_print_depends(struct apk_database *db, struct apk_package *pkg)
{
	apk_blob_t separator = APK_BLOB_STR(apk_verbosity > 1 ? " " : "\n");
	char dep[256];
	int i;

	if (apk_verbosity == 1)
		printf(PKG_VER_FMT " depends on:\n",
		       PKG_VER_PRINTF(pkg));
	if (apk_verbosity > 1)
		printf("%s: ", pkg->name->name);
	for (i = 0; i < pkg->depends->num; i++) {
		apk_blob_t b = APK_BLOB_BUF(dep);
		apk_blob_push_dep(&b, db, &pkg->depends->item[i]);
		apk_blob_push_blob(&b, separator);
		b = apk_blob_pushed(APK_BLOB_BUF(dep), b);
		fwrite(b.ptr, b.len, 1, stdout);
	}
}

static void info_print_required_by(struct apk_database *db, struct apk_package *pkg)
{
	int i, j, k;
	char *separator = apk_verbosity > 1 ? " " : "\n";

	if (apk_verbosity == 1)
		printf(PKG_VER_FMT " is required by:\n",
		       PKG_VER_PRINTF(pkg));
	if (apk_verbosity > 1)
		printf("%s: ", pkg->name->name);
	for (i = 0; i < pkg->name->rdepends->num; i++) {
		struct apk_name *name0 = pkg->name->rdepends->item[i];

		/* Check only the package that is installed, and that
		 * it actually has this package as dependency. */
		for (j = 0; j < name0->pkgs->num; j++) {
			struct apk_package *pkg0 = name0->pkgs->item[j];

			if (pkg0->ipkg == NULL)
				continue;
			for (k = 0; k < pkg0->depends->num; k++) {
				if (pkg0->depends->item[k].name != pkg->name)
					continue;
				printf(PKG_VER_FMT "%s",
				       PKG_VER_PRINTF(pkg0),
				       separator);
				break;
			}
		}
	}
}

static void info_print_install_if(struct apk_database *db, struct apk_package *pkg)
{
	apk_blob_t separator = APK_BLOB_STR(apk_verbosity > 1 ? " " : "\n");
	char dep[256];
	int i;

	if (apk_verbosity == 1)
		printf(PKG_VER_FMT " has auto-install rule:\n",
		       PKG_VER_PRINTF(pkg));
	if (apk_verbosity > 1)
		printf("%s: ", pkg->name->name);
	for (i = 0; i < pkg->install_if->num; i++) {
		apk_blob_t b = APK_BLOB_BUF(dep);
		apk_blob_push_dep(&b, db, &pkg->install_if->item[i]);
		apk_blob_push_blob(&b, separator);
		b = apk_blob_pushed(APK_BLOB_BUF(dep), b);
		fwrite(b.ptr, b.len, 1, stdout);
	}
}

static void info_print_rinstall_if(struct apk_database *db, struct apk_package *pkg)
{
	int i, j, k;
	char *separator = apk_verbosity > 1 ? " " : "\n";

	if (apk_verbosity == 1)
		printf(PKG_VER_FMT " affects auto-installation of:\n",
		       PKG_VER_PRINTF(pkg));
	if (apk_verbosity > 1)
		printf("%s: ", pkg->name->name);
	for (i = 0; i < pkg->name->rinstall_if->num; i++) {
		struct apk_name *name0 = pkg->name->rinstall_if->item[i];

		/* Check only the package that is installed, and that
		 * it actually has this package in install_if. */
		for (j = 0; j < name0->pkgs->num; j++) {
			struct apk_package *pkg0 = name0->pkgs->item[j];

			if (pkg0->ipkg == NULL)
				continue;
			for (k = 0; k < pkg0->install_if->num; k++) {
				if (pkg0->install_if->item[k].name != pkg->name)
					continue;
				printf(PKG_VER_FMT "%s",
				       PKG_VER_PRINTF(pkg0),
				       separator);
				break;
			}
		}
	}
}

static void info_print_contents(struct apk_database *db, struct apk_package *pkg)
{
	struct apk_installed_package *ipkg = pkg->ipkg;
	struct apk_db_dir_instance *diri;
	struct apk_db_file *file;
	struct hlist_node *dc, *dn, *fc, *fn;

	if (apk_verbosity == 1)
		printf(PKG_VER_FMT " contains:\n",
		       PKG_VER_PRINTF(pkg));

	hlist_for_each_entry_safe(diri, dc, dn, &ipkg->owned_dirs,
				  pkg_dirs_list) {
		hlist_for_each_entry_safe(file, fc, fn, &diri->owned_files,
					  diri_files_list) {
			if (apk_verbosity > 1)
				printf("%s: ", pkg->name->name);
			printf("%s/%s\n", diri->dir->name, file->name);
		}
	}
}

static void info_print_triggers(struct apk_database *db, struct apk_package *pkg)
{
	struct apk_installed_package *ipkg = pkg->ipkg;
	int i;

	if (apk_verbosity == 1)
		printf(PKG_VER_FMT " triggers:\n",
		       PKG_VER_PRINTF(pkg));

	for (i = 0; i < ipkg->triggers->num; i++) {
		if (apk_verbosity > 1)
			printf("%s: trigger ", pkg->name->name);
		printf("%s\n", ipkg->triggers->item[i]);
	}
}

static void info_print_replaces(struct apk_database *db, struct apk_package *pkg)
{
	apk_blob_t separator = APK_BLOB_STR(apk_verbosity > 1 ? " " : "\n");
	char dep[256];
	int i;

	if (apk_verbosity == 1)
		printf(PKG_VER_FMT " replaces:\n",
		       PKG_VER_PRINTF(pkg));
	if (apk_verbosity > 1)
		printf("%s: ", pkg->name->name);
	for (i = 0; i < pkg->ipkg->replaces->num; i++) {
		apk_blob_t b = APK_BLOB_BUF(dep);
		apk_blob_push_dep(&b, db, &pkg->ipkg->replaces->item[i]);
		apk_blob_push_blob(&b, separator);
		b = apk_blob_pushed(APK_BLOB_BUF(dep), b);
		fwrite(b.ptr, b.len, 1, stdout);
	}
}

static void info_subaction(struct info_ctx *ctx, struct apk_package *pkg)
{
	typedef void (*subaction_t)(struct apk_database *, struct apk_package *);
	static subaction_t subactions[] = {
		info_print_description,
		info_print_url,
		info_print_size,
		info_print_depends,
		info_print_required_by,
		info_print_contents,
		info_print_triggers,
		info_print_install_if,
		info_print_rinstall_if,
		info_print_replaces,
	};
	const int requireipkg =
		APK_INFO_CONTENTS | APK_INFO_TRIGGERS | APK_INFO_RDEPENDS |
		APK_INFO_RINSTALL_IF | APK_INFO_REPLACES;
	int i;

	for (i = 0; i < ARRAY_SIZE(subactions); i++) {
		if (!(BIT(i) & ctx->subaction_mask))
			continue;

		if (pkg->ipkg == NULL && (BIT(i) & requireipkg))
			continue;

		subactions[i](ctx->db, pkg);
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
		if (name == NULL || name->pkgs->num == 0) {
			apk_error("Not found: %s", argv[i]);
			return 1;
		}
		for (j = 0; j < name->pkgs->num; j++)
			info_subaction(ctx, name->pkgs->item[j]);
	}
	return 0;
}

static int info_parse(void *ctx, struct apk_db_options *dbopts,
		      int optch, int optindex, const char *optarg)
{
	struct info_ctx *ictx = (struct info_ctx *) ctx;

	ictx->action = info_package;
	switch (optch) {
	case 'e':
		ictx->action = info_exists;
		dbopts->open_flags |= APK_OPENF_NO_REPOS;
		break;
	case 'W':
		ictx->action = info_who_owns;
		dbopts->open_flags |= APK_OPENF_NO_REPOS;
		break;
	case 'w':
		ictx->subaction_mask |= APK_INFO_URL;
		break;
	case 'R':
		ictx->subaction_mask |= APK_INFO_DEPENDS;
		break;
	case 'r':
		ictx->subaction_mask |= APK_INFO_RDEPENDS;
		break;
	case 'I':
		ictx->subaction_mask |= APK_INFO_INSTALL_IF;
		break;
	case 'i':
		ictx->subaction_mask |= APK_INFO_RINSTALL_IF;
		break;
	case 's':
		ictx->subaction_mask |= APK_INFO_SIZE;
		break;
	case 'd':
		ictx->subaction_mask |= APK_INFO_DESC;
		break;
	case 'L':
		ictx->subaction_mask |= APK_INFO_CONTENTS;
		break;
	case 't':
		ictx->subaction_mask |= APK_INFO_TRIGGERS;
		break;
	case 0x10000:
		ictx->subaction_mask |= APK_INFO_REPLACES;
		break;
	case 'a':
		ictx->subaction_mask = 0xffffffff;
		break;
	default:
		return -1;
	}
	return 0;
}

static int info_package_short(struct info_ctx *ictx, struct apk_database *db,
			      int argc, char **argv)
{
	ictx->subaction_mask |= APK_INFO_DESC | APK_INFO_URL | APK_INFO_SIZE;
	return info_package(ictx, db, argc, argv);
}

static int info_main(void *ctx, struct apk_database *db, int argc, char **argv)
{
	struct info_ctx *ictx = (struct info_ctx *) ctx;

	ictx->db = db;
	if (ictx->action != NULL)
		return ictx->action(ictx, db, argc, argv);

	if (argc > 0)
		return info_package_short(ictx, db, argc, argv);

	return info_list(ictx, db, argc, argv);
}

static struct apk_option info_options[] = {
	{ 'L', "contents",	"List contents of the PACKAGE" },
	{ 'e', "installed",	"Check if PACKAGE is installed" },
	{ 'W', "who-owns",	"Print the package owning the specified file" },
	{ 'R', "depends",	"List packages that the PACKAGE depends on" },
	{ 'r', "rdepends",	"List all packages depending on PACKAGE" },
	{ 0x10000, "replaces",	"List packages whom files PACKAGE might replace" },
	{ 'i', "install-if",	"List the PACKAGE's install-if rule" },
	{ 'I', "rinstall-if",	"List all packages having install-if referencing PACKAGE" },
	{ 'w', "webpage",	"Show URL for more information about PACKAGE" },
	{ 's', "size",		"Show installed size of PACKAGE" },
	{ 'd', "description",	"Print description for PACKAGE" },
	{ 't', "triggers",	"Print active triggers of PACKAGE" },
	{ 'a', "all",		"Print all information about PACKAGE" },
};

static struct apk_applet apk_info = {
	.name = "info",
	.help = "Give detailed information about PACKAGEs or repositores.",
	.arguments = "PACKAGE...",
	.open_flags = APK_OPENF_READ,
	.context_size = sizeof(struct info_ctx),
	.num_options = ARRAY_SIZE(info_options),
	.options = info_options,
	.parse = info_parse,
	.main = info_main,
};

APK_DEFINE_APPLET(apk_info);

