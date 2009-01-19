/* database.c - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2005-2008 Natanael Copa <n@tanael.org>
 * Copyright (C) 2008 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <sys/file.h>

#include "apk_defines.h"
#include "apk_package.h"
#include "apk_database.h"
#include "apk_state.h"
#include "apk_applet.h"

struct install_ctx {
	struct apk_database *db;
	struct apk_package *pkg;

	int script;
	struct apk_db_dir_instance *diri;

	apk_progress_cb cb;
	void *cb_ctx;
	size_t installed_size;
	size_t current_file_size;

	struct hlist_node **diri_node;
	struct hlist_node **file_diri_node;
};

static apk_blob_t pkg_name_get_key(apk_hash_item item)
{
	return APK_BLOB_STR(((struct apk_name *) item)->name);
}

static void pkg_name_free(struct apk_name *name)
{
	free(name->name);
	free(name->pkgs);
	free(name);
}

static const struct apk_hash_ops pkg_name_hash_ops = {
	.node_offset = offsetof(struct apk_name, hash_node),
	.get_key = pkg_name_get_key,
	.hash_key = apk_blob_hash,
	.compare = apk_blob_compare,
	.delete_item = (apk_hash_delete_f) pkg_name_free,
};

static apk_blob_t pkg_info_get_key(apk_hash_item item)
{
	return APK_BLOB_BUF(((struct apk_package *) item)->csum);
}

static unsigned long csum_hash(apk_blob_t csum)
{
	/* Checksum's highest bits have the most "randomness", use that
	 * directly as hash */
	return *(unsigned long *) csum.ptr;
}

static const struct apk_hash_ops pkg_info_hash_ops = {
	.node_offset = offsetof(struct apk_package, hash_node),
	.get_key = pkg_info_get_key,
	.hash_key = csum_hash,
	.compare = apk_blob_compare,
	.delete_item = (apk_hash_delete_f) apk_pkg_free,
};

static apk_blob_t apk_db_dir_get_key(apk_hash_item item)
{
	return APK_BLOB_STR(((struct apk_db_dir *) item)->dirname);
}

static const struct apk_hash_ops dir_hash_ops = {
	.node_offset = offsetof(struct apk_db_dir, hash_node),
	.get_key = apk_db_dir_get_key,
	.hash_key = apk_blob_hash,
	.compare = apk_blob_compare,
	.delete_item = (apk_hash_delete_f) free,
};

struct apk_db_file_hash_key {
	apk_blob_t dirname;
	apk_blob_t filename;
};

static unsigned long apk_db_file_hash_key(apk_blob_t _key)
{
	struct apk_db_file_hash_key *key = (struct apk_db_file_hash_key *) _key.ptr;

	return apk_blob_hash(key->dirname) ^
	       apk_blob_hash(key->filename);
}

static unsigned long apk_db_file_hash_item(apk_hash_item item)
{
	struct apk_db_file *dbf = (struct apk_db_file *) item;

	return apk_blob_hash(APK_BLOB_STR(dbf->diri->dir->dirname)) ^
	       apk_blob_hash(APK_BLOB_STR(dbf->filename));
}

static int apk_db_file_compare_item(apk_hash_item item, apk_blob_t _key)
{
	struct apk_db_file *dbf = (struct apk_db_file *) item;
	struct apk_db_file_hash_key *key = (struct apk_db_file_hash_key *) _key.ptr;
	int r;

	r = apk_blob_compare(key->dirname, APK_BLOB_STR(dbf->diri->dir->dirname));
	if (r != 0)
		return r;

	return apk_blob_compare(key->filename, APK_BLOB_STR(dbf->filename));
}

static const struct apk_hash_ops file_hash_ops = {
	.node_offset = offsetof(struct apk_db_file, hash_node),
	.hash_key = apk_db_file_hash_key,
	.hash_item = apk_db_file_hash_item,
	.compare_item = apk_db_file_compare_item,
	.delete_item = (apk_hash_delete_f) free,
};

struct apk_name *apk_db_query_name(struct apk_database *db, apk_blob_t name)
{
	return (struct apk_name *) apk_hash_get(&db->available.names, name);
}

struct apk_name *apk_db_get_name(struct apk_database *db, apk_blob_t name)
{
	struct apk_name *pn;

	pn = apk_db_query_name(db, name);
	if (pn != NULL)
		return pn;

	pn = calloc(1, sizeof(struct apk_name));
	if (pn == NULL)
		return NULL;

	pn->name = apk_blob_cstr(name);
	apk_hash_insert(&db->available.names, pn);

	return pn;
}

static void apk_db_dir_unref(struct apk_database *db, struct apk_db_dir *dir)
{
	dir->refs--;
	if (dir->refs > 0)
		return;

	db->installed.stats.dirs--;

	if (dir->parent != NULL)
		apk_db_dir_unref(db, dir->parent);
}

