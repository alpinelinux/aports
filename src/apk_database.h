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

extern const char * const apk_index_gz;

struct apk_name;
APK_ARRAY(apk_name_array, struct apk_name *);

struct apk_db_file {
	struct hlist_node hash_node;
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

#define APK_NAME_TOPLEVEL	0x0001
#define APK_NAME_VIRTUAL	0x0002

struct apk_name {
	apk_hash_node hash_node;
	unsigned int id;
	unsigned int flags;
	char *name;
	struct apk_package_array *pkgs;
	struct apk_name_array *rdepends;
};

struct apk_repository {
	char *url;
	csum_t url_csum;
};

struct apk_database {
	char *root;
	int root_fd, lock_fd;
	unsigned name_id, num_repos;
	const char *cache_dir;

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
		struct apk_hash files;
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
struct apk_name *apk_db_query_name(struct apk_database *db, apk_blob_t name);
struct apk_db_dir *apk_db_dir_query(struct apk_database *db,
				    apk_blob_t name);
struct apk_db_file *apk_db_file_query(struct apk_database *db,
				      apk_blob_t dir,
				      apk_blob_t name);

#define APK_OPENF_READ		0x0000
#define APK_OPENF_WRITE		0x0001
#define APK_OPENF_CREATE	0x0002
#define APK_OPENF_NO_INSTALLED	0x0010
#define APK_OPENF_NO_SCRIPTS	0x0020
#define APK_OPENF_NO_WORLD	0x0040
#define APK_OPENF_NO_REPOS	0x0080

#define APK_OPENF_NO_STATE	(APK_OPENF_NO_INSTALLED |	\
				 APK_OPENF_NO_SCRIPTS |		\
				 APK_OPENF_NO_WORLD)

int apk_db_open(struct apk_database *db, const char *root, unsigned int flags);
int apk_db_write_config(struct apk_database *db);
void apk_db_close(struct apk_database *db);
int apk_db_cache_active(struct apk_database *db);

struct apk_package *apk_db_pkg_add_file(struct apk_database *db, const char *file);
struct apk_package *apk_db_pkg_add(struct apk_database *db, struct apk_package *pkg);
struct apk_package *apk_db_get_pkg(struct apk_database *db, csum_t sum);
struct apk_package *apk_db_get_file_owner(struct apk_database *db, apk_blob_t filename);

int apk_db_index_read(struct apk_database *db, struct apk_istream *is, int repo);
int apk_db_index_write(struct apk_database *db, struct apk_ostream *os);

int apk_db_add_repository(apk_database_t db, apk_blob_t repository);
int apk_repository_update(struct apk_database *db, struct apk_repository *repo);
int apk_cache_download(struct apk_database *db, csum_t csum,
		       const char *url, const char *item);
int apk_cache_exists(struct apk_database *db, csum_t csum, const char *item);

int apk_db_install_pkg(struct apk_database *db,
		       struct apk_package *oldpkg,
		       struct apk_package *newpkg,
		       apk_progress_cb cb, void *cb_ctx);

#endif
