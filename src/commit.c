/* commit.c - Alpine Package Keeper (APK)
 * Apply solver calculated changes to database.
 *
 * Copyright (C) 2008-2013 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#include <stdint.h>
#include <unistd.h>
#include "apk_defines.h"
#include "apk_database.h"
#include "apk_package.h"
#include "apk_solver.h"

#include "apk_print.h"

static inline int pkg_available(struct apk_database *db, struct apk_package *pkg)
{
	if (pkg->repos & db->available_repos)
		return TRUE;
	return FALSE;
}

static int print_change(struct apk_database *db, struct apk_change *change,
			int cur, int total)
{
	struct apk_name *name;
	struct apk_package *oldpkg = change->old_pkg;
	struct apk_package *newpkg = change->new_pkg;
	const char *msg = NULL;
	char status[32], n[512], *nameptr;
	apk_blob_t *oneversion = NULL;
	int r;

	snprintf(status, sizeof(status), "(%i/%i)", cur+1, total);
	status[sizeof(status) - 1] = 0;

	name = newpkg ? newpkg->name : oldpkg->name;
	if (change->new_repository_tag > 0) {
		snprintf(n, sizeof(n), "%s@" BLOB_FMT,
			 name->name,
			 BLOB_PRINTF(*db->repo_tags[change->new_repository_tag].name));
		n[sizeof(n) - 1] = 0;
		nameptr = n;
	} else {
		nameptr = name->name;
	}

	if (oldpkg == NULL) {
		msg = "Installing";
		oneversion = newpkg->version;
	} else if (newpkg == NULL) {
		msg = "Purging";
		oneversion = oldpkg->version;
	} else if (newpkg == oldpkg) {
		if (change->reinstall) {
			if (pkg_available(db, newpkg))
				msg = "Re-installing";
			else
				msg = "[APK unavailable, skipped] Re-installing";
		} else if (change->old_repository_tag != change->new_repository_tag) {
			msg = "Updating pinning";
		}
		oneversion = newpkg->version;
	} else {
		r = apk_pkg_version_compare(newpkg, oldpkg);
		switch (r) {
		case APK_VERSION_LESS:
			msg = "Downgrading";
			break;
		case APK_VERSION_EQUAL:
			msg = "Replacing";
			break;
		case APK_VERSION_GREATER:
			msg = "Upgrading";
			break;
		}
	}
	if (msg == NULL)
		return FALSE;

	if (oneversion) {
		apk_message("%s %s %s (" BLOB_FMT ")",
			    status, msg, nameptr,
			    BLOB_PRINTF(*oneversion));
	} else {
		apk_message("%s %s %s (" BLOB_FMT " -> " BLOB_FMT ")",
			    status, msg, nameptr,
			    BLOB_PRINTF(*oldpkg->version),
			    BLOB_PRINTF(*newpkg->version));
	}
	return TRUE;
}

struct apk_stats {
	unsigned int changes;
	unsigned int bytes;
	unsigned int packages;
};

static void count_change(struct apk_change *change, struct apk_stats *stats)
{
	if (change->new_pkg != change->old_pkg || change->reinstall) {
		if (change->new_pkg != NULL) {
			stats->bytes += change->new_pkg->installed_size;
			stats->packages++;
		}
		if (change->old_pkg != NULL)
			stats->packages++;
		stats->changes++;
	} else if (change->new_repository_tag != change->old_repository_tag) {
		stats->packages++;
		stats->changes++;
	}
}

static void draw_progress(int percent)
{
	const int bar_width = apk_get_screen_width() - 7;
	int i;

	fprintf(stderr, "\e7%3i%% [", percent);
	for (i = 0; i < bar_width * percent / 100; i++)
		fputc('#', stderr);
	for (; i < bar_width; i++)
		fputc(' ', stderr);
	fputc(']', stderr);
	fflush(stderr);
	fputs("\e8\e[0K", stderr);
}

struct progress {
	struct apk_stats done;
	struct apk_stats total;
	struct apk_package *pkg;
	size_t percent;
	int progress_fd;
};

static void update_progress(struct progress *prog, size_t percent, int force)
{
	if (prog->percent == percent && !force)
		return;
	prog->percent = percent;
	if (apk_flags & APK_PROGRESS)
		draw_progress(percent);
	if (prog->progress_fd != 0) {
		char buf[8];
		size_t n = snprintf(buf, sizeof(buf), "%zu\n", percent);
		write(prog->progress_fd, buf, n);
	}
}

static void progress_cb(void *ctx, size_t pkg_percent)
{
	struct progress *prog = (struct progress *) ctx;
	size_t partial = 0, percent, total;

	if (prog->pkg != NULL)
		partial = muldiv(pkg_percent, prog->pkg->installed_size, APK_PROGRESS_SCALE);
	total = prog->total.bytes + prog->total.packages;
	if (total > 0)
		percent = muldiv(100, prog->done.bytes + prog->done.packages + partial,
				 prog->total.bytes + prog->total.packages);
	else
		percent = 0;
	update_progress(prog, percent, pkg_percent == 0);
}

static int dump_packages(struct apk_changeset *changeset,
			 int (*cmp)(struct apk_change *change),
			 const char *msg)
{
	struct apk_change *change;
	struct apk_name *name;
	struct apk_indent indent = { .indent = 2 };
	int match = 0, i;

	for (i = 0; i < changeset->changes->num; i++) {
		change = &changeset->changes->item[i];
		if (!cmp(change))
			continue;
		if (match == 0)
			printf("%s:\n", msg);
		if (change->new_pkg != NULL)
			name = change->new_pkg->name;
		else
			name = change->old_pkg->name;

		apk_print_indented(&indent, APK_BLOB_STR(name->name));
		match++;
	}
	if (match)
		printf("\n");
	return match;
}

static int cmp_remove(struct apk_change *change)
{
	return change->new_pkg == NULL;
}

static int cmp_new(struct apk_change *change)
{
	return change->old_pkg == NULL;
}

static int cmp_downgrade(struct apk_change *change)
{
	if (change->new_pkg == NULL || change->old_pkg == NULL)
		return 0;
	if (apk_pkg_version_compare(change->new_pkg, change->old_pkg)
	    & APK_VERSION_LESS)
		return 1;
	return 0;
}

static int cmp_upgrade(struct apk_change *change)
{
	if (change->new_pkg == NULL || change->old_pkg == NULL)
		return 0;

	/* Count swapping package as upgrade too - this can happen if
	 * same package version is used after it was rebuilt against
	 * newer libraries. Basically, different (and probably newer)
	 * package, but equal version number. */
	if ((apk_pkg_version_compare(change->new_pkg, change->old_pkg) &
	     (APK_VERSION_GREATER | APK_VERSION_EQUAL)) &&
	    (change->new_pkg != change->old_pkg))
		return 1;

	return 0;
}