static struct apk_db_dir *apk_db_dir_ref(struct apk_db_dir *dir)
{
	dir->refs++;
	return dir;
}

struct apk_db_dir *apk_db_dir_query(struct apk_database *db,
				    apk_blob_t name)
{
	return (struct apk_db_dir *) apk_hash_get(&db->installed.dirs, name);
}

static struct apk_db_dir *apk_db_dir_get(struct apk_database *db,
					 apk_blob_t name)
{
	struct apk_db_dir *dir;
	apk_blob_t bparent;
	int i;

	if (name.len && name.ptr[name.len-1] == '/')
		name.len--;

	dir = apk_db_dir_query(db, name);
	if (dir != NULL)
		return apk_db_dir_ref(dir);

	db->installed.stats.dirs++;
	dir = calloc(1, sizeof(*dir) + name.len + 1);
	dir->refs = 1;
	memcpy(dir->dirname, name.ptr, name.len);
	dir->dirname[name.len] = 0;
	apk_hash_insert(&db->installed.dirs, dir);

	if (name.len == 0)
		dir->parent = NULL;
	else if (apk_blob_rsplit(name, '/', &bparent, NULL))
		dir->parent = apk_db_dir_get(db, bparent);
	else
		dir->parent = apk_db_dir_get(db, APK_BLOB_NULL);

	if (dir->parent != NULL)
		dir->flags = dir->parent->flags;

	for (i = 0; i < db->protected_paths->num; i++) {
		if (db->protected_paths->item[i][0] == '-' &&
		    strcmp(&db->protected_paths->item[i][1], dir->dirname) == 0)
			dir->flags &= ~APK_DBDIRF_PROTECTED;
		else if (strcmp(db->protected_paths->item[i], dir->dirname) == 0)
			dir->flags |= APK_DBDIRF_PROTECTED;
	}

	return dir;
}

static struct apk_db_dir_instance *apk_db_diri_new(struct apk_database *db,
						   struct apk_package *pkg,
						   apk_blob_t name,
						   struct hlist_node ***after)
{
	struct apk_db_dir_instance *diri;

	diri = calloc(1, sizeof(struct apk_db_dir_instance));
	if (diri != NULL) {
		hlist_add_after(&diri->pkg_dirs_list, *after);
		*after = &diri->pkg_dirs_list.next;
		diri->dir = apk_db_dir_get(db, name);
		diri->pkg = pkg;
	}

	return diri;
}

static void apk_db_diri_set(struct apk_db_dir_instance *diri, mode_t mode,
			    uid_t uid, gid_t gid)
{
	diri->mode = mode;
	diri->uid = uid;
	diri->gid = gid;
}

static void apk_db_diri_mkdir(struct apk_db_dir_instance *diri)
{
	if (diri->dir->refs == 1) {
		mkdir(diri->dir->dirname, diri->mode);
		chown(diri->dir->dirname, diri->uid, diri->gid);
	}
}

static void apk_db_diri_rmdir(struct apk_db_dir_instance *diri)
{
	if (diri->dir->refs == 1) {
		rmdir(diri->dir->dirname);
	}
}

static void apk_db_diri_free(struct apk_database *db,
			     struct apk_db_dir_instance *diri)
{
	apk_db_dir_unref(db, diri->dir);
	free(diri);
}

struct apk_db_file *apk_db_file_query(struct apk_database *db,
				      apk_blob_t dir,
				      apk_blob_t name)
{
	struct apk_db_file_hash_key key;

	key = (struct apk_db_file_hash_key) {
		.dirname = dir,
		.filename = name,
	};

	return (struct apk_db_file *) apk_hash_get(&db->installed.files,
						   APK_BLOB_BUF(&key));
}

static struct apk_db_file *apk_db_file_get(struct apk_database *db,
					   struct apk_db_dir_instance *diri,
					   apk_blob_t name,
					   struct hlist_node ***after)
{
	struct apk_db_file *file;
	struct apk_db_file_hash_key key;

	key = (struct apk_db_file_hash_key) {
		.dirname = APK_BLOB_STR(diri->dir->dirname),
		.filename = name,
	};

	file = (struct apk_db_file *) apk_hash_get(&db->installed.files,
						   APK_BLOB_BUF(&key));
	if (file != NULL)
		return file;

	file = calloc(1, sizeof(*file) + name.len + 1);
	memcpy(file->filename, name.ptr, name.len);
	file->filename[name.len] = 0;

	file->diri = diri;
	hlist_add_after(&file->diri_files_list, *after);
	*after = &file->diri_files_list.next;

	apk_hash_insert(&db->installed.files, file);
	db->installed.stats.files++;

