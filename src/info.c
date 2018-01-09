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
#include <limits.h>
#include "apk_defines.h"
#include "apk_applet.h"
#include "apk_package.h"
#include "apk_database.h"
#include "apk_print.h"

struct info_ctx {
	struct apk_database *db;
	void (*action)(struct info_ctx *ctx, struct apk_database *db, struct apk_string_array *args);
	int subaction_mask;
	int errors;
};

/* These need to stay in sync with the function pointer array in
 * info_subaction() */
#define APK_INFO_DESC		0x01
#define APK_INFO_URL		0x02
#define APK_INFO_SIZE		0x04
#define APK_INFO_DEPENDS	0x08
#define APK_INFO_PROVIDES	0x10
#define APK_INFO_RDEPENDS	0x20
#define APK_INFO_CONTENTS	0x40
#define APK_INFO_TRIGGERS	0x80
#define APK_INFO_INSTALL_IF	0x100
#define APK_INFO_RINSTALL_IF	0x200
#define APK_INFO_REPLACES	0x400
#define APK_INFO_LICENSE	0x800

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

static void info_exists(struct info_ctx *ctx, struct apk_database *db,
			struct apk_string_array *args)
{
	struct apk_name *name;
	struct apk_dependency dep;
	struct apk_provider *p;
	char **parg;
	int ok;

	foreach_array_item(parg, args) {
		apk_blob_t b = APK_BLOB_STR(*parg);

		apk_blob_pull_dep(&b, db, &dep);
		if (APK_BLOB_IS_NULL(b) || b.len > 0)
			continue;

		name = dep.name;
		if (name == NULL)
			continue;

		ok = apk_dep_is_provided(&dep, NULL);
		foreach_array_item(p, name->providers) {
			if (!p->pkg->ipkg) continue;
			ok = apk_dep_is_provided(&dep, p);
			if (ok) verbose_print_pkg(p->pkg, 0);
			break;
		}
		if (!ok) ctx->errors++;
	}
}

static void info_who_owns(struct info_ctx *ctx, struct apk_database *db,
			  struct apk_string_array *args)
{
	struct apk_package *pkg;
	struct apk_dependency_array *deps;
	struct apk_dependency dep;
	struct apk_ostream *os;
	const char *via;
	char **parg, fnbuf[PATH_MAX], buf[PATH_MAX];
	apk_blob_t fn;
	ssize_t r;