static void run_triggers(struct apk_database *db, struct apk_changeset *changeset)
{
	int i;

	if (apk_db_fire_triggers(db) == 0)
		return;

	for (i = 0; i < changeset->changes->num; i++) {
		struct apk_package *pkg = changeset->changes->item[i].new_pkg;
		struct apk_installed_package *ipkg;

		if (pkg == NULL)
			continue;

		ipkg = pkg->ipkg;
		if (ipkg->pending_triggers->num == 0)
			continue;

		*apk_string_array_add(&ipkg->pending_triggers) = NULL;
		apk_ipkg_run_script(ipkg, db, APK_SCRIPT_TRIGGER,
				    ipkg->pending_triggers->item);
		apk_string_array_free(&ipkg->pending_triggers);
	}
}

int apk_solver_commit_changeset(struct apk_database *db,
				struct apk_changeset *changeset,
				struct apk_dependency_array *world)
{
	struct progress prog;
	struct apk_change *change;
	int i, r = 0, size_diff = 0, size_unit;

	if (apk_db_check_world(db, world) != 0) {
		apk_error("Not committing changes due to missing repository tags. Use --force to override.");
		return -1;
	}

	if (changeset->changes == NULL)
		goto all_done;

	/* Count what needs to be done */
	memset(&prog, 0, sizeof(prog));
	prog.progress_fd = db->progress_fd;
	for (i = 0; i < changeset->changes->num; i++) {
		change = &changeset->changes->item[i];
		count_change(change, &prog.total);
		if (change->new_pkg)
			size_diff += change->new_pkg->installed_size;
		if (change->old_pkg)
			size_diff -= change->old_pkg->installed_size;
	}
	size_diff /= 1024;
	size_unit = 'K';
	if (abs(size_diff) > 10000) {
		size_diff /= 1024;
		size_unit = 'M';
	}