	return file;
}

static void apk_db_file_change_owner(struct apk_database *db,
				     struct apk_db_file *file,
				     struct apk_db_dir_instance *diri,
				     struct hlist_node ***after)
{
	hlist_del(&file->diri_files_list, &file->diri->owned_files);
	file->diri = diri;
	hlist_add_after(&file->diri_files_list, *after);
	*after = &file->diri_files_list.next;
}

static struct apk_package *apk_db_pkg_add(struct apk_database *db, struct apk_package *pkg)
{
	struct apk_package *idb;

	idb = apk_hash_get(&db->available.packages, APK_BLOB_BUF(pkg->csum));
	if (idb == NULL) {
		idb = pkg;
		pkg->id = db->pkg_id++;
		apk_hash_insert(&db->available.packages, pkg);
		*apk_package_array_add(&pkg->name->pkgs) = pkg;
	} else {
		idb->repos |= pkg->repos;
		apk_pkg_free(pkg);
	}
	return idb;
}

static int apk_db_index_read(struct apk_database *db, struct apk_istream *is, int repo)
{
	struct apk_package *pkg = NULL;
	struct apk_db_dir_instance *diri = NULL;
	struct apk_db_file *file = NULL;
	struct hlist_node **diri_node = NULL;
	struct hlist_node **file_diri_node = NULL;

	char buf[1024];
	apk_blob_t l, r;
	int n, field;

	r = APK_BLOB_PTR_LEN(buf, 0);
	while (1) {
		n = is->read(is, &r.ptr[r.len], sizeof(buf) - r.len);
		if (n <= 0)
			break;
		r.len += n;

		while (apk_blob_splitstr(r, "\n", &l, &r)) {
			if (l.len < 2 || l.ptr[1] != ':') {
				if (pkg == NULL)
					continue;

				if (repo != -1)
					pkg->repos |= BIT(repo);
				else
					apk_pkg_set_state(db, pkg, APK_STATE_INSTALL);

				if (apk_db_pkg_add(db, pkg) != pkg && repo == -1) {
					apk_error("Installed database load failed");
					return -1;
				}
				pkg = NULL;
				continue;
			}

			/* Get field */
			field = l.ptr[0];
			l.ptr += 2;
			l.len -= 2;

			/* If no package, create new */
			if (pkg == NULL) {
				pkg = apk_pkg_new();
				diri = NULL;
				diri_node = hlist_tail_ptr(&pkg->owned_dirs);
				file_diri_node = NULL;
			}

			/* Standard index line? */
			if (apk_pkg_add_info(db, pkg, field, l) == 0)
				continue;

			if (repo != -1) {
				apk_error("Invalid index entry '%c'", field);
				return -1;
			}

			/* Check FDB special entries */
			switch (field) {
			case 'F':
				if (pkg->name == NULL) {
					apk_error("FDB directory entry before package entry");
					return -1;
				}
				diri = apk_db_diri_new(db, pkg, l, &diri_node);
				file_diri_node = &diri->owned_files.first;
				break;
			case 'M':
				if (diri == NULL) {
					apk_error("FDB directory metadata entry before directory entry");
					return -1;
				}
				/* FIXME: sscanf may touch unallocated area */
				if (sscanf(l.ptr, "%d:%d:%o",
					   &diri->uid, &diri->gid, &diri->mode) != 3) {
					apk_error("FDB bad directory mode entry");
					return -1;
				}
				break;
			case 'R':
				if (diri == NULL) {
					apk_error("FDB file entry before directory entry");
					return -1;
				}
				file = apk_db_file_get(db, diri, l,
						       &file_diri_node);
				break;
			case 'Z':
				if (file == NULL) {
					apk_error("FDB checksum entry before file entry");
					return -1;
				}
				if (apk_hexdump_parse(APK_BLOB_BUF(file->csum), l)) {
					apk_error("Not a valid checksum");
					return -1;
				}
				break;
			default:
				apk_error("FDB entry '%c' unsupported", n);
				return -1;
			}
		}

		memcpy(&buf[0], r.ptr, r.len);
		r = APK_BLOB_PTR_LEN(buf, r.len);
	}

	return 0;
}

