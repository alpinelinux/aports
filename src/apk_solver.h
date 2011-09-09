/* apk_solver.h - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2005-2008 Natanael Copa <n@tanael.org>
 * Copyright (C) 2008 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#ifndef APK_SOLVER_H
#define APK_SOLVER_H

struct apk_change {
	struct apk_package *oldpkg;
	struct apk_package *newpkg;
};
APK_ARRAY(apk_change_array, struct apk_change);

struct apk_changeset {
	struct apk_change_array *changes;
};

#define APK_SOLVERF_UPGRADE		0x0001
#define APK_SOLVERF_AVAILABLE		0x0002
#define APK_SOLVERF_REINSTALL		0x0004

int apk_solver_solve(struct apk_database *db,
		     unsigned short solver_flags,
		     struct apk_dependency_array *world,
		     struct apk_package_array **solution,
		     struct apk_changeset *changeset);
int apk_solver_commit_changeset(struct apk_database *db,
				struct apk_changeset *changeset,
				struct apk_dependency_array *world);
void apk_solver_print_errors(struct apk_database *db,
			     struct apk_package_array *solution,
			     struct apk_dependency_array *world,
			     int unsatisfiable);
int apk_solver_commit(struct apk_database *db,
		      unsigned short solver_flags,
		      struct apk_dependency_array *world);

#endif