	if (apk_verbosity > 1 || (apk_flags & APK_INTERACTIVE)) {
		r = dump_packages(changeset, cmp_remove,
				  "The following packages will be REMOVED");
		r += dump_packages(changeset, cmp_downgrade,
				   "The following packages will be DOWNGRADED");
		if (r || (apk_flags & APK_INTERACTIVE) || apk_verbosity > 2) {
			dump_packages(changeset, cmp_new,
				      "The following NEW packages will be installed");
			dump_packages(changeset, cmp_upgrade,
				      "The following packages will be upgraded");
			printf("After this operation, %d %ciB of %s\n",
				abs(size_diff), size_unit,
				(size_diff < 0) ?
				"disk space will be freed." :
				"additional disk space will be used.");
		}
		if (changeset->changes->num > 0 &&
		    (apk_flags & APK_INTERACTIVE)) {
			printf("Do you want to continue [Y/n]? ");
			fflush(stdout);
			r = fgetc(stdin);
			if (r != 'y' && r != 'Y' && r != '\n')
				return -1;
		}
	}

	/* Go through changes */
	r = 0;
	for (i = 0; i < changeset->changes->num; i++) {
		change = &changeset->changes->item[i];

		if (print_change(db, change, prog.done.changes, prog.total.changes)) {
			prog.pkg = change->new_pkg;
			progress_cb(&prog, 0);

			if (!(apk_flags & APK_SIMULATE)) {
				if (change->old_pkg != change->new_pkg ||
				    (change->reinstall && pkg_available(db, change->new_pkg)))
					r = apk_db_install_pkg(db,
							       change->old_pkg, change->new_pkg,
							       progress_cb, &prog);
				if (r != 0)
					break;
				if (change->new_pkg)
					change->new_pkg->ipkg->repository_tag = change->new_repository_tag;
			}
		}

		count_change(change, &prog.done);
	}
	update_progress(&prog, 100, 1);

	run_triggers(db, changeset);

all_done:
	apk_dependency_array_copy(&db->world, world);
	apk_db_write_config(db);

	if (r == 0 && !db->performing_self_update) {
		if (apk_verbosity > 1) {
			apk_message("OK: %d packages, %d dirs, %d files, %zu MiB",
				    db->installed.stats.packages,
				    db->installed.stats.dirs,
				    db->installed.stats.files,
				    db->installed.stats.bytes / (1024 * 1024));
		} else {
			apk_message("OK: %zu MiB in %d packages",
				    db->installed.stats.bytes / (1024 * 1024),
				    db->installed.stats.packages);
		}
	}

	return r;
}

static void add_name_if_virtual(struct apk_name_array **names, struct apk_name *name)
{
	int i;

	if (name->providers->num == 0)
		return;

	for (i = 0; i < name->providers->num; i++)
		if (name->providers->item[i].pkg->name == name)
			return;

	for (i = 0; i < (*names)->num; i++)
		if ((*names)->item[i] == name)
			return;

	*apk_name_array_add(names) = name;
}

static void print_pinning_errors(struct apk_database *db, char *label,
				 struct apk_package *pkg, unsigned int tag)
{
	int i;

	if (pkg->ipkg != NULL)
		return;
	if (pkg->repos & apk_db_get_pinning_mask_repos(db, APK_DEFAULT_PINNING_MASK | BIT(tag)))
		return;

	printf("  %s: not pinned:", label);
	for (i = 0; i < db->num_repo_tags; i++) {
		if (pkg->repos & db->repo_tags[i].allowed_repos)
			printf(" @" BLOB_FMT, BLOB_PRINTF(*db->repo_tags[i].name));
	}
	printf("\n");
}

static void print_dep_errors(struct apk_database *db, char *label,
			     struct apk_dependency_array *deps,
			     struct apk_name_array **names)
{
	int i, print_label = 1;
	char buf[256];
	apk_blob_t p;
	struct apk_indent indent;

	for (i = 0; i < deps->num; i++) {
		struct apk_dependency *dep = &deps->item[i];
		struct apk_package *pkg;

		pkg = (struct apk_package*) (dep->name->state_int & ~1);
		if (apk_dep_is_materialized_or_provided(dep, pkg))
			continue;

		if (pkg == NULL)
			add_name_if_virtual(names, dep->name);

		if (print_label) {
			print_label = 0;
			indent.x = printf("  %s:", label);
			indent.indent = indent.x + 1;
		}
		p = APK_BLOB_BUF(buf);
		apk_blob_push_dep(&p, db, dep);
		p = apk_blob_pushed(APK_BLOB_BUF(buf), p);
		apk_print_indented(&indent, p);
	}
	if (!print_label)
		printf("\n");
}