	apk_dependency_array_init(&deps);
	foreach_array_item(parg, args) {
		if (*parg[0] != '/' && realpath(*parg, fnbuf))
			fn = APK_BLOB_STR(fnbuf);
		else
			fn = APK_BLOB_STR(*parg);

		via = "";
		pkg = apk_db_get_file_owner(db, fn);
		if (pkg == NULL) {
			r = readlinkat(db->root_fd, *parg, buf, sizeof(buf));
			if (r > 0 && r < PATH_MAX && buf[0] == '/') {
				pkg = apk_db_get_file_owner(db, APK_BLOB_STR(buf));
				via = "symlink target ";
			}
		}

		if (pkg == NULL) {
			apk_error(BLOB_FMT ": Could not find owner package",
				  BLOB_PRINTF(fn));
			ctx->errors++;
			continue;
		}

		if (apk_verbosity < 1) {
			dep = (struct apk_dependency) {
				.name = pkg->name,
				.version = apk_blob_atomize(APK_BLOB_NULL),
				.result_mask = APK_DEPMASK_ANY,
			};
			apk_deps_add(&deps, &dep);
		} else {
			printf(BLOB_FMT " %sis owned by " PKG_VER_FMT "\n",
			       BLOB_PRINTF(fn), via, PKG_VER_PRINTF(pkg));
		}
	}
	if (apk_verbosity < 1 && deps->num != 0) {
		os = apk_ostream_to_fd(STDOUT_FILENO);
		if (!IS_ERR_OR_NULL(os)) {
			apk_deps_write(db, deps, os, APK_BLOB_PTR_LEN(" ", 1));
			apk_ostream_write(os, "\n", 1);
			apk_ostream_close(os);
		}
	}
	apk_dependency_array_free(&deps);
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

static void info_print_license(struct apk_database *db, struct apk_package *pkg)
{
	if (apk_verbosity > 1)
		printf("%s: " BLOB_FMT , pkg->name->name, BLOB_PRINTF(*pkg->license));
	else
		printf(PKG_VER_FMT " license:\n" BLOB_FMT "\n",
		       PKG_VER_PRINTF(pkg),
		       BLOB_PRINTF(*pkg->license));
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

static void info_print_dep_array(struct apk_database *db, struct apk_package *pkg,
				 struct apk_dependency_array *deps, const char *dep_text)
{
	struct apk_dependency *d;
	apk_blob_t separator = APK_BLOB_STR(apk_verbosity > 1 ? " " : "\n");
	char buf[256];

	if (apk_verbosity == 1)
		printf(PKG_VER_FMT " %s:\n", PKG_VER_PRINTF(pkg), dep_text);
	if (apk_verbosity > 1)
		printf("%s: ", pkg->name->name);
	foreach_array_item(d, deps) {
		apk_blob_t b = APK_BLOB_BUF(buf);
		apk_blob_push_dep(&b, db, d);
		apk_blob_push_blob(&b, separator);
		b = apk_blob_pushed(APK_BLOB_BUF(buf), b);
		fwrite(b.ptr, b.len, 1, stdout);
	}
}

static void info_print_depends(struct apk_database *db, struct apk_package *pkg)
{
	info_print_dep_array(db, pkg, pkg->depends, "depends on");
}

static void info_print_provides(struct apk_database *db, struct apk_package *pkg)
{
	info_print_dep_array(db, pkg, pkg->provides, "provides");
}

static void print_rdep_pkg(struct apk_package *pkg0, struct apk_dependency *dep0, struct apk_package *pkg, void *pctx)
{
	printf(PKG_VER_FMT "%s", PKG_VER_PRINTF(pkg0), apk_verbosity > 1 ? " " : "\n");
}

static void info_print_required_by(struct apk_database *db, struct apk_package *pkg)
{
	if (apk_verbosity == 1)
		printf(PKG_VER_FMT " is required by:\n", PKG_VER_PRINTF(pkg));
	if (apk_verbosity > 1)
		printf("%s: ", pkg->name->name);
	apk_pkg_foreach_reverse_dependency(
		pkg,
		APK_FOREACH_INSTALLED | APK_DEP_SATISFIES | apk_foreach_genid(),
		print_rdep_pkg, NULL);
}

static void info_print_install_if(struct apk_database *db, struct apk_package *pkg)
{
	info_print_dep_array(db, pkg, pkg->install_if, "has auto-install rule");
}

static void info_print_rinstall_if(struct apk_database *db, struct apk_package *pkg)
{
	int i, j;
	char *separator = apk_verbosity > 1 ? " " : "\n";

	if (apk_verbosity == 1)
		printf(PKG_VER_FMT " affects auto-installation of:\n",
		       PKG_VER_PRINTF(pkg));
	if (apk_verbosity > 1)
		printf("%s: ", pkg->name->name);
	for (i = 0; i < pkg->name->rinstall_if->num; i++) {
		struct apk_name *name0;
		struct apk_package *pkg0;

		/* Check only the package that is installed, and that
		 * it actually has this package in install_if. */
		name0 = pkg->name->rinstall_if->item[i];
		pkg0 = apk_pkg_get_installed(name0);
		if (pkg0 == NULL)
			continue;

		for (j = 0; j < pkg0->install_if->num; j++) {
			if (pkg0->install_if->item[j].name != pkg->name)
				continue;
			printf(PKG_VER_FMT "%s",
			       PKG_VER_PRINTF(pkg0),
			       separator);
			break;
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
			printf(DIR_FILE_FMT "\n", DIR_FILE_PRINTF(diri->dir, file));
		}
	}
}

static void info_print_triggers(struct apk_database *db, struct apk_package *pkg)
{
	struct apk_installed_package *ipkg = pkg->ipkg;
	char **trigger;

	if (apk_verbosity == 1)
		printf(PKG_VER_FMT " triggers:\n",
		       PKG_VER_PRINTF(pkg));

	foreach_array_item(trigger, ipkg->triggers) {
		if (apk_verbosity > 1)
			printf("%s: trigger ", pkg->name->name);
		printf("%s\n", *trigger);
	}
}

static void info_print_replaces(struct apk_database *db, struct apk_package *pkg)
{
	info_print_dep_array(db, pkg, pkg->ipkg->replaces, "replaces");
}

static void info_subaction(struct info_ctx *ctx, struct apk_package *pkg)
{
	typedef void (*subaction_t)(struct apk_database *, struct apk_package *);
	static subaction_t subactions[] = {
		info_print_description,
		info_print_url,
		info_print_size,
		info_print_depends,
		info_print_provides,
		info_print_required_by,
		info_print_contents,
		info_print_triggers,
		info_print_install_if,
		info_print_rinstall_if,
		info_print_replaces,
		info_print_license,
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

static void print_name_info(struct apk_database *db, const char *match, struct apk_name *name, void *pctx)
{
	struct info_ctx *ctx = (struct info_ctx *) pctx;
	struct apk_provider *p;

	if (name == NULL) {
		ctx->errors++;
		return;
	}

	foreach_array_item(p, name->providers)
		info_subaction(ctx, p->pkg);
}

static int option_parse_applet(void *pctx, struct apk_db_options *dbopts, int optch, const char *optarg)
{
	struct info_ctx *ctx = (struct info_ctx *) pctx;

	ctx->action = NULL;
	switch (optch) {
	case 'e':
		ctx->action = info_exists;
		dbopts->open_flags |= APK_OPENF_NO_REPOS;
		break;
	case 'W':
		ctx->action = info_who_owns;
		dbopts->open_flags |= APK_OPENF_NO_REPOS;
		break;
	case 'w':
		ctx->subaction_mask |= APK_INFO_URL;
		break;
	case 'R':
		ctx->subaction_mask |= APK_INFO_DEPENDS;
		break;
	case 'P':
		ctx->subaction_mask |= APK_INFO_PROVIDES;
		break;
	case 'r':
		ctx->subaction_mask |= APK_INFO_RDEPENDS;
		break;
	case 'I':
		ctx->subaction_mask |= APK_INFO_INSTALL_IF;
		break;
	case 'i':
		ctx->subaction_mask |= APK_INFO_RINSTALL_IF;
		break;
	case 's':
		ctx->subaction_mask |= APK_INFO_SIZE;
		break;
	case 'd':
		ctx->subaction_mask |= APK_INFO_DESC;
		break;
	case 'L':
		ctx->subaction_mask |= APK_INFO_CONTENTS;
		break;
	case 't':
		ctx->subaction_mask |= APK_INFO_TRIGGERS;
		break;
	case 0x10000:
		ctx->subaction_mask |= APK_INFO_REPLACES;
		break;
	case 0x10001:
		ctx->subaction_mask |= APK_INFO_LICENSE;
		break;
	case 'a':
		ctx->subaction_mask = 0xffffffff;
		break;
	default:
		return -1;
	}
	return 0;
}

static int info_main(void *ctx, struct apk_database *db, struct apk_string_array *args)
{
	struct info_ctx *ictx = (struct info_ctx *) ctx;
	struct apk_installed_package *ipkg;

	ictx->db = db;
	if (ictx->subaction_mask == 0)
		ictx->subaction_mask = APK_INFO_DESC | APK_INFO_URL | APK_INFO_SIZE;
	if (ictx->action != NULL) {
		ictx->action(ictx, db, args);
	} else if (args->num > 0) {
		/* Print info on given names */
		apk_name_foreach_matching(
			db, args, APK_FOREACH_NULL_MATCHES_ALL | apk_foreach_genid(),
			print_name_info, ctx);
	} else {
		/* Print all installed packages */
		list_for_each_entry(ipkg, &db->installed.packages, installed_pkgs_list)
			verbose_print_pkg(ipkg->pkg, 1);
	}

	return ictx->errors;
}

static const struct apk_option options_applet[] = {
	{ 'L', "contents",	"List contents of the PACKAGE" },
	{ 'e', "installed",	"Check if PACKAGE is installed" },
	{ 'W', "who-owns",	"Print the package owning the specified file" },
	{ 'R', "depends",	"List packages that the PACKAGE depends on" },
	{ 'P', "provides",	"List virtual packages provided by PACKAGE" },
	{ 'r', "rdepends",	"List all packages depending on PACKAGE" },
	{ 0x10000, "replaces",	"List packages whom files PACKAGE might replace" },
	{ 'i', "install-if",	"List the PACKAGE's install_if rule" },
	{ 'I', "rinstall-if",	"List all packages having install_if referencing PACKAGE" },
	{ 'w', "webpage",	"Show URL for more information about PACKAGE" },
	{ 's', "size",		"Show installed size of PACKAGE" },
	{ 'd', "description",	"Print description for PACKAGE" },
	{ 0x10001, "license",	"Print license for PACKAGE" },
	{ 't', "triggers",	"Print active triggers of PACKAGE" },
	{ 'a', "all",		"Print all information about PACKAGE" },
};

static const struct apk_option_group optgroup_applet = {
	.name = "Info",
	.options = options_applet,
	.num_options = ARRAY_SIZE(options_applet),
	.parse = option_parse_applet,
};

static struct apk_applet apk_info = {
	.name = "info",
	.help = "Give detailed information about PACKAGEs or repositories",
	.arguments = "PACKAGE...",
	.open_flags = APK_OPENF_READ,
	.command_groups = APK_COMMAND_GROUP_QUERY,
	.context_size = sizeof(struct info_ctx),
	.optgroups = { &optgroup_global, &optgroup_applet },
	.main = info_main,
};

APK_DEFINE_APPLET(apk_info);

