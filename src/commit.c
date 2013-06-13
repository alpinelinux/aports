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

enum {
	STATE_UNSET = 0,
	STATE_PRESENT,
	STATE_MISSING
};

struct print_state {
	struct apk_database *db;
	struct apk_dependency_array *world;
	struct apk_indent i;
	struct apk_name_array *missing;
	const char *label;
	int num_labels;
	int match;
};

static void label_start(struct print_state *ps, const char *text)
{
	if (ps->label) {
		printf("  %s:\n", ps->label);
		ps->label = NULL;
		ps->i.x = ps->i.indent = 0;
		ps->num_labels++;
	}
	if (ps->i.x == 0) {
		ps->i.x = printf("    %s", text);
		ps->i.indent = ps->i.x + 1;
	}
}
static void label_end(struct print_state *ps)
{
	if (ps->i.x != 0) {
		printf("\n");
		ps->i.x = ps->i.indent = 0;
	}
}

static void print_pinning_errors(struct print_state *ps, struct apk_package *pkg, unsigned int tag)
{
	struct apk_database *db = ps->db;
	int i;

	if (pkg->ipkg != NULL)
		return;
	if (pkg->repos & apk_db_get_pinning_mask_repos(db, APK_DEFAULT_PINNING_MASK | BIT(tag)))
		return;

	for (i = 0; i < db->num_repo_tags; i++) {
		if (pkg->repos & db->repo_tags[i].allowed_repos) {
			label_start(ps, "masked in:");
			apk_print_indented(&ps->i, *db->repo_tags[i].name);
		}
	}
	label_end(ps);
}

static void print_conflicts(struct print_state *ps, struct apk_package *pkg)
{
	struct apk_provider *p;
	struct apk_dependency *d;

	foreach_array_item(p, pkg->name->providers) {
		if (p->pkg == pkg || !p->pkg->marked)
			continue;
		label_start(ps, "conflicts:");
		apk_print_indented_fmt(&ps->i, PKG_VER_FMT, PKG_VER_PRINTF(p->pkg));
	}
	foreach_array_item(d, pkg->provides) {
		foreach_array_item(p, d->name->providers) {
			if (p->pkg == pkg || !p->pkg->marked)
				continue;
			label_start(ps, "conflicts:");
			apk_print_indented_fmt(
				&ps->i,
				PKG_VER_FMT "[%s]",
				PKG_VER_PRINTF(p->pkg),
				d->name->name);
		}
	}
	label_end(ps);
}

static void print_dep(struct apk_package *pkg0, struct apk_dependency *d0, struct apk_package *pkg, void *ctx)
{
	struct print_state *ps = (struct print_state *) ctx;
	const char *label = (ps->match & APK_DEP_SATISFIES) ? "satisfies:" : "breaks:";
	char tmp[256];

	label_start(ps, label);
	if (pkg0 == NULL)
		apk_print_indented_fmt(&ps->i, "world[%s]", apk_dep_snprintf(tmp, sizeof(tmp), d0));
	else
		apk_print_indented_fmt(&ps->i, PKG_VER_FMT "[%s]",
				       PKG_VER_PRINTF(pkg0),
				       apk_dep_snprintf(tmp, sizeof(tmp), d0));
}

static void print_deps(struct print_state *ps, struct apk_package *pkg, int match)
{
	ps->match = match;
	apk_pkg_foreach_matching_dependency(NULL, ps->world, ps->match, pkg, print_dep, ps);
	apk_pkg_foreach_reverse_dependency(pkg, ps->match, print_dep, ps);
	label_end(ps);
}

static void analyze_package(struct print_state *ps, struct apk_package *pkg, unsigned int tag)
{
	char pkgtext[256];

	snprintf(pkgtext, sizeof(pkgtext), PKG_VER_FMT, PKG_VER_PRINTF(pkg));
	ps->label = pkgtext;

	print_pinning_errors(ps, pkg, tag);
	print_conflicts(ps, pkg);
	print_deps(ps, pkg, APK_DEP_CONFLICTS | APK_FOREACH_MARKED);
	if (ps->label == NULL)
		print_deps(ps, pkg, APK_DEP_SATISFIES | APK_FOREACH_MARKED);
}

