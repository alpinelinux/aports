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
#include <signal.h>
#include <sys/file.h>

#include "apk_defines.h"
#include "apk_package.h"
#include "apk_database.h"
#include "apk_state.h"
#include "apk_applet.h"
#include "apk_archive.h"

const char * const apkindex_tar_gz = "APKINDEX.tar.gz";
const char * const apk_index_gz = "APK_INDEX.gz";
static const char * const apk_static_cache_dir = "var/lib/apk";
static const char * const apk_linked_cache_dir = "etc/apk/cache";

struct install_ctx {
	struct apk_database *db;
	struct apk_package *pkg;

	int script;
	int script_pending : 1;
	struct apk_db_dir_instance *diri;
	struct apk_checksum data_csum;
	struct apk_sign_ctx sctx;

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
	return APK_BLOB_CSUM(((struct apk_package *) item)->csum);
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
	struct apk_db_dir *dir = (struct apk_db_dir *) item;
	return APK_BLOB_PTR_LEN(dir->name, dir->namelen);
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

	return apk_blob_hash_seed(key->filename, apk_blob_hash(key->dirname));
}

static unsigned long apk_db_file_hash_item(apk_hash_item item)
{
	struct apk_db_file *dbf = (struct apk_db_file *) item;

	return apk_blob_hash_seed(APK_BLOB_PTR_LEN(dbf->name, dbf->namelen),
				  dbf->diri->dir->hash);
}

static int apk_db_file_compare_item(apk_hash_item item, apk_blob_t _key)
{
	struct apk_db_file *dbf = (struct apk_db_file *) item;
	struct apk_db_file_hash_key *key = (struct apk_db_file_hash_key *) _key.ptr;
	struct apk_db_dir *dir = dbf->diri->dir;
	int r;

	r = apk_blob_compare(key->filename,
			     APK_BLOB_PTR_LEN(dbf->name, dbf->namelen));
	if (r != 0)
		return r;

	r = apk_blob_compare(key->dirname,
			     APK_BLOB_PTR_LEN(dir->name, dir->namelen));
	return r;
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
	unsigned long hash = apk_hash_from_key(&db->available.names, name);

	pn = (struct apk_name *) apk_hash_get_hashed(&db->available.names, name, hash);
	if (pn != NULL)
		return pn;

	pn = calloc(1, sizeof(struct apk_name));
	if (pn == NULL)
		return NULL;

	pn->name = apk_blob_cstr(name);
	pn->id = db->name_id++;
	apk_hash_insert_hashed(&db->available.names, pn, hash);

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
	unsigned long hash = apk_hash_from_key(&db->installed.dirs, name);
	int i;

	if (name.len && name.ptr[name.len-1] == '/')
		name.len--;

	dir = (struct apk_db_dir *) apk_hash_get_hashed(&db->installed.dirs, name, hash);
	if (dir != NULL)
		return apk_db_dir_ref(dir);

	db->installed.stats.dirs++;
	dir = malloc(sizeof(*dir) + name.len + 1);
	memset(dir, 0, sizeof(*dir));
	dir->refs = 1;
	memcpy(dir->name, name.ptr, name.len);
	dir->name[name.len] = 0;
	dir->namelen = name.len;
	dir->hash = hash;
	apk_hash_insert_hashed(&db->installed.dirs, dir, hash);

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
		    strcmp(&db->protected_paths->item[i][1], dir->name) == 0)
			dir->flags &= ~APK_DBDIRF_PROTECTED;
		else if (strcmp(db->protected_paths->item[i], dir->name) == 0)
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
	if (mkdir(diri->dir->name, diri->mode) == 0)
		chown(diri->dir->name, diri->uid, diri->gid);
}