static int apk_db_write_fdb(struct apk_database *db, struct apk_ostream *os)
{
	struct apk_package *pkg;
	struct apk_db_dir_instance *diri;
	struct apk_db_file *file;
	struct hlist_node *c1, *c2;
	char buf[1024];
	apk_blob_t blob;
	int n = 0;

	list_for_each_entry(pkg, &db->installed.packages, installed_pkgs_list) {
		blob = apk_pkg_format_index_entry(pkg, sizeof(buf), buf);
		if (blob.ptr)
			os->write(os, blob.ptr, blob.len - 1);

		hlist_for_each_entry(diri, c1, &pkg->owned_dirs, pkg_dirs_list) {
			n += snprintf(&buf[n], sizeof(buf)-n,
				      "F:%s\n"
				      "M:%d:%d:%o\n",
				      diri->dir->dirname,
				      diri->uid, diri->gid, diri->mode);

			hlist_for_each_entry(file, c2, &diri->owned_files, diri_files_list) {
				n += snprintf(&buf[n], sizeof(buf)-n,
					      "R:%s\n",
					      file->filename);
				if (csum_valid(file->csum)) {
					n += snprintf(&buf[n], sizeof(buf)-n, "Z:");
					n += apk_hexdump_format(sizeof(buf)-n, &buf[n],
								APK_BLOB_BUF(file->csum));
					n += snprintf(&buf[n], sizeof(buf)-n, "\n");
				}

				if (os->write(os, buf, n) != n)
					return -1;
				n = 0;
			}
			if (n != 0 && os->write(os, buf, n) != n)
				return -1;
			n = 0;
		}
		os->write(os, "\n", 1);
	}

	return 0;
}

struct apk_script_header {
	csum_t csum;
	unsigned int type;
	unsigned int size;
};

static int apk_db_scriptdb_write(struct apk_database *db, struct apk_ostream *os)
{
	struct apk_package *pkg;
	struct apk_script *script;
	struct apk_script_header hdr;
	struct hlist_node *c2;

	list_for_each_entry(pkg, &db->installed.packages, installed_pkgs_list) {
		hlist_for_each_entry(script, c2, &pkg->scripts, script_list) {
			memcpy(hdr.csum, pkg->csum, sizeof(csum_t));
			hdr.type = script->type;
			hdr.size = script->size;

			if (os->write(os, &hdr, sizeof(hdr)) != sizeof(hdr))
				return -1;

			if (os->write(os, script->script, script->size) != script->size)
				return -1;
		}
	}

	return 0;
}

static int apk_db_scriptdb_read(struct apk_database *db, struct apk_istream *is)
{
	struct apk_package *pkg;
	struct apk_script_header hdr;

	while (is->read(is, &hdr, sizeof(hdr)) == sizeof(hdr)) {
		pkg = apk_db_get_pkg(db, hdr.csum);
		if (pkg != NULL)
			apk_pkg_add_script(pkg, is, hdr.type, hdr.size);
	}

	return 0;
}

static int apk_db_read_state(struct apk_database *db)
{
	struct apk_istream *is;
	apk_blob_t blob;

	/* Read:
	 * 1. installed repository
	 * 2. source repositories
	 * 3. master dependencies
	 * 4. package statuses
	 * 5. files db
	 * 6. script db
	 */
	fchdir(db->root_fd);

	blob = apk_blob_from_file("var/lib/apk/world");
	if (APK_BLOB_IS_NULL(blob))
		return -ENOENT;
	apk_deps_parse(db, &db->world, blob);
	free(blob.ptr);

	is = apk_istream_from_file("var/lib/apk/installed");
	if (is != NULL) {
		apk_db_index_read(db, is, -1);
		is->close(is);
	}

	is = apk_istream_from_file("var/lib/apk/scripts");
	if (is != NULL) {
		apk_db_scriptdb_read(db, is);
		is->close(is);
	}

	return 0;
}

static int add_protected_path(void *ctx, apk_blob_t blob)
{
	struct apk_database *db = (struct apk_database *) ctx;

	*apk_string_array_add(&db->protected_paths) = apk_blob_cstr(blob);
	return 0;
}

static int apk_db_create(struct apk_database *db)
{
	apk_blob_t deps = APK_BLOB_STR("busybox alpine-baselayout "
				       "apk-tools alpine-conf");
	int fd;

	fchdir(db->root_fd);
	mkdir("tmp", 01777);
	mkdir("dev", 0755);
	mknod("dev/null", 0666, makedev(1, 3));
	mkdir("var", 0755);
	mkdir("var/lib", 0755);
	mkdir("var/lib/apk", 0755);

	fd = creat("var/lib/apk/world", 0644);
	if (fd < 0)
		return -errno;
	write(fd, deps.ptr, deps.len);
	close(fd);

	return 0;
}

