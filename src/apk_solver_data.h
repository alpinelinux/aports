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

struct apk_solver_name_state {
	struct apk_provider chosen;
	union {
		struct {
			struct list_head dirty_list;
			struct list_head unresolved_list;
		};
		struct {
			struct apk_name *installed_name;
			struct apk_package *installed_pkg;
		};
	};
	unsigned short requirers;
	unsigned short merge_depends;
	unsigned short merge_provides;
	unsigned short max_dep_chain;
	unsigned seen : 1;
	unsigned locked : 1;
	unsigned in_changeset : 1;
	unsigned reevaluate_deps : 1;
	unsigned reevaluate_iif : 1;
	unsigned has_iif : 1;
	unsigned has_options : 1;
	unsigned reverse_deps_done : 1;
};

struct apk_solver_package_state {
	unsigned int conflicts;
	unsigned short max_dep_chain;
	unsigned short pinning_allowed;
	unsigned short pinning_preferred;
	unsigned solver_flags : 4;
	unsigned solver_flags_inheritable : 4;
	unsigned seen : 1;
	unsigned pkg_available : 1;
	unsigned pkg_selectable : 1;
	unsigned tag_ok : 1;
	unsigned tag_preferred : 1;
	unsigned dependencies_used : 1;
	unsigned dependencies_merged : 1;
	unsigned in_changeset : 1;
	unsigned iif_triggered : 1;
	unsigned error : 1;
};

#endif