static void analyze_name(struct print_state *ps, struct apk_name *name)
{
	struct apk_name **pname0, *name0;
	struct apk_provider *p0;
	struct apk_dependency *d0;
	char tmp[256];

	if (name->providers->num) {
		snprintf(tmp, sizeof(tmp), "%s (virtual)", name->name);
		ps->label = tmp;

		label_start(ps, "provided by:");
		foreach_array_item(p0, name->providers) {
			/* FIXME: print only name if all pkgs provide it */
			struct apk_package *pkg = p0->pkg;
			apk_print_indented_fmt(&ps->i, PKG_VER_FMT, PKG_VER_PRINTF(pkg));
		}
		label_end(ps);
	} else {
		snprintf(tmp, sizeof(tmp), "%s (missing)", name->name);
		ps->label = tmp;
	}

	label_start(ps, "required by:");
	foreach_array_item(d0, ps->world) {
		if (d0->name != name || d0->conflict)
			continue;
		apk_print_indented_fmt(&ps->i, "world[%s]",
			apk_dep_snprintf(tmp, sizeof(tmp), d0));
	}
	foreach_array_item(pname0, name->rdepends) {
		name0 = *pname0;
		foreach_array_item(p0, name0->providers) {
			if (!p0->pkg->marked)
				continue;
			foreach_array_item(d0, p0->pkg->depends) {
				if (d0->name != name || d0->conflict)
					continue;
				apk_print_indented_fmt(&ps->i,
					PKG_VER_FMT "[%s]",
					PKG_VER_PRINTF(p0->pkg),
					apk_dep_snprintf(tmp, sizeof(tmp), d0));
				break;
			}
			if (d0 != NULL)
				break;
		}
	}
	label_end(ps);
}

static void analyze_deps(struct print_state *ps, struct apk_dependency_array *deps)
{
	struct apk_dependency *d0;

	foreach_array_item(d0, deps) {
		if (d0->name->state_int != STATE_UNSET)
			continue;
		d0->name->state_int = STATE_MISSING;
		analyze_name(ps, d0->name);
	}
}

void apk_solver_print_errors(struct apk_database *db,
			     struct apk_changeset *changeset,
			     struct apk_dependency_array *world)
{
	struct print_state ps;
	struct apk_change *change;
	struct apk_dependency *p;

	/* ERROR: unsatisfiable dependencies:
	 *   name:
	 *     required by: a b c d e
	 *     not available in any repository
	 *   name (virtual):
	 *     required by: a b c d e
	 *     provided by: foo bar zed
	 *   pkg-1.2:
	 *     masked by: @testing
	 *     satisfies: a[pkg]
	 *     conflicts: pkg-2.0 foo-1.2 bar-1.2
	 *     breaks: b[pkg>2] c[foo>2] d[!pkg]
	 *
	 * When two packages provide same name 'foo':
	 *   a-1:
	 *     satisfies: world[a]
	 *     conflicts: b-1[foo]
	 *   b-1:
	 *     satisfies: world[b]
	 *     conflicts: a-1[foo]
	 * 
	 *   c-1:
	 *     satisfies: world[a]
	 *     conflicts: c-1[foo]  (self-conflict by providing foo twice)
	 *
	 * When two packages get pulled in:
	 *   a-1:
	 *     satisfies: app1[so:a.so.1]
	 *     conflicts: a-2
	 *   a-2:
	 *     satisfies: app2[so:a.so.2]
	 *     conflicts: a-1
	 *
	 * satisfies lists all dependencies that is not satisfiable by
	 * any other selected version. or all of them with -v.
	 */
 
	apk_error("unsatisfiable constraints:");

	/* Construct information about names */
	foreach_array_item(change, changeset->changes) {
		struct apk_package *pkg = change->new_pkg;
		if (pkg == NULL)
			continue;
		pkg->marked = 1;
		pkg->name->state_int = STATE_PRESENT;
		foreach_array_item(p, pkg->provides)
			p->name->state_int = STATE_PRESENT;
	}

	/* Analyze is package, and missing names referred to */
	ps = (struct print_state) {
		.db = db,
		.world = world,
	};
	analyze_deps(&ps, world);
	foreach_array_item(change, changeset->changes) {
		struct apk_package *pkg = change->new_pkg;
		if (pkg == NULL)
			continue;
		analyze_package(&ps, pkg, change->new_repository_tag);
		analyze_deps(&ps, pkg->depends);
	}

	if (ps.num_labels == 0)
		printf("  Huh? Error reporter did not find the broken constraints.\n");
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
	if (r == 0)
		r = apk_solver_commit_changeset(db, &changeset, world);
	else
		apk_solver_print_errors(db, &changeset, world);

	apk_change_array_free(&changeset.changes);
	return r;
}