int apk_db_open(struct apk_database *db, const char *root, unsigned int flags)
{
	apk_blob_t blob;
	const char *apk_repos = getenv("APK_REPOS"), *msg;
	int r;

	memset(db, 0, sizeof(*db));
	apk_hash_init(&db->available.names, &pkg_name_hash_ops, 1000);
	apk_hash_init(&db->available.packages, &pkg_info_hash_ops, 4000);
	apk_hash_init(&db->installed.dirs, &dir_hash_ops, 1000);
	apk_hash_init(&db->installed.files, &file_hash_ops, 4000);
	list_init(&db->installed.packages);

	if (root != NULL) {
		fchdir(apk_cwd_fd);
		db->root = strdup(root);
		db->root_fd = open(root, O_RDONLY);
		if (db->root_fd < 0 && (flags & APK_OPENF_CREATE)) {
			mkdir(db->root, 0755);
			db->root_fd = open(root, O_RDONLY);
		}
		if (db->root_fd < 0) {
			msg = "Unable to open root";
			goto ret_errno;
		}

		fchdir(db->root_fd);
		if (flags & APK_OPENF_WRITE) {
			db->lock_fd = open("var/lib/apk/lock",
					   O_CREAT | O_WRONLY, 0400);
			if (db->lock_fd < 0 && errno == ENOENT &&
			    (flags & APK_OPENF_CREATE)) {
				r = apk_db_create(db);
				if (r != 0) {
					msg = "Unable to create database";
					goto ret_r;
				}
				db->lock_fd = open("var/lib/apk/lock",
						   O_CREAT | O_WRONLY, 0400);
			}
			if (db->lock_fd < 0 ||
			    flock(db->lock_fd, LOCK_EX | LOCK_NB) < 0) {
				msg = "Unable to lock database";
				goto ret_errno;
			}
		}
	}

	blob = APK_BLOB_STR("etc:-etc/init.d");
	apk_blob_for_each_segment(blob, ":", add_protected_path, db);

	if (root != NULL) {
		r = apk_db_read_state(db);
		if (r == -ENOENT && (flags & APK_OPENF_CREATE)) {
			r = apk_db_create(db);
			if (r != 0) {
				msg = "Unable to create database";
				goto ret_r;
			}
			r = apk_db_read_state(db);
		}
		if (r != 0) {
			msg = "Unable to read database state";
			goto ret_r;
		}

		if (apk_repos == NULL)
			apk_repos = "/etc/apk/repositories";
		blob = apk_blob_from_file(apk_repos);
		if (!APK_BLOB_IS_NULL(blob)) {
			r = apk_blob_for_each_segment(blob, "\n",
						      apk_db_add_repository, db);
			free(blob.ptr);
			if (r != 0) {
				msg = "Unable to load repositories";
				goto ret_r;
			}
		}
	}

	if (apk_repository != NULL) {
		r = apk_db_add_repository(db, APK_BLOB_STR(apk_repository));
		if (r != 0) {
			msg = "Unable to load repositories";
			goto ret_r;
		}
	}

	return 0;

ret_errno:
	r = -errno;
ret_r:
	apk_error("%s: %s", msg, strerror(-r));
	apk_db_close(db);
	return r;
}

struct write_ctx {
	struct apk_database *db;
	int fd;
};

static int apk_db_write_config(struct apk_database *db)
{
	struct apk_ostream *os;
	char buf[1024];
	int n;

	if (db->root == NULL)
		return 0;

	if (db->lock_fd == 0) {
		apk_error("Refusing to write db without write lock!");
		return -1;
	}

	fchdir(db->root_fd);

	os = apk_ostream_to_file("var/lib/apk/world", 0644);
	if (os == NULL)
		return -1;
	n = apk_deps_format(buf, sizeof(buf), db->world);
	if (n < sizeof(buf))
		buf[n++] = '\n';
	os->write(os, buf, n);
	os->close(os);

	os = apk_ostream_to_file("var/lib/apk/installed.new", 0644);
	if (os == NULL)
		return -1;
	apk_db_write_fdb(db, os);
	os->close(os);

	if (rename("var/lib/apk/installed.new", "var/lib/apk/installed") < 0)
		return -errno;

	os = apk_ostream_to_file("var/lib/apk/scripts", 0644);
	if (os == NULL)
		return -1;
	apk_db_scriptdb_write(db, os);
	os->close(os);

	return 0;
}

void apk_db_close(struct apk_database *db)
{
	struct apk_package *pkg;
	struct apk_db_dir_instance *diri;
	struct hlist_node *dc, *dn;
	int i;

	list_for_each_entry(pkg, &db->installed.packages, installed_pkgs_list) {
		hlist_for_each_entry_safe(diri, dc, dn, &pkg->owned_dirs, pkg_dirs_list) {
			apk_db_diri_free(db, diri);
		}
	}

	for (i = 0; i < db->num_repos; i++)
		free(db->repos[i].url);
	if (db->protected_paths) {
		for (i = 0; i < db->protected_paths->num; i++)
			free(db->protected_paths->item[i]);
		free(db->protected_paths);
	}
	if (db->world)
		free(db->world);

	apk_hash_free(&db->available.names);
	apk_hash_free(&db->available.packages);
	apk_hash_free(&db->installed.files);
	apk_hash_free(&db->installed.dirs);

	if (db->root_fd)
		close(db->root_fd);
	if (db->lock_fd)
		close(db->lock_fd);
	if (db->root != NULL)
		free(db->root);
}

