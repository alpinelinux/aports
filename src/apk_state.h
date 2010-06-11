/* apk_state.h - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2005-2008 Natanael Copa <n@tanael.org>
 * Copyright (C) 2008 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#ifndef APK_STATE_H
#define APK_STATE_H

#include "apk_database.h"

typedef void *apk_name_state_t;

struct apk_change {
	struct list_head change_list;
	struct apk_package *oldpkg;
	struct apk_package *newpkg;
};

struct apk_state {
	unsigned int refs, num_names;
	struct apk_database *db;
	struct list_head change_list_head;
	struct apk_package_array *conflicts;
	struct apk_name_array *missing;
	apk_name_state_t name[];
};

struct apk_state *apk_state_new(struct apk_database *db);
struct apk_state *apk_state_dup(struct apk_state *state);
void apk_state_unref(struct apk_state *state);

void apk_state_print_errors(struct apk_state *state);
int apk_state_commit(struct apk_state *state, struct apk_database *db);
int apk_state_lock_dependency(struct apk_state *state,
			      struct apk_dependency *dep);
int apk_state_lock_name(struct apk_state *state,
			struct apk_name *name,
			struct apk_package *newpkg);

#endif