static void print_provides_errors(struct apk_database *db, char *label,
				  struct apk_package *pkg)
{
	int i;

	for (i = 0; i < pkg->provides->num; i++) {
		struct apk_name *pn = pkg->provides->item[i].name;
		struct apk_provider p = APK_PROVIDER_FROM_PROVIDES(pkg, &pkg->provides->item[i]);
		struct apk_package *pkgC = (struct apk_package *) (pn->state_int & ~1);

		if (pkgC == NULL && p.version == &apk_null_blob)
			continue;

		if ((pkgC == pkg) && (pn->state_int & 1) == 0) {
			pn->state_int |= 1;
			continue;
		}

		printf("  %s provides " PROVIDER_FMT " conflicting with " PKG_VER_FMT "\n",
			label, PROVIDER_PRINTF(pn, &p), PKG_VER_PRINTF(pkgC));
	}
}

void apk_solver_print_errors(struct apk_database *db,
			     struct apk_changeset *changeset,
			     struct apk_dependency_array *world)
{
	struct apk_name_array *names;
	char pkgtext[256];
	int i, j;

	apk_name_array_init(&names);
	/* FIXME: Print more rational error messages. Group failed
	 * dependencies by package name. As in,
	 * pkg-1.2 selected, but:
	 *   world: pkg>2
	 */
	apk_error("unsatisfiable dependencies:");

	for (i = 0; i < changeset->changes->num; i++) {
		struct apk_package *pkg = changeset->changes->item[i].new_pkg;
		if (pkg == NULL)
			continue;
		pkg->state_int = 1;
		pkg->name->state_ptr = pkg;
		for (j = 0; j < pkg->provides->num; j++) {
			if (pkg->provides->item[j].version == &apk_null_blob)
				continue;
			if (pkg->provides->item[j].name->state_ptr == NULL)
				pkg->provides->item[j].name->state_ptr = pkg;
		}
	}

	print_dep_errors(db, "world", world, &names);
	for (i = 0; i < changeset->changes->num; i++) {
		struct apk_change *change = &changeset->changes->item[i];
		struct apk_package *pkg = change->new_pkg;
		if (pkg == NULL)
			continue;
		snprintf(pkgtext, sizeof(pkgtext), PKG_VER_FMT, PKG_VER_PRINTF(pkg));
		print_pinning_errors(db, pkgtext, pkg, change->new_repository_tag);
		print_dep_errors(db, pkgtext, pkg->depends, &names);
		print_provides_errors(db, pkgtext, pkg);
	}

	for (i = 0; i < names->num; i++) {
		struct apk_indent indent;
		struct apk_name *name = names->item[i];

		printf("\n  %s is a virtual package provided by:\n    ", name->name);
		indent.x = 4;
		indent.indent = 4;

		for (j = 0; j < name->providers->num; j++) {
			struct apk_package *pkg = name->providers->item[j].pkg;
			snprintf(pkgtext, sizeof(pkgtext), PKG_VER_FMT, PKG_VER_PRINTF(pkg));
			apk_print_indented(&indent, APK_BLOB_STR(pkgtext));
		}
		printf("\n");
	}
	apk_name_array_free(&names);
}

int apk_solver_commit(struct apk_database *db,
		      unsigned short solver_flags,
		      struct apk_dependency_array *world)
{
	struct apk_changeset changeset = {};
	int r;

	if (apk_db_check_world(db, world) != 0) {
		apk_error("Not committing changes due to missing repository tags. Use --force to override.");
		return -1;
	}

	r = apk_solver_solve(db, solver_flags, world, &changeset);
	if (r < 0)
		return r;

	if (r == 0 || (apk_flags & APK_FORCE)) {
		/* Success -- or forced installation of bad graph */
		r = apk_solver_commit_changeset(db, &changeset, world);
	} else {
		/* Failure -- print errors */
		apk_solver_print_errors(db, &changeset, world);
	}
	apk_change_array_free(&changeset.changes);

	return r;
}