struct apk_package *apk_db_get_pkg(struct apk_database *db, csum_t sum)
{
	return apk_hash_get(&db->available.packages,
			    APK_BLOB_PTR_LEN((void*) sum, sizeof(csum_t)));
}

struct apk_package *apk_db_get_file_owner(struct apk_database *db,
					  apk_blob_t filename)
{
	struct apk_db_file *dbf;
	struct apk_db_file_hash_key key;

	if (filename.len && filename.ptr[0] == '/')
		filename.len--, filename.ptr++;

	if (!apk_blob_rsplit(filename, '/', &key.dirname, &key.filename))
		return NULL;

	dbf = (struct apk_db_file *) apk_hash_get(&db->installed.files,
						  APK_BLOB_BUF(&key));
	if (dbf == NULL)
		return NULL;

	return dbf->diri->pkg;
}

struct apk_package *apk_db_pkg_add_file(struct apk_database *db, const char *file)
{
	struct apk_package *info;

	info = apk_pkg_read(db, file);
	if (info != NULL)
		apk_db_pkg_add(db, info);
	return info;
}

struct index_write_ctx {
	struct apk_ostream *os;
	int count;
};

static int write_index_entry(apk_hash_item item, void *ctx)
{
	struct index_write_ctx *iwctx = (struct index_write_ctx *) ctx;
	struct apk_package *pkg = (struct apk_package *) item;
	char buf[1024];
	apk_blob_t blob;

	if (pkg->repos != 0)
		return 0;

	blob = apk_pkg_format_index_entry(pkg, sizeof(buf), buf);
	if (APK_BLOB_IS_NULL(blob))
		return 0;

	if (iwctx->os->write(iwctx->os, blob.ptr, blob.len) != blob.len)
		return -1;

	iwctx->count++;
	return 0;
}

int apk_db_index_write(struct apk_database *db, struct apk_ostream *os)
{
	struct index_write_ctx ctx = { os, 0 };

	apk_hash_foreach(&db->available.packages, write_index_entry, &ctx);

	return ctx.count;
}

int apk_db_add_repository(apk_database_t _db, apk_blob_t repository)
{
	struct apk_database *db = _db.db;
	struct apk_istream *is;
	char tmp[256];
	int r;

	if (repository.ptr == NULL || *repository.ptr == '\0' 
			|| *repository.ptr == '#')
		return 0;

	if (db->num_repos >= APK_MAX_REPOS)
		return -1;

	r = db->num_repos++;
	db->repos[r] = (struct apk_repository){
		.url = apk_blob_cstr(repository)
	};

	snprintf(tmp, sizeof(tmp), "%s/APK_INDEX.gz", db->repos[r].url);
	is = apk_istream_from_url_gz(tmp);
	if (is == NULL) {
		apk_error("Failed to open index file %s", tmp);
		return -1;
	}
	apk_db_index_read(db, is, r);
	is->close(is);

	return 0;
}

int apk_db_recalculate_and_commit(struct apk_database *db)
{
	struct apk_state *state;
	int r;

	state = apk_state_new(db);
	r = apk_state_satisfy_deps(state, db->world);
	if (r == 0) {
		r = apk_state_purge_unneeded(state, db);
		if (r != 0) {
			apk_error("Failed to clean up state");
			return r;
		}

		r = apk_state_commit(state, db);
		if (r != 0) {
			apk_error("Failed to commit changes");
			return r;
		}
		apk_db_write_config(db);

		apk_message("OK: %d packages, %d dirs, %d files",
			    db->installed.stats.packages,
			    db->installed.stats.dirs,
			    db->installed.stats.files);
	} else {
		apk_error("Failed to build installation graph");
	}
	apk_state_unref(state);

	return r;
}

static void extract_cb(void *_ctx, size_t progress)
{
	struct install_ctx *ctx = (struct install_ctx *) _ctx;

	if (ctx->cb) {
		size_t size = ctx->installed_size;

		size += muldiv(progress, ctx->current_file_size, APK_PROGRESS_SCALE);
		if (size > ctx->pkg->installed_size)
			size = ctx->pkg->installed_size;

		ctx->cb(ctx->cb_ctx, muldiv(APK_PROGRESS_SCALE, size, ctx->pkg->installed_size));
	}
}

