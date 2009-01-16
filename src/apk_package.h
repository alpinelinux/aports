/* apk_database.h - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2005-2008 Natanael Copa <n@tanael.org>
 * Copyright (C) 2008 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#ifndef APK_PKG_H
#define APK_PKG_H

#include "apk_version.h"
#include "apk_hash.h"
#include "apk_io.h"

struct apk_database;
struct apk_name;

#define APK_SCRIPT_INVALID		-1
#define APK_SCRIPT_GENERIC		0
#define APK_SCRIPT_PRE_INSTALL		1
#define APK_SCRIPT_POST_INSTALL		2
#define APK_SCRIPT_PRE_DEINSTALL	3
#define APK_SCRIPT_POST_DEINSTALL	4
#define APK_SCRIPT_PRE_UPGRADE		5
#define APK_SCRIPT_POST_UPGRADE		6

struct apk_script {
	struct hlist_node script_list;
	unsigned int type;
	unsigned int size;
	char script[];
};

struct apk_dependency {
	struct apk_name *name;
	char *min_version;
	char *max_version;
};
APK_ARRAY(apk_dependency_array, struct apk_dependency);

struct apk_package {
	apk_hash_node hash_node;

	csum_t csum;
	unsigned id, repos;
	struct apk_name *name;
	char *version;
	char *url, *description, *license;
	struct apk_dependency_array *depends;
	size_t installed_size, size;
	char *filename;

	/* for installed packages only */
	struct list_head installed_pkgs_list;
	struct hlist_head owned_dirs;
	struct hlist_head scripts;
};
APK_ARRAY(apk_package_array, struct apk_package *);

int apk_deps_add(struct apk_dependency_array **depends,
		 struct apk_dependency *dep);
void apk_deps_parse(struct apk_database *db,
		    struct apk_dependency_array **depends,
		    apk_blob_t blob);
int apk_deps_format(char *buf, int size,
		    struct apk_dependency_array *depends);
int apk_script_type(const char *name);

struct apk_package *apk_pkg_new(void);
struct apk_package *apk_pkg_read(struct apk_database *db, const char *name);
void apk_pkg_free(struct apk_package *pkg);

int apk_pkg_add_info(struct apk_database *db, struct apk_package *pkg,
		     char field, apk_blob_t value);
int apk_pkg_get_state(struct apk_package *pkg);
void apk_pkg_set_state(struct apk_database *db, struct apk_package *pkg, int state);
int apk_pkg_add_script(struct apk_package *pkg, struct apk_istream *is,
		       unsigned int type, unsigned int size);
int apk_pkg_run_script(struct apk_package *pkg, int root_fd,
		       unsigned int type);

struct apk_package *apk_pkg_parse_index_entry(struct apk_database *db, apk_blob_t entry);
apk_blob_t apk_pkg_format_index_entry(struct apk_package *pkg, int size, char *buf);

#endif
