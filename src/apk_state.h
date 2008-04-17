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

#define APK_STATE_NOT_CONSIDERED		0
#define APK_STATE_PREFER_UPGRADE		1
#define APK_STATE_INSTALL			2
#define APK_STATE_NO_INSTALL			3

struct apk_state {
	int refs;
	unsigned char bitarray[];
};

struct apk_deferred_state {
	unsigned int preference;
	struct apk_package *deferred_install;
	/* struct apk_pkg_name_queue *install_queue; */
	struct apk_state *state;
};

struct apk_state *apk_state_new(struct apk_database *db);
struct apk_state *apk_state_dup(struct apk_state *state);
void apk_state_unref(struct apk_state *state);

int apk_state_commit(struct apk_state *state, struct apk_database *db);

int apk_state_satisfy_deps(struct apk_state *state,
			   struct apk_dependency_array *deps);

void apk_state_pkg_set(struct apk_state *state,
		       struct apk_package *pkg);
int apk_state_pkg_install(struct apk_state *state,
			  struct apk_package *pkg);
int apk_state_pkg_is_installed(struct apk_state *state,
			       struct apk_package *pkg);

#endif