static void apk_db_diri_rmdir(struct apk_db_dir_instance *diri)
{
	if (diri->dir->refs == 1)
		rmdir(diri->dir->name);
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

static struct apk_db_file *apk_db_file_new(struct apk_db_dir_instance *diri,
					   apk_blob_t name,
					   struct hlist_node ***after)
{
	struct apk_db_file *file;

	file = malloc(sizeof(*file) + name.len + 1);
	if (file == NULL)
		return NULL;

	memset(file, 0, sizeof(*file));
	memcpy(file->name, name.ptr, name.len);
	file->name[name.len] = 0;
	file->namelen = name.len;

	file->diri = diri;
	hlist_add_after(&file->diri_files_list, *after);
	*after = &file->diri_files_list.next;

	return file;
}

static struct apk_db_file *apk_db_file_get(struct apk_database *db,
					   struct apk_db_dir_instance *diri,
					   apk_blob_t name,
					   struct hlist_node ***after)
{
	struct apk_db_file *file;
	struct apk_db_file_hash_key key;
	struct apk_db_dir *dir = diri->dir;
	unsigned long hash;

	key = (struct apk_db_file_hash_key) {
		.dirname = APK_BLOB_PTR_LEN(dir->name, dir->namelen),
		.filename = name,
	};

	hash = apk_blob_hash_seed(name, dir->hash);
	file = (struct apk_db_file *) apk_hash_get_hashed(
		&db->installed.files, APK_BLOB_BUF(&key), hash);
	if (file != NULL)
		return file;

	file = apk_db_file_new(diri, name, after);
	apk_hash_insert_hashed(&db->installed.files, file, hash);
	db->installed.stats.files++;

	return file;
}

static void apk_db_pkg_rdepends(struct apk_database *db, struct apk_package *pkg)
{
	int i, j;

	if (pkg->depends == NULL)
		return;

	for (i = 0; i < pkg->depends->num; i++) {
		struct apk_name *rname = pkg->depends->item[i].name;

		if (rname->rdepends) {
			for (j = 0; j < rname->rdepends->num; j++)
				if (rname->rdepends->item[j] == pkg->name)
					return;
		}
		*apk_name_array_add(&rname->rdepends) = pkg->name;
	}
}

struct apk_package *apk_db_pkg_add(struct apk_database *db, struct apk_package *pkg)
{
	struct apk_package *idb;

	idb = apk_hash_get(&db->available.packages, APK_BLOB_CSUM(pkg->csum));
	if (idb == NULL) {
		idb = pkg;
		apk_hash_insert(&db->available.packages, pkg);
		*apk_package_array_add(&pkg->name->pkgs) = pkg;
		apk_db_pkg_rdepends(db, pkg);
	} else {
		idb->repos |= pkg->repos;
		if (idb->filename == NULL && pkg->filename != NULL) {
			idb->filename = pkg->filename;
			pkg->filename = NULL;
		}
		apk_pkg_free(pkg);
	}
	return idb;
}

int apk_db_index_read(struct apk_database *db, struct apk_bstream *bs, int repo)
{
	struct apk_package *pkg = NULL;
	struct apk_db_dir_instance *diri = NULL;
	struct apk_db_file *file = NULL;
	struct hlist_node **diri_node = NULL;
	struct hlist_node **file_diri_node = NULL;
	apk_blob_t token = APK_BLOB_STR("\n"), l;
	int field;

	while (!APK_BLOB_IS_NULL(l = bs->read(bs, token))) {
		if (l.len < 2 || l.ptr[1] != ':') {
			if (pkg == NULL)
				continue;

			if (repo != -1)
				pkg->repos |= BIT(repo);
			else
				apk_pkg_set_state(db, pkg, APK_PKG_INSTALLED);

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
			diri->uid = apk_blob_pull_uint(&l, 10);
			apk_blob_pull_char(&l, ':');
			diri->gid = apk_blob_pull_uint(&l, 10);
			apk_blob_pull_char(&l, ':');
			diri->mode = apk_blob_pull_uint(&l, 8);
			break;
		case 'R':
			if (diri == NULL) {
				apk_error("FDB file entry before directory entry");
				return -1;
			}
			file = apk_db_file_get(db, diri, l, &file_diri_node);
			break;
		case 'Z':
			if (file == NULL) {
				apk_error("FDB checksum entry before file entry");
				return -1;
			}
			apk_blob_pull_csum(&l, &file->csum);
			break;
		default:
			apk_error("FDB entry '%c' unsupported", field);
			return -1;
		}
		if (APK_BLOB_IS_NULL(l)) {
			apk_error("FDB format error in entry '%c'", field);
			return -1;
		}
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
	apk_blob_t bbuf = APK_BLOB_BUF(buf);
	int r;

	list_for_each_entry(pkg, &db->installed.packages, installed_pkgs_list) {
		r = apk_pkg_write_index_entry(pkg, os);
		if (r < 0)
			return r;

		hlist_for_each_entry(diri, c1, &pkg->owned_dirs, pkg_dirs_list) {
			apk_blob_push_blob(&bbuf, APK_BLOB_STR("F:"));
			apk_blob_push_blob(&bbuf, APK_BLOB_PTR_LEN(diri->dir->name, diri->dir->namelen));
			apk_blob_push_blob(&bbuf, APK_BLOB_STR("\nM:"));
			apk_blob_push_uint(&bbuf, diri->uid, 10);
			apk_blob_push_blob(&bbuf, APK_BLOB_STR(":"));
			apk_blob_push_uint(&bbuf, diri->gid, 10);
			apk_blob_push_blob(&bbuf, APK_BLOB_STR(":"));
			apk_blob_push_uint(&bbuf, diri->mode, 8);
			apk_blob_push_blob(&bbuf, APK_BLOB_STR("\n"));

			hlist_for_each_entry(file, c2, &diri->owned_files, diri_files_list) {
				apk_blob_push_blob(&bbuf, APK_BLOB_STR("R:"));
				apk_blob_push_blob(&bbuf, APK_BLOB_PTR_LEN(file->name, file->namelen));
				if (file->csum.type != APK_CHECKSUM_NONE) {
					apk_blob_push_blob(&bbuf, APK_BLOB_STR("\nZ:"));
					apk_blob_push_csum(&bbuf, &file->csum);
				}
				apk_blob_push_blob(&bbuf, APK_BLOB_STR("\n"));

				if (os->write(os, buf, bbuf.ptr - buf) != bbuf.ptr - buf)
					return -1;
				bbuf = APK_BLOB_BUF(buf);
			}
			if (os->write(os, buf, bbuf.ptr - buf) != bbuf.ptr - buf)
				return -1;
			bbuf = APK_BLOB_BUF(buf);
		}
		os->write(os, "\n", 1);
	}

	return 0;
}

static int apk_db_scriptdb_write(struct apk_database *db, struct apk_ostream *os)
{
	struct apk_package *pkg;
	struct apk_script *script;
	struct hlist_node *c2;
	struct apk_file_info fi;
	char filename[256];
	apk_blob_t bfn;
	int r;

	list_for_each_entry(pkg, &db->installed.packages, installed_pkgs_list) {
		hlist_for_each_entry(script, c2, &pkg->scripts, script_list) {
			fi = (struct apk_file_info) {
				.name = filename,
				.size = script->size,
				.mode = 0755 | S_IFREG,
			};
			/* The scripts db expects file names in format:
			 * pkg-version.<hexdump of package checksum>.action */
			bfn = APK_BLOB_BUF(filename);
			apk_blob_push_blob(&bfn, APK_BLOB_STR(pkg->name->name));
			apk_blob_push_blob(&bfn, APK_BLOB_STR("-"));
			apk_blob_push_blob(&bfn, APK_BLOB_STR(pkg->version));
			apk_blob_push_blob(&bfn, APK_BLOB_STR("."));
			apk_blob_push_csum(&bfn, &pkg->csum);
			apk_blob_push_blob(&bfn, APK_BLOB_STR("."));
			apk_blob_push_blob(&bfn, APK_BLOB_STR(apk_script_types[script->type]));
			apk_blob_push_blob(&bfn, APK_BLOB_PTR_LEN("", 1));

			r = apk_tar_write_entry(os, &fi, script->script);
			if (r < 0)
				return r;
		}
	}

	return apk_tar_write_entry(os, NULL, NULL);
}

static int apk_db_scriptdb_read_v1(struct apk_database *db, struct apk_istream *is)
{
	struct apk_package *pkg;
	struct {
		unsigned char md5sum[16];
		unsigned int type;
		unsigned int size;
	} hdr;
	struct apk_checksum csum;

	while (is->read(is, &hdr, sizeof(hdr)) == sizeof(hdr)) {
		memcpy(csum.data, hdr.md5sum, sizeof(hdr.md5sum));
		csum.type = APK_CHECKSUM_MD5;

		pkg = apk_db_get_pkg(db, &csum);
		if (pkg != NULL)
			apk_pkg_add_script(pkg, is, hdr.type, hdr.size);
		else
			apk_istream_skip(is, hdr.size);
	}

	return 0;
}

static int apk_read_script_archive_entry(void *ctx,
					 const struct apk_file_info *ae,
					 struct apk_istream *is)
{
	struct apk_database *db = (struct apk_database *) ctx;
	struct apk_package *pkg;
	char *fncsum, *fnaction;
	struct apk_checksum csum;
	apk_blob_t blob;
	int type;

	if (!S_ISREG(ae->mode))
		return 0;

	/* The scripts db expects file names in format:
	 * pkgname-version.<hexdump of package checksum>.action */
	fnaction = memrchr(ae->name, '.', strlen(ae->name));
	if (fnaction == NULL || fnaction == ae->name)
		return 0;
	fncsum = memrchr(ae->name, '.', fnaction - ae->name - 1);
	if (fncsum == NULL)
		return 0;
	fnaction++;
	fncsum++;

	/* Parse it */
	type = apk_script_type(fnaction);
	if (type == APK_SCRIPT_INVALID)
		return 0;
	blob = APK_BLOB_PTR_PTR(fncsum, fnaction - 2);
	apk_blob_pull_csum(&blob, &csum);

	/* Attach script */
	pkg = apk_db_get_pkg(db, &csum);
	if (pkg != NULL)
		apk_pkg_add_script(pkg, is, type, ae->size);

	return 0;
}

static int apk_db_read_state(struct apk_database *db, int flags)
{
	struct apk_istream *is;
	struct apk_bstream *bs;
	apk_blob_t blob;
	int i;

	/* Read:
	 * 1. installed repository
	 * 2. source repositories
	 * 3. master dependencies
	 * 4. package statuses
	 * 5. files db
	 * 6. script db
	 */
	fchdir(db->root_fd);

	if (!(flags & APK_OPENF_NO_WORLD)) {
		blob = apk_blob_from_file("var/lib/apk/world");
		if (APK_BLOB_IS_NULL(blob))
			return -ENOENT;
		apk_deps_parse(db, &db->world, blob);
		free(blob.ptr);

		for (i = 0; i < db->world->num; i++)
			db->world->item[i].name->flags |= APK_NAME_TOPLEVEL;
	}

	if (!(flags & APK_OPENF_NO_INSTALLED)) {
		bs = apk_bstream_from_file("var/lib/apk/installed");
		if (bs != NULL) {
			apk_db_index_read(db, bs, -1);
			bs->close(bs, NULL);
		}
	}

	if (!(flags & APK_OPENF_NO_SCRIPTS)) {
		is = apk_istream_from_file("var/lib/apk/scripts.tar");
		if (is != NULL) {
			apk_tar_parse(is, apk_read_script_archive_entry, db);
		} else {
			is = apk_istream_from_file("var/lib/apk/scripts");
			if (is != NULL)
				apk_db_scriptdb_read_v1(db, is);
		}
		if (is != NULL)
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

static void handle_alarm(int sig)
{
}

int apk_db_open(struct apk_database *db, const char *root, unsigned int flags)
{
	const char *apk_repos = getenv("APK_REPOS"), *msg = NULL;
	struct apk_repository_url *repo = NULL;
	struct stat st;
	apk_blob_t blob;
	int r;

	memset(db, 0, sizeof(*db));
	apk_hash_init(&db->available.names, &pkg_name_hash_ops, 1000);
	apk_hash_init(&db->available.packages, &pkg_info_hash_ops, 4000);
	apk_hash_init(&db->installed.dirs, &dir_hash_ops, 2000);
	apk_hash_init(&db->installed.files, &file_hash_ops, 10000);
	list_init(&db->installed.packages);
	db->cache_dir = apk_static_cache_dir;

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
		if (stat(apk_linked_cache_dir, &st) == 0 && S_ISDIR(st.st_mode))
			db->cache_dir = apk_linked_cache_dir;

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
				if (apk_wait) {
					struct sigaction sa, old_sa;

					apk_message("Waiting for repository lock");
					memset(&sa, 0, sizeof sa);
					sa.sa_handler = handle_alarm;
					sa.sa_flags   = SA_ONESHOT;
					sigaction(SIGALRM, &sa, &old_sa);

					alarm(apk_wait);
					if (flock(db->lock_fd, LOCK_EX) < 0) {
						msg = "Unable to lock database";
						goto ret_errno;
					}

					alarm(0);
					sigaction(SIGALRM, &old_sa, NULL);
				}
			}
		}
	}

	blob = APK_BLOB_STR("etc:-etc/init.d");
	apk_blob_for_each_segment(blob, ":", add_protected_path, db);

	if (root != NULL) {
		r = apk_db_read_state(db, flags);
		if (r == -ENOENT && (flags & APK_OPENF_CREATE)) {
			r = apk_db_create(db);
			if (r != 0) {
				msg = "Unable to create database";
				goto ret_r;
			}
			r = apk_db_read_state(db, flags);
		}
		if (r != 0) {
			msg = "Unable to read database state";
			goto ret_r;
		}

		if (!(flags & APK_OPENF_NO_REPOS)) {
			if (apk_repos == NULL)
				apk_repos = "/etc/apk/repositories";
			blob = apk_blob_from_file(apk_repos);
			if (!APK_BLOB_IS_NULL(blob)) {
				apk_blob_for_each_segment(blob, "\n",
							  apk_db_add_repository, db);
				free(blob.ptr);
			}
		}
	}

	if (!(flags & APK_OPENF_NO_REPOS)) {
		list_for_each_entry(repo, &apk_repository_list.list, list)
			apk_db_add_repository(db, APK_BLOB_STR(repo->url));
	}

	fchdir(apk_cwd_fd);
	return 0;

ret_errno:
	r = -errno;
ret_r:
	if (msg != NULL)
		apk_error("%s: %s", msg, strerror(-r));
	apk_db_close(db);
	fchdir(apk_cwd_fd);
	return r;
}

struct write_ctx {
	struct apk_database *db;
	int fd;
};

int apk_db_write_config(struct apk_database *db)
{
	struct apk_ostream *os;

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
	apk_deps_write(db->world, os);
	os->write(os, "\n", 1);
	os->close(os);

	os = apk_ostream_to_file("var/lib/apk/installed.new", 0644);
	if (os == NULL)
		return -1;
	apk_db_write_fdb(db, os);
	os->close(os);

	if (rename("var/lib/apk/installed.new", "var/lib/apk/installed") < 0)
		return -errno;

	os = apk_ostream_to_file("var/lib/apk/scripts.tar.new", 0644);
	if (os == NULL)
		return -1;
	apk_db_scriptdb_write(db, os);
	os->close(os);
	if (rename("var/lib/apk/scripts.tar.new", "var/lib/apk/scripts.tar") < 0)
		return -errno;

	unlink("var/lib/apk/scripts");

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

	for (i = 0; i < db->num_repos; i++) {
		free(db->repos[i].url);
	}
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

int apk_db_cache_active(struct apk_database *db)
{
	return db->cache_dir != apk_static_cache_dir;
}

struct apk_package *apk_db_get_pkg(struct apk_database *db,
				   struct apk_checksum *csum)
{
	return apk_hash_get(&db->available.packages, APK_BLOB_CSUM(*csum));
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

struct index_write_ctx {
	struct apk_ostream *os;
	int count;
};

static int write_index_entry(apk_hash_item item, void *ctx)
{
	struct index_write_ctx *iwctx = (struct index_write_ctx *) ctx;
	struct apk_package *pkg = (struct apk_package *) item;
	int r;

	if (pkg->filename == NULL)
		return 0;

	r = apk_pkg_write_index_entry(pkg, iwctx->os);
	if (r < 0)
		return r;

	if (iwctx->os->write(iwctx->os, "\n", 1) != 1)
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

static void apk_db_cache_get_name(char *buf, size_t bufsz,
				  struct apk_database *db,
				  struct apk_checksum *csum,
				  const char *file, int temp)
{
	char csumstr[APK_CACHE_CSUM_BYTES*2+1];
	apk_blob_t bbuf = APK_BLOB_BUF(csumstr);

	apk_blob_push_hexdump(&bbuf,
			      APK_BLOB_PTR_LEN((char *) csum->data, APK_CACHE_CSUM_BYTES));
	apk_blob_push_blob(&bbuf, APK_BLOB_PTR_LEN("", 1));

	snprintf(buf, bufsz, "%s/%s/%s.%s%s",
		 db->root, db->cache_dir, csumstr, file, temp ? ".new" : "");
}

static struct apk_bstream *apk_db_cache_open(struct apk_database *db,
					     struct apk_checksum *csum,
					     const char *file)
{
	char tmp[256];

	if (db->root == NULL)
		return NULL;

	apk_db_cache_get_name(tmp, sizeof(tmp), db, csum, file, FALSE);
	return apk_bstream_from_file(tmp);
}

static struct apk_bstream *apk_repository_file_open(struct apk_repository *repo,
						    const char *file)
{
	char tmp[256];
	const char *url = repo->url;

	snprintf(tmp, sizeof(tmp), "%s%s%s",
		 url, url[strlen(url)-1] == '/' ? "" : "/", file);

	return apk_bstream_from_url(tmp);
}

int apk_cache_download(struct apk_database *db, struct apk_checksum *csum,
		       const char *url, const char *item, int verify)
{
	char tmp[256], tmp2[256];
	int r;

	snprintf(tmp, sizeof(tmp), "%s%s%s",
		 url, url[strlen(url)-1] == '/' ? "" : "/", item);
	apk_message("fetch %s", tmp);

	if (apk_flags & APK_SIMULATE)
		return 0;

	apk_db_cache_get_name(tmp2, sizeof(tmp2), db, csum, item, TRUE);
	r = apk_url_download(tmp, tmp2);
	if (r < 0)
		return r;

	if (verify != APK_SIGN_NONE) {
		struct apk_istream *is;
		struct apk_sign_ctx sctx;
		int ok;

		apk_sign_ctx_init(&sctx, APK_SIGN_VERIFY, NULL);
		is = apk_bstream_gunzip_mpart(apk_bstream_from_file(tmp2),
			apk_sign_ctx_mpart_cb, &sctx);
		r = apk_tar_parse(is, apk_sign_ctx_verify_tar, &sctx);
		is->close(is);
		ok = (r == 0) && sctx.control_verified && sctx.data_verified;
		apk_sign_ctx_free(&sctx);
		if (!ok) {
			unlink(tmp2);
			return -10;
		}
	}

	apk_db_cache_get_name(tmp, sizeof(tmp), db, csum, item, FALSE);
	if (rename(tmp2, tmp) < 0)
		return -errno;

	return 0;
}

int apk_cache_exists(struct apk_database *db, struct apk_checksum *csum,
		     const char *item)
{
	char tmp[256];

	if (db->root == NULL)
		return 0;

	apk_db_cache_get_name(tmp, sizeof(tmp), db, csum, item, FALSE);
	return access(tmp, R_OK | W_OK) == 0;
}

static int apk_cache_delete(struct apk_database *db, struct apk_checksum *csum,
			    const char *item)
{
	char tmp[256];

	if (db->root == NULL)
		return 0;

	apk_db_cache_get_name(tmp, sizeof(tmp), db, csum, item, FALSE);
	return unlink(tmp);
}

int apk_repository_update(struct apk_database *db, struct apk_repository *repo)
{
	int r;

	if (repo->csum.type == APK_CHECKSUM_NONE)
		return 0;

	r = apk_cache_download(db, &repo->csum, repo->url, apkindex_tar_gz,
			       APK_SIGN_VERIFY);
	if (r == 0 || r == -10) {
		if (r == -10)
			apk_error("%s: untrusted or bad signature!", repo->url);
		apk_cache_delete(db, &repo->csum, apk_index_gz);
		return r;
	}

	r = apk_cache_download(db, &repo->csum, repo->url, apk_index_gz,
			       APK_SIGN_NONE);
	if (r != 0)
		apk_error("Failed to update %s: download failed");
	return r;
}

struct apkindex_ctx {
	struct apk_database *db;
	struct apk_sign_ctx sctx;
	int repo;
};

static int load_apkindex(void *sctx, const struct apk_file_info *fi,
			 struct apk_istream *is)
{
	struct apkindex_ctx *ctx = (struct apkindex_ctx *) sctx;
	struct apk_bstream *bs;

	if (apk_sign_ctx_process_file(&ctx->sctx, fi, is) == 0)
		return 0;

	if (strcmp(fi->name, "APKINDEX") != 0)
		return 0;

	bs = apk_bstream_from_istream(is);
	apk_db_index_read(ctx->db, bs, ctx->repo);
	bs->close(bs, NULL);

	return 0;
}

static int load_index(struct apk_database *db, struct apk_bstream *bs,
		      int targz, int repo)
{
	if (targz) {
		struct apk_istream *is;
		struct apkindex_ctx ctx;

		ctx.db = db;
		ctx.repo = repo;
		apk_sign_ctx_init(&ctx.sctx, APK_SIGN_VERIFY, NULL);
		is = apk_bstream_gunzip_mpart(bs, apk_sign_ctx_mpart_cb, &ctx.sctx);
		apk_tar_parse(is, load_apkindex, &ctx);
		is->close(is);
		apk_sign_ctx_free(&ctx.sctx);
		if (!ctx.sctx.data_verified)
			return -1;
	} else {
		bs = apk_bstream_from_istream(apk_bstream_gunzip(bs));
		apk_db_index_read(db, bs, repo);
		bs->close(bs, NULL);
	}
	return 0;
}

int apk_db_index_read_file(struct apk_database *db, const char *file, int repo)
{
	int targz = 1;

	if (strstr(file, ".tar.gz") == NULL && strstr(file, ".gz") != NULL)
		targz = 0;

	return load_index(db, apk_bstream_from_file(file), targz, repo);
}

int apk_db_add_repository(apk_database_t _db, apk_blob_t repository)
{
	struct apk_database *db = _db.db;
	struct apk_bstream *bs = NULL;
	struct apk_repository *repo;
	int r, targz = 1;

	if (repository.ptr == NULL || *repository.ptr == '\0'
			|| *repository.ptr == '#')
		return 0;

	if (db->num_repos >= APK_MAX_REPOS)
		return -1;

	r = db->num_repos++;

	repo = &db->repos[r];
	*repo = (struct apk_repository) {
		.url = apk_blob_cstr(repository),
	};

	if (apk_url_local_file(repo->url) == NULL) {
		apk_blob_checksum(repository, apk_default_checksum(), &repo->csum);

		if (apk_flags & APK_UPDATE_CACHE)
			apk_repository_update(db, repo);

		bs = apk_db_cache_open(db, &repo->csum, apkindex_tar_gz);
		if (bs == NULL) {
			bs = apk_db_cache_open(db, &repo->csum, apk_index_gz);
			targz = 0;
		}
	} else {
		bs = apk_repository_file_open(repo, apkindex_tar_gz);
		if (bs == NULL) {
			bs = apk_repository_file_open(repo, apk_index_gz);
			targz = 0;
		}
	}
	if (bs == NULL) {
		apk_warning("Failed to open index for %s", repo->url);
		return -1;
	}

	r = load_index(db, bs, targz, r);
	if (r != 0)
		apk_error("%s: Bad repository signature", repo->url);
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

static int apk_db_run_pending_script(struct install_ctx *ctx)
{
	int r;

	if (!ctx->script_pending)
		return 0;
	if (!ctx->sctx.control_verified)
		return 0;

	ctx->script_pending = FALSE;
	r = apk_pkg_run_script(ctx->pkg, ctx->db->root_fd, ctx->script);
	if (r != 0)
		apk_error("%s-%s: Failed to execute "
			  "pre-install/upgrade script",
			  ctx->pkg->name->name, ctx->pkg->version);
	return r;
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
	char alt_name[PATH_MAX];
	const char *p;
	int r = 0, type = APK_SCRIPT_INVALID;

	apk_sign_ctx_verify_tar(&ctx->sctx, ae, is);

	/* Package metainfo and script processing */
	if (ae->name[0] == '.') {
		/* APK 2.0 format */
		type = apk_script_type(&ae->name[1]);
		if (type == APK_SCRIPT_INVALID)
			return 0;
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
		if (type == ctx->script)
			ctx->script_pending = TRUE;
		return apk_db_run_pending_script(ctx);
	}
	r = apk_db_run_pending_script(ctx);
	if (r != 0)
		return r;

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
		    apk_blob_compare(APK_BLOB_PTR_LEN(diri->dir->name,
						      diri->dir->namelen),
				     bdir) != 0) {
			struct hlist_node *n;

			hlist_for_each_entry(diri, n, &pkg->owned_dirs, pkg_dirs_list) {
				if (apk_blob_compare(APK_BLOB_PTR_LEN(diri->dir->name, diri->dir->namelen), bdir) == 0)
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

		file = apk_db_file_query(db, bdir, bfile);
		if (file != NULL) {
			opkg = file->diri->pkg;
			if (opkg->name != pkg->name) {
				if (!(apk_flags & APK_FORCE)) {
					apk_error("%s: Trying to overwrite %s "
						  "owned by %s.",
						  pkg->name->name, ae->name,
						  opkg->name->name);
					return -1;
				}
				apk_warning("%s: Overwriting %s owned by %s.",
					    pkg->name->name, ae->name,
					    opkg->name->name);
			}
		}

		/* Create the file entry without adding it to hash */
		file = apk_db_file_new(diri, bfile, &ctx->file_diri_node);

		if (apk_verbosity > 1)
			printf("%s\n", ae->name);

		/* Extract the file as name.apk-new */
		snprintf(alt_name, sizeof(alt_name), "%s/%s.apk-new",
			 diri->dir->name, file->name);
		r = apk_archive_entry_extract(ae, is, alt_name,
					      extract_cb, ctx);
	} else {
		if (apk_verbosity > 1)
			printf("%s\n", ae->name);

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

static void apk_db_purge_pkg(struct apk_database *db, struct apk_package *pkg,
			     const char *exten)
{
	struct apk_db_dir_instance *diri;
	struct apk_db_file *file;
	struct apk_db_file_hash_key key;
	struct hlist_node *dc, *dn, *fc, *fn;
	unsigned long hash;
	char name[1024];

	hlist_for_each_entry_safe(diri, dc, dn, &pkg->owned_dirs, pkg_dirs_list) {
		hlist_for_each_entry_safe(file, fc, fn, &diri->owned_files, diri_files_list) {
			snprintf(name, sizeof(name), "%s/%s%s",
				 diri->dir->name, file->name, exten ?: "");

			key = (struct apk_db_file_hash_key) {
				.dirname = APK_BLOB_PTR_LEN(diri->dir->name, diri->dir->namelen),
				.filename = APK_BLOB_PTR_LEN(file->name, file->namelen),
			};
			hash = apk_blob_hash_seed(key.filename, diri->dir->hash);
			unlink(name);
			if (apk_verbosity > 1)
				printf("%s\n", name);
			__hlist_del(fc, &diri->owned_files.first);
			if (exten == NULL) {
				apk_hash_delete_hashed(&db->installed.files, APK_BLOB_BUF(&key), hash);
				db->installed.stats.files--;
			}
		}
		apk_db_diri_rmdir(diri);
		__hlist_del(dc, &pkg->owned_dirs.first);
		apk_db_diri_free(db, diri);
	}
	apk_pkg_set_state(db, pkg, APK_PKG_NOT_INSTALLED);
}


static void apk_db_migrate_files(struct apk_database *db,
				 struct apk_package *pkg)
{
	struct apk_db_dir_instance *diri;
	struct apk_db_dir *dir;
	struct apk_db_file *file, *ofile;
	struct apk_db_file_hash_key key;
	struct apk_file_info fi;
	struct hlist_node *dc, *dn, *fc, *fn;
	unsigned long hash;
	char name[1024], tmpname[1024];

	hlist_for_each_entry_safe(diri, dc, dn, &pkg->owned_dirs, pkg_dirs_list) {
		dir = diri->dir;

		hlist_for_each_entry_safe(file, fc, fn, &diri->owned_files, diri_files_list) {
			snprintf(name, sizeof(name), "%s/%s",
				 diri->dir->name, file->name);
			snprintf(tmpname, sizeof(tmpname), "%s/%s.apk-new",
				 diri->dir->name, file->name);

			key = (struct apk_db_file_hash_key) {
				.dirname = APK_BLOB_PTR_LEN(dir->name, dir->namelen),
				.filename = APK_BLOB_PTR_LEN(file->name, file->namelen),
			};

			hash = apk_blob_hash_seed(key.filename, dir->hash);

			/* check for existing file */
			ofile = (struct apk_db_file *) apk_hash_get_hashed(
				&db->installed.files, APK_BLOB_BUF(&key), hash);

			r = apk_file_get_info(name,
				ofile ? ofile->csum.type : APK_CHECKSUM_NONE,
				&fi);
			if ((diri->dir->flags & APK_DBDIRF_PROTECTED) &&
			    (r == 0) &&
			    (ofile == NULL ||
			     apk_checksum_compare(&ofile->csum, &fi.csum) == 0)) {
				/* Protected directory, with file without
				 * db entry, or local modifications.
				 *
				 * Delete the apk-new if it's identical with the
				 * existing file */
				if (ofile == NULL ||
				    ofile->csum.type != file->csum.type)
					apk_file_get_info(name, file->csum.type, &fi);
				if (apk_checksum_compare(&file->csum, &fi.csum) == 0)
					unlink(tmpname);
			} else {
				/* Overwrite the old file */
				rename(tmpname, name);
			}

			/* Claim ownership of the file in db */
			if (ofile != NULL) {
				hlist_del(&ofile->diri_files_list,
					  &diri->owned_files);
				apk_hash_delete_hashed(&db->installed.files,
						       APK_BLOB_BUF(&key), hash);
			} else
				db->installed.stats.files++;

			apk_hash_insert_hashed(&db->installed.files, file, hash);
		}
	}
}

static int apk_db_unpack_pkg(struct apk_database *db,
			     struct apk_package *newpkg,
			     int upgrade, apk_progress_cb cb, void *cb_ctx)
{
	struct install_ctx ctx;
	struct apk_bstream *bs = NULL;
	struct apk_istream *tar;
	char pkgname[256], file[256];
	int r, i, need_copy = FALSE;

	snprintf(pkgname, sizeof(pkgname), "%s-%s.apk",
		 newpkg->name->name, newpkg->version);

	if (newpkg->filename == NULL) {
		struct apk_repository *repo;

		for (i = 0; i < APK_MAX_REPOS; i++)
			if (newpkg->repos & BIT(i))
				break;

		if (i >= APK_MAX_REPOS) {
			apk_error("%s-%s: not present in any repository",
				  newpkg->name->name, newpkg->version);
			return -1;
		}

		repo = &db->repos[i];
		if (apk_db_cache_active(db) &&
		    repo->csum.type != APK_CHECKSUM_NONE)
			bs = apk_db_cache_open(db, &newpkg->csum, pkgname);

		if (bs == NULL) {
			bs = apk_repository_file_open(repo, pkgname);
			if (repo->csum.type != APK_CHECKSUM_NONE)
				need_copy = TRUE;
		}
	} else {
		bs = apk_bstream_from_file(newpkg->filename);
		need_copy = TRUE;
	}
	if (!apk_db_cache_active(db))
		need_copy = FALSE;
	if (need_copy) {
		apk_db_cache_get_name(file, sizeof(file), db, &newpkg->csum,
				      pkgname, TRUE);
		bs = apk_bstream_tee(bs, file);
	}

	if (bs == NULL) {
		apk_error("%s: %s", file, strerror(errno));
		return errno;
	}

	ctx = (struct install_ctx) {
		.db = db,
		.pkg = newpkg,
		.script = upgrade ?
			APK_SCRIPT_PRE_UPGRADE : APK_SCRIPT_PRE_INSTALL,
		.cb = cb,
		.cb_ctx = cb_ctx,
	};
	apk_sign_ctx_init(&ctx.sctx, APK_SIGN_VERIFY_IDENTITY, &newpkg->csum);
	tar = apk_bstream_gunzip_mpart(bs, apk_sign_ctx_mpart_cb, &ctx.sctx);
	r = apk_tar_parse(tar, apk_db_install_archive_entry, &ctx);
	apk_sign_ctx_free(&ctx.sctx);
	tar->close(tar);

	if (r != 0) {
		apk_error("%s-%s: package integrity check failed",
			  newpkg->name->name, newpkg->version);
		goto err;
	}
	r = apk_db_run_pending_script(&ctx);
	if (r != 0)
		goto err;

	apk_db_migrate_files(db, newpkg);

	if (need_copy) {
		char file2[256];
		apk_db_cache_get_name(file2, sizeof(file2), db,
				      &newpkg->csum, pkgname, FALSE);
		rename(file, file2);
	}

	return 0;
err:
	apk_db_purge_pkg(db, newpkg, ".apk-new");
	return r;
}

int apk_db_install_pkg(struct apk_database *db,
		       struct apk_package *oldpkg,
		       struct apk_package *newpkg,
		       apk_progress_cb cb, void *cb_ctx)
{
	int r;

	if (fchdir(db->root_fd) < 0)
		return errno;

	/* Just purging? */
	if (oldpkg != NULL && newpkg == NULL) {
		r = apk_pkg_run_script(oldpkg, db->root_fd,
				       APK_SCRIPT_PRE_DEINSTALL);
		if (r != 0)
			return r;

		apk_db_purge_pkg(db, oldpkg, NULL);

		r = apk_pkg_run_script(oldpkg, db->root_fd,
				       APK_SCRIPT_POST_DEINSTALL);
		return r;
	}

	/* Install the new stuff */
	if (!(newpkg->name->flags & APK_NAME_VIRTUAL)) {
		r = apk_db_unpack_pkg(db, newpkg, (oldpkg != NULL), cb, cb_ctx);
		if (r != 0)
			return r;
	}

	apk_pkg_set_state(db, newpkg, APK_PKG_INSTALLED);

	if (oldpkg != NULL)
		apk_db_purge_pkg(db, oldpkg, NULL);

	r = apk_pkg_run_script(newpkg, db->root_fd,
			       (oldpkg == NULL) ?
			       APK_SCRIPT_POST_INSTALL : APK_SCRIPT_POST_UPGRADE);
	if (r != 0) {
		apk_error("%s-%s: Failed to execute post-install/upgrade script",
			  newpkg->name->name, newpkg->version);
	}
	return r;
}
