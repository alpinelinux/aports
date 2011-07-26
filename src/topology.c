/* topology.c - Alpine Package Keeper (APK)
 * Topological sorting of database packages
 *
 * Copyright (C) 2011 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#include "apk_database.h"

static int sort_pkg(apk_hash_item item, void *ctx)
{
	struct apk_package *pkg = (struct apk_package *) item;
	unsigned int *sort_order = (unsigned int *) ctx;
	int i, j;

	/* Avoid recursion to same package */
	if (pkg->topology_sort != 0) {
		/* if pkg->topology_sort==-1 we have encountered a
		 * dependency loop. Just silently ignore it and pick a
		 * random topology sorting. */
		return 0;
	}
	pkg->topology_sort = -1;

	/* Sort all dependants before self */
	for (i = 0; i < pkg->depends->num; i++) {
		struct apk_dependency *dep = &pkg->depends->item[i];
		struct apk_name *name0 = dep->name;

		/* FIXME: sort names in order of ascending preference */
		for (j = 0; j < name0->pkgs->num; j++) {
			struct apk_package *pkg0 = name0->pkgs->item[j];
			if (!apk_dep_is_satisfied(dep, pkg0))
				continue;
			sort_pkg(pkg0, ctx);
		}
	}

	/* FIXME: install_if, provides, etc.*/

	/* Finally assign a topology sorting order */
	pkg->topology_sort = ++(*sort_order);

	return 0;
}

void apk_solver_sort(struct apk_database *db)
{
	unsigned int sort_order = 0;

	apk_hash_foreach(&db->available.packages, sort_pkg, &sort_order);
}
