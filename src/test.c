/* test.c - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2008-2011 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#include <errno.h>
#include <fcntl.h>
#include "apk_applet.h"
#include "apk_database.h"
#include "apk_solver.h"
#include "apk_io.h"
#include "apk_print.h"

struct test_ctx {
	const char *installed_db;
	struct apk_string_array *repos;
	unsigned short solver_flags;
};

static int test_parse(void *pctx, struct apk_db_options *dbopts,
		      int optch, int optindex, const char *optarg)
{
	struct test_ctx *ctx = (struct test_ctx *) pctx;

	switch (optch) {
	case 0x10000:
		ctx->installed_db = optarg;
		break;
	case 0x10001:
		if (ctx->repos == NULL)
			apk_string_array_init(&ctx->repos);
		*apk_string_array_add(&ctx->repos) = (char*) optarg;
		break;
	case 'u':
		ctx->solver_flags |= APK_SOLVERF_UPGRADE;
		break;
	case 'a':
		ctx->solver_flags |= APK_SOLVERF_AVAILABLE;
		break;
	default:
		return -1;
	}
	return 0;
}

static inline void print_change(struct apk_package *oldpkg,
				struct apk_package *newpkg)
{
	const char *msg = NULL;
	struct apk_name *name;
	int r;

	if (oldpkg != NULL)
		name = oldpkg->name;
	else
		name = newpkg->name;

	if (oldpkg == NULL) {
		apk_message("Installing %s (" BLOB_FMT ")",
			    name->name,
			    BLOB_PRINTF(*newpkg->version));
	} else if (newpkg == NULL) {
		apk_message("Purging %s (" BLOB_FMT ")",
			    name->name,
			    BLOB_PRINTF(*oldpkg->version));
	} else {
		r = apk_pkg_version_compare(newpkg, oldpkg);
		switch (r) {
		case APK_VERSION_LESS:
			msg = "Downgrading";
			break;
		case APK_VERSION_EQUAL:
			if (newpkg == oldpkg)
				msg = "Re-installing";
			else
				msg = "Replacing";
			break;
		case APK_VERSION_GREATER:
			msg = "Upgrading";
			break;
		}
		apk_message("%s %s (" BLOB_FMT " -> " BLOB_FMT ")",
			    msg, name->name,
			    BLOB_PRINTF(*oldpkg->version),
			    BLOB_PRINTF(*newpkg->version));
	}
}

static void print_dep_errors(struct apk_database *db, char *label, struct apk_dependency_array *deps)
{
	int i, print_label = 1;
	char buf[256];
	apk_blob_t p = APK_BLOB_BUF(buf);

	for (i = 0; i < deps->num; i++) {
		struct apk_dependency *dep = &deps->item[i];
		struct apk_package *pkg = dep->name->state_ptr;

		if (apk_dep_is_satisfied(dep, pkg))
			continue;

		if (print_label) {
			print_label = 0;
			printf("%s: ", label);
		} else {
			printf(" ");
		}
		apk_blob_push_dep(&p, db, dep);
		p = apk_blob_pushed(APK_BLOB_BUF(buf), p);
		fwrite(p.ptr, p.len, 1, stdout);
	}
	if (!print_label)
		printf("\n");
}

static void print_errors_in_solution(struct apk_database *db, int unsatisfiable,
				     struct apk_package_array *solution)
{
	int i;

	printf("%d unsatisfiable dependencies (solution with %zu names)\n",
		unsatisfiable, solution->num);

	for (i = 0; i < solution->num; i++) {
		struct apk_package *pkg = solution->item[i];
		pkg->name->state_ptr = pkg;
	}

	print_dep_errors(db, "world", db->world);
	for (i = 0; i < solution->num; i++) {
		struct apk_package *pkg = solution->item[i];
		char pkgtext[256];
		snprintf(pkgtext, sizeof(pkgtext), PKG_VER_FMT, PKG_VER_PRINTF(solution->item[i]));
		print_dep_errors(db, pkgtext, pkg->depends);
	}

}

static int test_main(void *pctx, struct apk_database *db, int argc, char **argv)
{
	struct test_ctx *ctx = (struct test_ctx *) pctx;
	struct apk_bstream *bs;
	struct apk_package_array *solution = NULL;
	struct apk_changeset changeset = {};
	apk_blob_t b;
	int i, r;

	if (argc != 1)
		return -EINVAL;

	apk_db_get_tag_id(db, APK_BLOB_STR("testing"));

	/* load installed db */
	if (ctx->installed_db != NULL) {
		bs = apk_bstream_from_file(AT_FDCWD, ctx->installed_db);
		if (bs != NULL) {
			apk_db_index_read(db, bs, -1);
			bs->close(bs, NULL);
		}
	}

	/* load additional indexes */
	if (ctx->repos) {
		for (i = 0; i < ctx->repos->num; i++) {
			char *fn = ctx->repos->item[i];
			int repo = 0;
			if (fn[0] == '+') {
				fn++;
				repo = 1;
			}
			bs = apk_bstream_from_file(AT_FDCWD, fn);
			if (bs != NULL) {
				apk_db_index_read(db, bs, i);
				db->repo_tags[repo].allowed_repos |= BIT(i);
				bs->close(bs, NULL);
			}
		}
	}

	/* construct new world */
	b = APK_BLOB_STR(argv[0]);
	apk_blob_pull_deps(&b, db, &db->world);

	/* run solver */
	r = apk_solver_solve(db, ctx->solver_flags, db->world, &solution, &changeset);
	if (r == 0) {
		/* dump changeset */
		for (i = 0; i < changeset.changes->num; i++) {
			struct apk_change *c = &changeset.changes->item[i];
			print_change(c->oldpkg, c->newpkg);
		}
	} else { /* r >= 1*/
		print_errors_in_solution(db, r, solution);
	}

	return 0;
}

static struct apk_option test_options[] = {
	{ 0x10000,	"installed",	"Installed database",
			required_argument, "DB" },
	{ 0x10001,	"raw-repository", "Add unsigned test repository index",
			required_argument, "INDEX" },
	{ 'u',		"upgrade",	"Prefer to upgrade package" },
	{ 'a',		"available",
	  "Re-install or downgrade if currently installed package is not "
	  "currently available from any repository" },
};

static struct apk_applet test_applet = {
	.name = "test",
	.help = "Test dependency graph solver (uses simple repository and installed db)",
	.arguments = "'WORLD'",
	.open_flags = APK_OPENF_READ | APK_OPENF_NO_STATE | APK_OPENF_NO_REPOS,
	.context_size = sizeof(struct test_ctx),
	.num_options = ARRAY_SIZE(test_options),
	.options = test_options,
	.parse = test_parse,
	.main = test_main,
};

APK_DEFINE_APPLET(test_applet);