static int apk_db_install_archive_entry(void *_ctx,
					const struct apk_file_info *ae,
					struct apk_istream *is)
{
	struct install_ctx *ctx = (struct install_ctx *) _ctx;
	struct apk_database *db = ctx->db;
	struct apk_package *pkg = ctx->pkg, *opkg;
	apk_blob_t name = APK_BLOB_STR(ae->name), bdir, bfile;
	struct apk_db_dir_instance *diri = ctx->diri;
	struct apk_db_file *file;
	struct apk_file_info fi;
	char alt_name[PATH_MAX];
	const char *p;
	int r = 0, type = APK_SCRIPT_INVALID;

	/* Package metainfo and script processing */
	if (ae->name[0] == '.') {
		/* APK 2.0 format */
		if (strcmp(ae->name, ".INSTALL") != 0)
			return 0;
		type = APK_SCRIPT_GENERIC;
	} else if (strncmp(ae->name, "var/db/apk/", 11) == 0) {
		/* APK 1.0 format */
		p = &ae->name[11];
		if (strncmp(p, pkg->name->name, strlen(pkg->name->name)) != 0)
			return 0;
		p += strlen(pkg->name->name) + 1;
		if (strncmp(p, pkg->version, strlen(pkg->version)) != 0)
			return 0;
		p += strlen(pkg->version) + 1;

		type = apk_script_type(p);
		if (type == APK_SCRIPT_INVALID)
			return 0;
	}

	/* Handle script */
	if (type != APK_SCRIPT_INVALID) {
		apk_pkg_add_script(pkg, is, type, ae->size);

		if (type == APK_SCRIPT_GENERIC ||
		    type == ctx->script) {
			r = apk_pkg_run_script(pkg, db->root_fd, ctx->script);
			if (r != 0)
				apk_error("%s-%s: Failed to execute pre-install/upgrade script",
					  pkg->name->name, pkg->version);
		}

		return r;
	}

	/* Show progress */
	if (ctx->cb) {
		size_t size = ctx->installed_size;
		if (size > pkg->installed_size)
			size = pkg->installed_size;
		ctx->cb(ctx->cb_ctx, muldiv(APK_PROGRESS_SCALE, size, pkg->installed_size));
	}

	/* Installable entry */
	ctx->current_file_size = apk_calc_installed_size(ae->size);
	if (!S_ISDIR(ae->mode)) {
		if (!apk_blob_rsplit(name, '/', &bdir, &bfile))
			return 0;

		if (bfile.len > 6 && memcmp(bfile.ptr, ".keep_", 6) == 0)
			return 0;

		/* Make sure the file is part of the cached directory tree */
		if (diri == NULL ||
		    strncmp(diri->dir->dirname, bdir.ptr, bdir.len) != 0 ||
		    diri->dir->dirname[bdir.len] != 0) {
			struct hlist_node *n;

			hlist_for_each_entry(diri, n, &pkg->owned_dirs, pkg_dirs_list) {
				if (strncmp(diri->dir->dirname, bdir.ptr, bdir.len) == 0 &&
				    diri->dir->dirname[bdir.len] == 0)
					break;
			}
			if (diri == NULL) {
				apk_error("%s: File '%*s' entry without directory entry.\n",
					  pkg->name->name, name.len, name.ptr);
				return -1;
			}
			ctx->diri = diri;
			ctx->file_diri_node = hlist_tail_ptr(&diri->owned_files);
		}

		file = apk_db_file_get(db, diri, bfile, &ctx->file_diri_node);
		if (file == NULL) {
			apk_error("%s: Failed to create fdb entry for '%*s'\n",
				  pkg->name->name, name.len, name.ptr);
			return -1;
		}

		if (file->diri != diri) {
			opkg = file->diri->pkg;
			if (opkg->name != pkg->name &&
			    strcmp(opkg->name->name, "busybox") != 0) {
				apk_error("%s: Trying to overwrite %s owned by %s.\n",
					  pkg->name->name, ae->name, opkg->name->name);
				return -1;
			}

			apk_db_file_change_owner(db, file, diri,
						 &ctx->file_diri_node);
		}

		if ((diri->dir->flags & APK_DBDIRF_PROTECTED) &&
		    apk_file_get_info(ae->name, &fi) == 0) {
			/* Protected file. Extract to separate place */
			snprintf(alt_name, sizeof(alt_name),
				 "%s/%s.apk-new",
				 diri->dir->dirname, file->filename);
			r = apk_archive_entry_extract(ae, is, alt_name,
						      extract_cb, ctx);
			if (memcmp(ae->csum, fi.csum, sizeof(csum_t)) == 0) {
				/* not modified locally. rename to original */
				if (rename(alt_name, ae->name) < 0)
					apk_warning("%s: %s", ae->name, 
						    strerror(errno));
		    	}
		} else {
			r = apk_archive_entry_extract(ae, is, NULL,
						      extract_cb, ctx);
		}
		memcpy(file->csum, ae->csum, sizeof(csum_t));
	} else {
		if (name.ptr[name.len-1] == '/')
			name.len--;

		if (ctx->diri_node == NULL)
			ctx->diri_node = hlist_tail_ptr(&pkg->owned_dirs);
		ctx->diri = diri = apk_db_diri_new(db, pkg, name,
						   &ctx->diri_node);
		ctx->file_diri_node = hlist_tail_ptr(&diri->owned_files);

		apk_db_diri_set(diri, ae->mode & 0777, ae->uid, ae->gid);
		apk_db_diri_mkdir(diri);
	}
	ctx->installed_size += ctx->current_file_size;

