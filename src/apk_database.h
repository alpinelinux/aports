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
	struct hlist_node pkg_files_list;

	struct apk_db_dir *dir;
	struct apk_package *owner;
	char filename[];
};

struct apk_db_dir {
	apk_hash_node hash_node;

	struct hlist_head files;
	struct apk_db_dir *parent;

	unsigned refs;
	mode_t mode;
	char dirname[];
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
	struct apk_repository repos[APK_MAX_REPOS];

	struct {
		struct apk_hash names;
		struct apk_hash packages;
	} available;

	struct {
		struct hlist_head packages;
		struct apk_hash dirs;
		struct {
			unsigned files;
			unsigned dirs;
			unsigned packages;
		} stats;
	} installed;
};

struct apk_name *apk_db_get_name(struct apk_database *db, const char *name);
void apk_name_free(struct apk_name *pkgname);

int apk_db_create(const char *root);
int apk_db_open(struct apk_database *db, const char *root);
void apk_db_close(struct apk_database *db);

int apk_db_pkg_add_file(struct apk_database *db, const char *file);
struct apk_package *apk_db_get_pkg(struct apk_database *db, csum_t sum);

int apk_db_index_read(struct apk_database *db, int fd, int repo);
void apk_db_index_write(struct apk_database *db, int fd);

int apk_db_add_repository(struct apk_database *db, const char *repo);
int apk_db_recalculate_and_commit(struct apk_database *db);

int apk_db_install_pkg(struct apk_database *db,
		       struct apk_package *oldpkg,
		       struct apk_package *newpkg);

#endif
