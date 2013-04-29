/* apk_solver_data.h - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2005-2008 Natanael Copa <n@tanael.org>
 * Copyright (C) 2008-2012 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#ifndef APK_SOLVER_DATA_H
#define APK_SOLVER_DATA_H

#include <stdint.h>
#include "apk_defines.h"
#include "apk_provider_data.h"

struct apk_score {
	uint32_t unsatisfied;
	uint32_t non_preferred_actions;
	uint32_t non_preferred_pinnings;
	uint32_t preference;
};

struct apk_solver_name_state {
	/* dynamic */
	struct list_head unsolved_list;
	struct apk_score minimum_penalty;
	struct apk_provider chosen;

	unsigned int last_touched_decision;
	unsigned short requirers;
	unsigned short install_ifs;
	unsigned short preferred_pinning;
	unsigned short locked;

	/* one time prepare/finish flags */
	unsigned solver_flags_local : 4;
	unsigned solver_flags_inheritable : 4;

	unsigned decision_counted : 1;
	unsigned originally_installed : 1;
	unsigned has_available_pkgs : 1;
	unsigned in_changeset : 1;
	unsigned in_world_dependency : 1;

	/* dynamic state flags */
	unsigned none_excluded : 1;
	unsigned name_touched : 1;
	unsigned preferred_chosen : 1;
};

#endif