	return r;
}

static void apk_db_purge_pkg(struct apk_database *db,
			     struct apk_package *pkg)
{
	struct apk_db_dir_instance *diri;
	struct apk_db_file *file;
	struct apk_db_file_hash_key key;
	struct hlist_node *dc, *dn, *fc, *fn;
	char name[1024];

	hlist_for_each_entry_safe(diri, dc, dn, &pkg->owned_dirs, pkg_dirs_list) {
		hlist_for_each_entry_safe(file, fc, fn, &diri->owned_files, diri_files_list) {
			snprintf(name, sizeof(name), "%s/%s",
				 diri->dir->dirname,
				 file->filename);

			key = (struct apk_db_file_hash_key) {
				.dirname = APK_BLOB_STR(diri->dir->dirname),
				.filename = APK_BLOB_STR(file->filename),
			};
			apk_hash_delete(&db->installed.files,
					APK_BLOB_BUF(&key));
			unlink(name);
			__hlist_del(fc, &diri->owned_files.first);
			file->diri = NULL;
			db->installed.stats.files--;
		}
		apk_db_diri_rmdir(diri);
		apk_db_dir_unref(db, diri->dir);
		__hlist_del(dc, &pkg->owned_dirs.first);
	}
	apk_pkg_set_state(db, pkg, APK_STATE_NO_INSTALL);
}

int apk_db_install_pkg(struct apk_database *db,
		       struct apk_package *oldpkg,
		       struct apk_package *newpkg,
		       apk_progress_cb cb, void *cb_ctx)
{
	struct apk_bstream *bs;
	struct install_ctx ctx;
	csum_t csum;
	char file[256];
	int r, i;

	if (fchdir(db->root_fd) < 0)
		return errno;

	/* Just purging? */
	if (oldpkg != NULL && newpkg == NULL) {
		r = apk_pkg_run_script(oldpkg, db->root_fd,
				       APK_SCRIPT_PRE_DEINSTALL);
		if (r != 0)
			return r;

		apk_db_purge_pkg(db, oldpkg);

		r = apk_pkg_run_script(oldpkg, db->root_fd,
				       APK_SCRIPT_POST_DEINSTALL);
		return r;
	}

	/* Install the new stuff */
	if (newpkg->filename == NULL) {
		for (i = 0; i < APK_MAX_REPOS; i++)
			if (newpkg->repos & BIT(i))
				break;

		if (i >= APK_MAX_REPOS) {
			apk_error("%s-%s: not present in any repository",
				  newpkg->name->name, newpkg->version);
			return -1;
		}

		snprintf(file, sizeof(file),
			 "%s/%s-%s.apk",
			 db->repos[i].url,
			 newpkg->name->name, newpkg->version);
		bs = apk_bstream_from_url(file);
	} else
		bs = apk_bstream_from_file(newpkg->filename);

	if (bs == NULL) {
		apk_error("%s: %s", file, strerror(errno));
		return errno;
	}

	ctx = (struct install_ctx) {
		.db = db,
		.pkg = newpkg,
		.script = (oldpkg == NULL) ?
			APK_SCRIPT_PRE_INSTALL : APK_SCRIPT_PRE_UPGRADE,
		.cb = cb,
		.cb_ctx = cb_ctx,
	};
	if (apk_parse_tar_gz(bs, apk_db_install_archive_entry, &ctx) != 0)
		goto err_close;

	bs->close(bs, csum, NULL);

	apk_pkg_set_state(db, newpkg, APK_STATE_INSTALL);

	if (memcmp(csum, newpkg->csum, sizeof(csum)) != 0)
		apk_warning("%s-%s: checksum does not match",
			    newpkg->name->name, newpkg->version);

	if (oldpkg != NULL)
		apk_db_purge_pkg(db, oldpkg);

	r = apk_pkg_run_script(newpkg, db->root_fd,
			       (oldpkg == NULL) ?
			       APK_SCRIPT_POST_INSTALL : APK_SCRIPT_POST_UPGRADE);
	if (r != 0) {
		apk_error("%s-%s: Failed to execute post-install/upgrade script",
			  newpkg->name->name, newpkg->version);
	}
	return r;
err_close:
	bs->close(bs, NULL, NULL);
	return -1;
}
