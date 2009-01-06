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

#ifndef APK_PKGDB_H
#define APK_PKGDB_H

#include "apk_version.h"
#include "apk_hash.h"
#include "apk_archive.h"
#include "apk_package.h"

#define APK_MAX_REPOS 32

struct apk_db_file {
	struct hlist_node dir_files_list;
	struct hlist_node diri_files_list;

	struct apk_db_dir_instance *diri;
	csum_t csum;
	char filename[];
};

#define APK_DBDIRF_PROTECTED		0x0001

struct apk_db_dir {
	apk_hash_node hash_node;

	struct hlist_head files;
	struct apk_db_dir *parent;

	unsigned short refs;
	unsigned short flags;
	char dirname[];
};

struct apk_db_dir_instance {
	struct hlist_node pkg_dirs_list;
	struct hlist_head owned_files;
	struct apk_package *pkg;
	struct apk_db_dir *dir;
	mode_t mode;
	uid_t uid;
	gid_t gid;
};

struct apk_name {
	apk_hash_node hash_node;

	char *name;
	struct apk_package_array *pkgs;
};

struct apk_repository {
	char *url;
};

struct apk_database {
	char *root;
	int root_fd;
	unsigned pkg_id, num_repos;

	struct apk_dependency_array *world;
	struct apk_string_array *protected_paths;
	struct apk_repository repos[APK_MAX_REPOS];

	struct {
		struct apk_hash names;
		struct apk_hash packages;
	} available;

	struct {
		struct list_head packages;
		struct apk_hash dirs;
		struct {
			unsigned files;
			unsigned dirs;
			unsigned packages;
		} stats;
	} installed;
};

typedef union apk_database_or_void {
	struct apk_database *db;
	void *ptr;
} apk_database_t __attribute__ ((__transparent_union__));

struct apk_name *apk_db_get_name(struct apk_database *db, apk_blob_t name);
void apk_name_free(struct apk_name *pkgname);

int apk_db_create(const char *root);
int apk_db_open(struct apk_database *db, const char *root);
void apk_db_close(struct apk_database *db);

struct apk_package *apk_db_pkg_add_file(struct apk_database *db, const char *file);
struct apk_package *apk_db_get_pkg(struct apk_database *db, csum_t sum);

void apk_db_index_write(struct apk_database *db, struct apk_ostream *os);

int apk_db_add_repository(apk_database_t db, apk_blob_t repository);
int apk_db_recalculate_and_commit(struct apk_database *db);

int apk_db_install_pkg(struct apk_database *db,
		       struct apk_package *oldpkg,
		       struct apk_package *newpkg);

#endif
