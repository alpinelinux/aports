/* database.c - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2005-2008 Natanael Copa <n@tanael.org>
 * Copyright (C) 2008-2009 Timo Teräs <timo.teras@iki.fi>
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
#include <fnmatch.h>
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
	struct apk_installed_package *ipkg;

	int script;
	char **script_args;
	int script_pending : 1;

	struct apk_db_dir_instance *diri;
	struct apk_checksum data_csum;
	struct apk_sign_ctx sctx;
	struct apk_name_array *replaces;

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
	if (name->pkgs)
		free(name->pkgs);
	if (name->rdepends)
		free(name->rdepends);
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
	dir->rooted_name[0] = '/';
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
		int flags = dir->flags, j;

		flags |= APK_DBDIRF_PROTECTED;
		for (j = 0; ; j++) {
			switch (db->protected_paths->item[i][j]) {
			case '-':
				flags &= ~(APK_DBDIRF_PROTECTED |
					   APK_DBDIRF_SYMLINKS_ONLY);
				continue;
			case '*':
				flags |= APK_DBDIRF_SYMLINKS_ONLY |
					 APK_DBDIRF_PROTECTED;
				continue;
			}
			break;
		}

		if (strcmp(&db->protected_paths->item[i][j], dir->name) == 0)
			dir->flags = flags;
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

static void apk_db_diri_mkdir(struct apk_database *db, struct apk_db_dir_instance *diri)
{
	if (mkdirat(db->root_fd, diri->dir->name, diri->mode) == 0)
		fchownat(db->root_fd, diri->dir->name, diri->uid, diri->gid, 0);
}

static void apk_db_diri_rmdir(struct apk_database *db, struct apk_db_dir_instance *diri)
{
	if (diri->dir->refs == 1)
		unlinkat(db->root_fd, diri->dir->name, 1);
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
		if (idb->ipkg == NULL && pkg->ipkg != NULL) {
			idb->ipkg = pkg->ipkg;
			idb->ipkg->pkg = idb;
			pkg->ipkg = NULL;
		}
		apk_pkg_free(pkg);
	}
	return idb;
}

void apk_cache_format_index(apk_blob_t to, struct apk_repository *repo, int ver)
{
	/* APKINDEX.12345678.tar.gz */
	/* APK_INDEX.12345678.gz */
	if (ver == 0)
		apk_blob_push_blob(&to, APK_BLOB_STR("APKINDEX."));
	else
		apk_blob_push_blob(&to, APK_BLOB_STR("APK_INDEX."));
	apk_blob_push_hexdump(&to, APK_BLOB_PTR_LEN((char *) repo->csum.data,
						    APK_CACHE_CSUM_BYTES));
	if (ver == 0)
		apk_blob_push_blob(&to, APK_BLOB_STR(".tar.gz"));
	else
		apk_blob_push_blob(&to, APK_BLOB_STR(".gz"));
	apk_blob_push_blob(&to, APK_BLOB_PTR_LEN("", 1));
}

int apk_cache_download(struct apk_database *db, const char *url,
		       const char *item, const char *cacheitem, int verify)
{
	char fullurl[PATH_MAX];
	int r;

	snprintf(fullurl, sizeof(fullurl), "%s%s%s",
		 url, url[strlen(url)-1] == '/' ? "" : "/", item);
	apk_message("fetch %s", fullurl);

	if (apk_flags & APK_SIMULATE)
		return 0;

	r = apk_url_download(fullurl, db->cachetmp_fd, cacheitem);
	if (r < 0)
		return r;

	if (verify != APK_SIGN_NONE) {
		struct apk_istream *is;
		struct apk_sign_ctx sctx;

		apk_sign_ctx_init(&sctx, APK_SIGN_VERIFY, NULL, db->keys_fd);
		is = apk_bstream_gunzip_mpart(
			apk_bstream_from_file(db->cachetmp_fd, cacheitem),
			apk_sign_ctx_mpart_cb, &sctx);

		r = apk_tar_parse(is, apk_sign_ctx_verify_tar, &sctx, FALSE);
		is->close(is);
		apk_sign_ctx_free(&sctx);
		if (r != 0) {
			unlinkat(db->cachetmp_fd, cacheitem, 0);
			return r;
		}
	}

	if (renameat(db->cachetmp_fd, cacheitem, db->cache_fd, cacheitem) < 0)
		return -errno;

	return 0;
}

static struct apk_db_dir_instance *find_diri(struct apk_installed_package *ipkg,
					     apk_blob_t dirname,
					     struct apk_db_dir_instance *curdiri,
					     struct hlist_node ***tail)
{
	struct hlist_node *n;
	struct apk_db_dir_instance *diri;

	if (curdiri != NULL &&
	    apk_blob_compare(APK_BLOB_PTR_LEN(curdiri->dir->name,
					      curdiri->dir->namelen),
			     dirname) == 0)
		return curdiri;

	hlist_for_each_entry(diri, n, &ipkg->owned_dirs, pkg_dirs_list) {
		if (apk_blob_compare(APK_BLOB_PTR_LEN(diri->dir->name,
						      diri->dir->namelen), dirname) == 0) {
			if (tail != NULL)
				*tail = hlist_tail_ptr(&diri->owned_files);
			return diri;
		}
	}
	return NULL;
}

int apk_db_read_overlay(struct apk_database *db, struct apk_bstream *bs)
{
	struct apk_db_dir_instance *diri = NULL;
	struct hlist_node **diri_node = NULL, **file_diri_node = NULL;
	struct apk_package *pkg;
	struct apk_installed_package *ipkg;
	struct apk_db_file *file;
	apk_blob_t token = APK_BLOB_STR("\n"), line, bdir, bfile;

	pkg = apk_pkg_new();
	if (pkg == NULL)
		return -1;

	ipkg = apk_pkg_install(db, pkg);
	if (ipkg == NULL)
		return -1;

	diri_node = hlist_tail_ptr(&ipkg->owned_dirs);

	while (!APK_BLOB_IS_NULL(line = bs->read(bs, token))) {
		if (!apk_blob_rsplit(line, '/', &bdir, &bfile))
			break;

		if (bfile.len == 0) {
			diri = apk_db_diri_new(db, pkg, bdir, &diri_node);
			file_diri_node = &diri->owned_files.first;
		} else {
			diri = find_diri(ipkg, bdir, diri, &file_diri_node);
			if (diri == NULL) {
				diri = apk_db_diri_new(db, pkg, bdir, &diri_node);
				file_diri_node = &diri->owned_files.first;
			}
			file = apk_db_file_get(db, diri, bfile, &file_diri_node);
		}
	}

	return 0;
}

int apk_db_index_read(struct apk_database *db, struct apk_bstream *bs, int repo)
{
	struct apk_package *pkg = NULL;
	struct apk_installed_package *ipkg = NULL;
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

			if (repo >= 0)
				pkg->repos |= BIT(repo);

			if (apk_db_pkg_add(db, pkg) == NULL) {
				apk_error("Installed database load failed");
				return -1;
			}
			pkg = NULL;
			ipkg = NULL;
			continue;
		}

		/* Get field */
		field = l.ptr[0];
		l.ptr += 2;
		l.len -= 2;

		/* If no package, create new */
		if (pkg == NULL) {
			pkg = apk_pkg_new();
                        ipkg = NULL;
			diri = NULL;
			file_diri_node = NULL;
		}

		/* Standard index line? */
		if (apk_pkg_add_info(db, pkg, field, l) == 0) {
		        if (repo == -1 && field == 'S') {
                                /* Instert to installed database; this needs to
                                 * happen after package name has been read, but
                                 * before first FDB entry. */
                                ipkg = apk_pkg_install(db, pkg);
                                diri_node = hlist_tail_ptr(&ipkg->owned_dirs);
		        }
			continue;
                }

		if (repo != -1 || ipkg == NULL) {
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
	struct apk_installed_package *ipkg;
	struct apk_package *pkg;
	struct apk_db_dir_instance *diri;
	struct apk_db_file *file;
	struct hlist_node *c1, *c2;
	char buf[1024];
	apk_blob_t bbuf = APK_BLOB_BUF(buf);
	int r;

	list_for_each_entry(ipkg, &db->installed.packages, installed_pkgs_list) {
		pkg = ipkg->pkg;
		r = apk_pkg_write_index_entry(pkg, os);
		if (r < 0)
			return r;

		hlist_for_each_entry(diri, c1, &ipkg->owned_dirs, pkg_dirs_list) {
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
	struct apk_installed_package *ipkg;
	struct apk_package *pkg;
	struct apk_file_info fi;
	char filename[256];
	apk_blob_t bfn;
	int r, i;
	time_t now = time(NULL);

	list_for_each_entry(ipkg, &db->installed.packages, installed_pkgs_list) {
		pkg = ipkg->pkg;

		for (i = 0; i < APK_SCRIPT_MAX; i++) {
			if (ipkg->script[i].ptr == NULL)
				continue;

			fi = (struct apk_file_info) {
				.name = filename,
				.size = ipkg->script[i].len,
				.mode = 0755 | S_IFREG,
				.mtime = now,
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
			apk_blob_push_blob(&bfn, APK_BLOB_STR(apk_script_types[i]));
			apk_blob_push_blob(&bfn, APK_BLOB_PTR_LEN("", 1));

			r = apk_tar_write_entry(os, &fi, ipkg->script[i].ptr);
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
		if (pkg != NULL && pkg->ipkg != NULL)
			apk_ipkg_add_script(pkg->ipkg, is, hdr.type, hdr.size);
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
	if (pkg != NULL && pkg->ipkg != NULL)
		apk_ipkg_add_script(pkg->ipkg, is, type, ae->size);

	return 0;
}

static int parse_triggers(void *ctx, apk_blob_t blob)
{
	struct apk_installed_package *ipkg = ctx;

	if (blob.len == 0)
		return 0;

	*apk_string_array_add(&ipkg->triggers) = apk_blob_cstr(blob);
	return 0;
}

static void apk_db_triggers_write(struct apk_database *db, struct apk_ostream *os)
{
	struct apk_installed_package *ipkg;
	char buf[APK_BLOB_CHECKSUM_BUF];
	apk_blob_t bfn;
	int i;

	list_for_each_entry(ipkg, &db->installed.triggers, trigger_pkgs_list) {
		bfn = APK_BLOB_BUF(buf);
		apk_blob_push_csum(&bfn, &ipkg->pkg->csum);
		bfn = apk_blob_pushed(APK_BLOB_BUF(buf), bfn);
		os->write(os, bfn.ptr, bfn.len);

		for (i = 0; i < ipkg->triggers->num; i++) {
			os->write(os, " ", 1);
			apk_ostream_write_string(os, ipkg->triggers->item[i]);
		}
		os->write(os, "\n", 1);
	}
}

static void apk_db_triggers_read(struct apk_database *db, struct apk_bstream *bs)
{
	struct apk_checksum csum;
	struct apk_package *pkg;
	struct apk_installed_package *ipkg;
	apk_blob_t l;

	while (!APK_BLOB_IS_NULL(l = bs->read(bs, APK_BLOB_STR("\n")))) {
		apk_blob_pull_csum(&l, &csum);
		apk_blob_pull_char(&l, ' ');

		pkg = apk_db_get_pkg(db, &csum);
		if (pkg == NULL || pkg->ipkg == NULL)
			continue;

		ipkg = pkg->ipkg;
		apk_blob_for_each_segment(l, " ", parse_triggers, ipkg);
		if (ipkg->triggers && !list_hashed(&ipkg->trigger_pkgs_list))
			list_add_tail(&ipkg->trigger_pkgs_list,
				      &db->installed.triggers);
	}
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
	if (!(flags & APK_OPENF_NO_WORLD)) {
		blob = apk_blob_from_file(db->root_fd, "var/lib/apk/world");
		if (APK_BLOB_IS_NULL(blob))
			return -ENOENT;
		apk_deps_parse(db, &db->world, blob);
		free(blob.ptr);

		for (i = 0; db->world != NULL && i < db->world->num; i++)
			db->world->item[i].name->flags |= APK_NAME_TOPLEVEL;
	}

	if (!(flags & APK_OPENF_NO_INSTALLED)) {
		bs = apk_bstream_from_file(db->root_fd, "var/lib/apk/installed");
		if (bs != NULL) {
			apk_db_index_read(db, bs, -1);
			bs->close(bs, NULL);
		}

		bs = apk_bstream_from_file(db->root_fd, "var/lib/apk/triggers");
		if (bs != NULL) {
			apk_db_triggers_read(db, bs);
			bs->close(bs, NULL);
		}
	}

	if (!(flags & APK_OPENF_NO_SCRIPTS)) {
		is = apk_istream_from_file(db->root_fd, "var/lib/apk/scripts.tar");
		if (is != NULL) {
			apk_tar_parse(is, apk_read_script_archive_entry, db,
				      FALSE);
		} else {
			is = apk_istream_from_file(db->root_fd, "var/lib/apk/scripts");
			if (is != NULL)
				apk_db_scriptdb_read_v1(db, is);
		}
		if (is != NULL)
			is->close(is);
	}

	return 0;
}

struct index_write_ctx {
	struct apk_ostream *os;
	int count;
	int force;
};

static int write_index_entry(apk_hash_item item, void *ctx)
{
	struct index_write_ctx *iwctx = (struct index_write_ctx *) ctx;
	struct apk_package *pkg = (struct apk_package *) item;
	int r;

	if (!iwctx->force && pkg->filename == NULL)
		return 0;

	r = apk_pkg_write_index_entry(pkg, iwctx->os);
	if (r < 0)
		return r;

	if (iwctx->os->write(iwctx->os, "\n", 1) != 1)
		return -1;

	iwctx->count++;
	return 0;
}

static int apk_db_index_write_nr_cache(struct apk_database *db)
{
	struct index_write_ctx ctx = { NULL, 0, TRUE };
	struct apk_installed_package *ipkg;
	struct apk_ostream *os;
	int r;

	if (!apk_db_cache_active(db))
		return 0;

	/* Write list of installed non-repository packages to
	 * cached index file */
	os = apk_ostream_to_file(db->cache_fd,
				 "installed",
				 "installed.new",
				 0644);
	if (os == NULL)
		return -1;

	ctx.os = os;
	list_for_each_entry(ipkg, &db->installed.packages, installed_pkgs_list) {
		if (ipkg->pkg->repos != 0)
			continue;
		r = write_index_entry(ipkg->pkg, &ctx);
		if (r != 0)
			return r;
	}
	r = os->close(os);
	if (r < 0)
		return r;

	return ctx.count;
}

int apk_db_index_write(struct apk_database *db, struct apk_ostream *os)
{
	struct index_write_ctx ctx = { os, 0, FALSE };

	apk_hash_foreach(&db->available.packages, write_index_entry, &ctx);

	return ctx.count;
}

static int add_protected_path(void *ctx, apk_blob_t blob)
{
	struct apk_database *db = (struct apk_database *) ctx;

	*apk_string_array_add(&db->protected_paths) = apk_blob_cstr(blob);
	return 0;
}

static int apk_db_create(struct apk_database *db)
{
	int fd;

	mkdirat(db->root_fd, "tmp", 01777);
	mkdirat(db->root_fd, "dev", 0755);
	mknodat(db->root_fd, "dev/null", 0666, makedev(1, 3));
	mkdirat(db->root_fd, "var", 0755);
	mkdirat(db->root_fd, "var/lib", 0755);
	mkdirat(db->root_fd, "var/lib/apk", 0755);
	mkdirat(db->root_fd, "var/cache", 0755);
	mkdirat(db->root_fd, "var/cache/misc", 0755);

	fd = openat(db->root_fd, "var/lib/apk/world", O_CREAT|O_RDWR|O_TRUNC, 0644);
	if (fd < 0)
		return -errno;
	close(fd);

	return 0;
}

static void handle_alarm(int sig)
{
}

int apk_db_open(struct apk_database *db, struct apk_db_options *dbopts)
{
	const char *msg = NULL;
	struct apk_repository_list *repo = NULL;
	struct apk_bstream *bs;
	struct stat64 st;
	apk_blob_t blob;
	int r, rr = 0;

	memset(db, 0, sizeof(*db));
	if (dbopts->open_flags == 0) {
		msg = "Invalid open flags (internal error)";
		r = -1;
		goto ret_r;
	}

	apk_hash_init(&db->available.names, &pkg_name_hash_ops, 1000);
	apk_hash_init(&db->available.packages, &pkg_info_hash_ops, 4000);
	apk_hash_init(&db->installed.dirs, &dir_hash_ops, 2000);
	apk_hash_init(&db->installed.files, &file_hash_ops, 10000);
	list_init(&db->installed.packages);
	list_init(&db->installed.triggers);
	db->cache_dir = apk_static_cache_dir;
	db->permanent = 1;

	db->root = strdup(dbopts->root ?: "/");
	db->root_fd = openat(AT_FDCWD, db->root, O_RDONLY);
	if (db->root_fd < 0 && (dbopts->open_flags & APK_OPENF_CREATE)) {
		mkdirat(AT_FDCWD, db->root, 0755);
		db->root_fd = openat(AT_FDCWD, db->root, O_RDONLY);
	}
	if (db->root_fd < 0) {
		msg = "Unable to open root";
		goto ret_errno;
	}
	if (fstat64(db->root_fd, &st) != 0 || major(st.st_dev) == 0)
		db->permanent = 0;

	if (fstatat64(db->root_fd, apk_linked_cache_dir, &st, 0) == 0 &&
	    S_ISDIR(st.st_mode) && major(st.st_dev) != 0)
		db->cache_dir = apk_linked_cache_dir;

	if (dbopts->open_flags & APK_OPENF_WRITE) {
		db->lock_fd = openat(db->root_fd, "var/lib/apk/lock",
				     O_CREAT | O_RDWR, 0400);
		if (db->lock_fd < 0 && errno == ENOENT &&
		    (dbopts->open_flags & APK_OPENF_CREATE)) {
			r = apk_db_create(db);
			if (r != 0) {
				msg = "Unable to create database";
				goto ret_r;
			}
			db->lock_fd = openat(db->root_fd, "var/lib/apk/lock",
					     O_CREAT | O_RDWR, 0400);
		}
		if (db->lock_fd < 0 ||
		    flock(db->lock_fd, LOCK_EX | LOCK_NB) < 0) {
			msg = "Unable to lock database";
			if (apk_wait) {
				struct sigaction sa, old_sa;

				apk_message("Waiting for repository lock");
				memset(&sa, 0, sizeof sa);
				sa.sa_handler = handle_alarm;
				sa.sa_flags   = SA_ONESHOT;
				sigaction(SIGALRM, &sa, &old_sa);

				alarm(apk_wait);
				if (flock(db->lock_fd, LOCK_EX) < 0)
					goto ret_errno;

				alarm(0);
				sigaction(SIGALRM, &old_sa, NULL);
			} else
				goto ret_errno;
		}
	}

	blob = APK_BLOB_STR("etc:*etc/init.d");
	apk_blob_for_each_segment(blob, ":", add_protected_path, db);

	db->cache_fd = openat(db->root_fd, db->cache_dir, O_RDONLY);
	mkdirat(db->cache_fd, "tmp", 0644);
	db->cachetmp_fd = openat(db->cache_fd, "tmp", O_RDONLY);
	db->keys_fd = openat(db->root_fd,
			     dbopts->keys_dir ?: "etc/apk/keys",
			     O_RDONLY);

	if (apk_flags & APK_OVERLAY_FROM_STDIN) {
		apk_flags &= ~APK_OVERLAY_FROM_STDIN;
		apk_db_read_overlay(db, apk_bstream_from_istream(
				apk_istream_from_fd(STDIN_FILENO)));
	}

	r = apk_db_read_state(db, dbopts->open_flags);
	if (r == -ENOENT && (dbopts->open_flags & APK_OPENF_CREATE)) {
		r = apk_db_create(db);
		if (r != 0) {
			msg = "Unable to create database";
			goto ret_r;
		}
		r = apk_db_read_state(db, dbopts->open_flags);
	}
	if (r != 0) {
		msg = "Unable to read database state";
		goto ret_r;
	}

	if (!(dbopts->open_flags & APK_OPENF_NO_INSTALLED_REPO)) {
		if (apk_db_cache_active(db)) {
			bs = apk_bstream_from_file(db->cache_fd, "installed");
			if (bs != NULL) {
				apk_db_index_read(db, bs, -2);
				bs->close(bs, NULL);
			}
		}
	}
	if (!(dbopts->open_flags & APK_OPENF_NO_SYS_REPOS)) {
		list_for_each_entry(repo, &dbopts->repository_list, list) {
			r = apk_db_add_repository(db, APK_BLOB_STR(repo->url));
			rr = r ?: rr;
		}
		blob = apk_blob_from_file(
			db->root_fd,
			dbopts->repositories_file ?: "etc/apk/repositories");
		if (!APK_BLOB_IS_NULL(blob)) {
			r = apk_blob_for_each_segment(
				blob, "\n",
				apk_db_add_repository, db);
			rr = r ?: rr;
			free(blob.ptr);
		}
		if (apk_flags & APK_UPDATE_CACHE)
			apk_db_index_write_nr_cache(db);
	}
	if (rr != 0) {
		r = rr;
		goto ret_r;
	}

	return rr;

ret_errno:
	r = -errno;
ret_r:
	if (msg != NULL)
		apk_error("%s: %s", msg, strerror(-r));
	apk_db_close(db);

	return r;
}

struct write_ctx {
	struct apk_database *db;
	int fd;
};

int apk_db_write_config(struct apk_database *db)
{
	struct apk_ostream *os;
	int r;

	if ((apk_flags & APK_SIMULATE) || db->root == NULL)
		return 0;

	if (db->lock_fd == 0) {
		apk_error("Refusing to write db without write lock!");
		return -1;
	}

	os = apk_ostream_to_file(db->root_fd,
				 "var/lib/apk/world",
				 "var/lib/apk/world.new",
				 0644);
	if (os == NULL)
		return -1;

	apk_deps_write(db->world, os);
	os->write(os, "\n", 1);
	r = os->close(os);
	if (r < 0)
		return r;

	os = apk_ostream_to_file(db->root_fd,
				 "var/lib/apk/installed",
				 "var/lib/apk/installed.new",
				 0644);
	if (os == NULL)
		return -1;
	apk_db_write_fdb(db, os);
	r = os->close(os);
	if (r < 0)
		return r;

	os = apk_ostream_to_file(db->root_fd,
				 "var/lib/apk/scripts.tar",
				 "var/lib/apk/scripts.tar.new",
				 0644);
	if (os == NULL)
		return -1;
	apk_db_scriptdb_write(db, os);
	r = os->close(os);
	if (r < 0)
		return r;

	unlinkat(db->root_fd, "var/lib/apk/scripts", 0);
	apk_db_index_write_nr_cache(db);

	os = apk_ostream_to_file(db->root_fd,
				 "var/lib/apk/triggers",
				 "var/lib/apk/triggers.new",
				 0644);
	if (os == NULL)
		return -1;
	apk_db_triggers_write(db, os);
	r = os->close(os);
	if (r < 0)
		return r;

	return 0;
}

void apk_db_close(struct apk_database *db)
{
	struct apk_installed_package *ipkg;
	struct apk_db_dir_instance *diri;
	struct hlist_node *dc, *dn;
	int i;

	list_for_each_entry(ipkg, &db->installed.packages, installed_pkgs_list) {
		hlist_for_each_entry_safe(diri, dc, dn, &ipkg->owned_dirs, pkg_dirs_list) {
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

	apk_hash_free(&db->available.packages);
	apk_hash_free(&db->available.names);
	apk_hash_free(&db->installed.files);
	apk_hash_free(&db->installed.dirs);

	if (db->keys_fd)
		close(db->keys_fd);
	if (db->cachetmp_fd)
		close(db->cachetmp_fd);
	if (db->cache_fd)
		close(db->cache_fd);
	if (db->root_fd)
		close(db->root_fd);
	if (db->lock_fd)
		close(db->lock_fd);
	if (db->root != NULL)
		free(db->root);
}

static int fire_triggers(apk_hash_item item, void *ctx)
{
	struct apk_database *db = (struct apk_database *) ctx;
	struct apk_db_dir *dbd = (struct apk_db_dir *) item;
	struct apk_installed_package *ipkg;
	int i;

	list_for_each_entry(ipkg, &db->installed.triggers, trigger_pkgs_list) {
		if (((ipkg->flags & APK_IPKGF_RUN_ALL_TRIGGERS) == 0) &&
		    ((dbd->flags & APK_DBDIRF_MODIFIED) == 0))
			continue;

		for (i = 0; i < ipkg->triggers->num; i++) {
			if (ipkg->triggers->item[i][0] != '/')
				continue;

			if (fnmatch(ipkg->triggers->item[i], dbd->rooted_name,
				    FNM_PATHNAME) != 0)
				continue;

			if (ipkg->pending_triggers == NULL)
				*apk_string_array_add(&ipkg->pending_triggers) =
					NULL;

			*apk_string_array_add(&ipkg->pending_triggers) =
				dbd->rooted_name;
			break;
		}
	}

	return 0;
}

int apk_db_run_triggers(struct apk_database *db)
{
	struct apk_installed_package *ipkg;

	apk_hash_foreach(&db->installed.dirs, fire_triggers, db);

	list_for_each_entry(ipkg, &db->installed.triggers, trigger_pkgs_list) {
		if (ipkg->pending_triggers == NULL)
			continue;

		*apk_string_array_add(&ipkg->pending_triggers) = NULL;
		apk_ipkg_run_script(ipkg, db->root_fd, APK_SCRIPT_TRIGGER,
				    ipkg->pending_triggers->item);
		free(ipkg->pending_triggers);
		ipkg->pending_triggers = NULL;
	}

	return 0;
}

int apk_db_cache_active(struct apk_database *db)
{
	return db->cache_dir != apk_static_cache_dir;
}

int apk_db_permanent(struct apk_database *db)
{
	return db->permanent;
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

static int apk_repo_is_remote(struct apk_repository *repo)
{
	return repo->csum.type != APK_CHECKSUM_NONE;
}

static struct apk_bstream *apk_repo_file_open(struct apk_repository *repo,
					      const char *file)
{
	char tmp[256];
	const char *url = repo->url;

	if ((apk_flags & APK_NO_NETWORK) && apk_repo_is_remote(repo))
		return NULL;

	/* We should not get called for non-repository files */
	if (strcmp(repo->url, "cache") == 0)
		return NULL;

	snprintf(tmp, sizeof(tmp), "%s%s%s",
		 url, url[strlen(url)-1] == '/' ? "" : "/", file);

	return apk_bstream_from_url(tmp);
}

struct apk_repository *apk_db_select_repo(struct apk_database *db,
					  struct apk_package *pkg)
{
	static struct apk_repository cache_repo = {
		.url = "cache",
		.csum.data = {
			0xb0,0x35,0x92,0x80,0x6e,0xfa,0xbf,0xee,0xb7,0x09,
			0xf5,0xa7,0x0a,0x7c,0x17,0x26,0x69,0xb0,0x05,0x38 },
		.csum.type = APK_CHECKSUM_SHA1,
	};
	unsigned int repos = pkg->repos;
	int i;

	/* Always prefer local repositories */
	if ((repos & db->local_repos) != 0)
		repos &= db->local_repos;

	/* Pick first repository providing this package */
	for (i = 0; i < APK_MAX_REPOS; i++)
		if (repos & BIT(i))
			break;

	/* If this is a remote repository, and we have no network,
	 * check that we have it in cache */
	if ((i >= APK_MAX_REPOS) ||
	    ((db->local_repos & BIT(i)) == 0 && (apk_flags & APK_NO_NETWORK))) {
		char cacheitem[PATH_MAX];

		apk_pkg_format_cache(pkg, APK_BLOB_BUF(cacheitem));
		if (faccessat(db->cache_fd, cacheitem, R_OK, 0) != 0)
			return NULL;
	}

	if (i >= APK_MAX_REPOS)
		return &cache_repo;

	return &db->repos[i];
}

int apk_repository_update(struct apk_database *db, struct apk_repository *repo)
{
	char cacheitem[PATH_MAX];
	int r;

	if (!apk_repo_is_remote(repo))
		return 0;

	apk_cache_format_index(APK_BLOB_BUF(cacheitem), repo, 0);
	r = apk_cache_download(db, repo->url, apkindex_tar_gz, cacheitem,
			       (apk_flags & APK_ALLOW_UNTRUSTED) ?
			       APK_SIGN_NONE : APK_SIGN_VERIFY);
	if (r == 0 || r == -ENOKEY || r == -EKEYREJECTED) {
		if (r != 0)
			apk_error("%s: %s", repo->url, apk_error_str(r));
		return r;
	}

	apk_cache_format_index(APK_BLOB_BUF(cacheitem), repo, 1);
	r = apk_cache_download(db, repo->url, apk_index_gz, cacheitem,
			       APK_SIGN_NONE);
	if (r != 0)
		apk_error("Failed to update %s: download failed", repo->url);
	return r;
}

struct apkindex_ctx {
	struct apk_database *db;
	struct apk_sign_ctx sctx;
	int repo, found;
};

static int load_apkindex(void *sctx, const struct apk_file_info *fi,
			 struct apk_istream *is)
{
	struct apkindex_ctx *ctx = (struct apkindex_ctx *) sctx;
	struct apk_bstream *bs;
	struct apk_repository *repo;

	if (apk_sign_ctx_process_file(&ctx->sctx, fi, is) == 0)
		return 0;

	repo = &ctx->db->repos[ctx->repo];

	if (strcmp(fi->name, "DESCRIPTION") == 0) {
		repo->description = apk_blob_from_istream(is, fi->size);
	} else if (strcmp(fi->name, "APKINDEX") == 0) {
		ctx->found = 1;
		bs = apk_bstream_from_istream(is);
		apk_db_index_read(ctx->db, bs, ctx->repo);
		bs->close(bs, NULL);
	}

	return 0;
}

static int load_index(struct apk_database *db, struct apk_bstream *bs,
		      int targz, int repo)
{
	int r = 0;

	if (targz) {
		struct apk_istream *is;
		struct apkindex_ctx ctx;

		ctx.db = db;
		ctx.repo = repo;
		ctx.found = 0;
		apk_sign_ctx_init(&ctx.sctx, APK_SIGN_VERIFY, NULL, db->keys_fd);
		is = apk_bstream_gunzip_mpart(bs, apk_sign_ctx_mpart_cb, &ctx.sctx);
		r = apk_tar_parse(is, load_apkindex, &ctx, FALSE);
		is->close(is);
		apk_sign_ctx_free(&ctx.sctx);
		if (ctx.found == 0)
			r = -ENOMSG;
	} else {
		bs = apk_bstream_from_istream(apk_bstream_gunzip(bs));
		apk_db_index_read(db, bs, repo);
		bs->close(bs, NULL);
	}
	return r;
}

int apk_db_index_read_file(struct apk_database *db, const char *file, int repo)
{
	int targz = 1;

	if (strstr(file, ".tar.gz") == NULL && strstr(file, ".gz") != NULL)
		targz = 0;

	return load_index(db, apk_bstream_from_file(AT_FDCWD, file), targz, repo);
}

int apk_db_add_repository(apk_database_t _db, apk_blob_t repository)
{
	struct apk_database *db = _db.db;
	struct apk_bstream *bs = NULL;
	struct apk_repository *repo;
	int r, targz = 1;

	if (repository.ptr == NULL || repository.len == 0 ||
	    *repository.ptr == '#')
		return 0;

	if (db->num_repos >= APK_MAX_REPOS)
		return -1;

	r = db->num_repos++;

	repo = &db->repos[r];
	*repo = (struct apk_repository) {
		.url = apk_blob_cstr(repository),
	};

	if (apk_url_local_file(repo->url) == NULL) {
		char cacheitem[PATH_MAX];

		apk_blob_checksum(repository, apk_checksum_default(), &repo->csum);

		if (apk_flags & APK_UPDATE_CACHE)
			apk_repository_update(db, repo);

		apk_cache_format_index(APK_BLOB_BUF(cacheitem), repo, 0);
		bs = apk_bstream_from_file(db->cache_fd, cacheitem);
		if (bs == NULL) {
			apk_cache_format_index(APK_BLOB_BUF(cacheitem), repo, 1);
			bs = apk_bstream_from_file(db->cache_fd, cacheitem);
			targz = 0;
		}
	} else {
		db->local_repos |= BIT(r);

		bs = apk_repo_file_open(repo, apkindex_tar_gz);
		if (bs == NULL) {
			bs = apk_repo_file_open(repo, apk_index_gz);
			targz = 0;
		}
	}
	if (bs == NULL) {
		apk_warning("Failed to open index for %s", repo->url);
		return 0;
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
	r = apk_ipkg_run_script(ctx->ipkg, ctx->db->root_fd, ctx->script,
				ctx->script_args);
	if (r != 0)
		apk_error("%s-%s: Failed to execute "
			  "pre-install/upgrade script",
			  ctx->pkg->name->name, ctx->pkg->version);
	return r;
}

static int parse_replaces(void *_ctx, apk_blob_t blob)
{
	struct install_ctx *ctx = (struct install_ctx *) _ctx;

	if (blob.len == 0)
		return 0;

	*apk_name_array_add(&ctx->replaces) = apk_db_get_name(ctx->db, blob);
	return 0;
}

static int read_info_line(void *_ctx, apk_blob_t line)
{
	struct install_ctx *ctx = (struct install_ctx *) _ctx;
	struct apk_installed_package *ipkg = ctx->ipkg;
	struct apk_database *db = ctx->db;
	apk_blob_t l, r;

	if (line.ptr == NULL || line.len < 1 || line.ptr[0] == '#')
		return 0;

	if (!apk_blob_split(line, APK_BLOB_STR(" = "), &l, &r))
		return 0;

	if (apk_blob_compare(APK_BLOB_STR("replaces"), l) == 0) {
		apk_blob_for_each_segment(r, " ", parse_replaces, ctx);
	} else if (apk_blob_compare(APK_BLOB_STR("triggers"), l) == 0) {
		if (ipkg->triggers) {
			free(ipkg->triggers);
			ipkg->triggers = NULL;
		}
		apk_blob_for_each_segment(r, " ", parse_triggers, ctx->ipkg);

		if (ctx->ipkg->triggers && !list_hashed(&ipkg->trigger_pkgs_list))
			list_add_tail(&ipkg->trigger_pkgs_list,
				      &db->installed.triggers);
	} else {
		apk_sign_ctx_parse_pkginfo_line(&ctx->sctx, line);
	}
	return 0;
}

static int apk_db_install_archive_entry(void *_ctx,
					const struct apk_file_info *ae,
					struct apk_istream *is)
{
	struct install_ctx *ctx = (struct install_ctx *) _ctx;
	struct apk_database *db = ctx->db;
	struct apk_package *pkg = ctx->pkg, *opkg;
	struct apk_installed_package *ipkg = pkg->ipkg;
	apk_blob_t name = APK_BLOB_STR(ae->name), bdir, bfile;
	struct apk_db_dir_instance *diri = ctx->diri;
	struct apk_db_file *file;
	const char *p;
	int r = 0, type = APK_SCRIPT_INVALID;

	if (apk_sign_ctx_process_file(&ctx->sctx, ae, is) == 0)
		return 0;

	/* Package metainfo and script processing */
	if (ae->name[0] == '.') {
		/* APK 2.0 format */
		if (strcmp(ae->name, ".PKGINFO") == 0) {
			apk_blob_t blob = apk_blob_from_istream(is, ae->size);
			apk_blob_for_each_segment(blob, "\n", read_info_line, ctx);
			free(blob.ptr);
			return 0;
		}
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
		apk_ipkg_add_script(ipkg, is, type, ae->size);
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
		diri = find_diri(ipkg, bdir, diri, &ctx->file_diri_node);
		if (diri == NULL) {
			apk_error("%s: File '%*s' entry without directory entry.\n",
				  pkg->name->name, name.len, name.ptr);
			return -1;
		}

		opkg = NULL;
		file = apk_db_file_query(db, bdir, bfile);
		if (file != NULL) {
			int i;

			opkg = file->diri->pkg;
			do {
				/* Overlay file? */
				if (opkg->name == NULL)
					break;
				/* Upgrading package? */
				if (opkg->name == pkg->name)
					break;
				/* Overwriting with permission? */
				for (i = 0; ctx->replaces && i < ctx->replaces->num; i++)
					if (opkg->name == ctx->replaces->item[i])
						break;
				if (ctx->replaces && i < ctx->replaces->num)
					break;

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
			} while (0);
		}

		if (opkg != pkg) {
			/* Create the file entry without adding it to hash */
			file = apk_db_file_new(diri, bfile, &ctx->file_diri_node);
		}

		if (apk_verbosity >= 3)
			apk_message("%s", ae->name);

		/* Extract the file as name.apk-new */
		r = apk_archive_entry_extract(db->root_fd, ae, ".apk-new", is,
					      extract_cb, ctx);

		/* Hardlinks need special care for checksum */
		if (ae->csum.type == APK_CHECKSUM_NONE &&
		    ae->link_target != NULL) {
			do {
				struct apk_db_file *lfile;
				struct apk_db_dir_instance *ldiri;
				struct hlist_node *n;

				if (!apk_blob_rsplit(APK_BLOB_STR(ae->link_target),
						     '/', &bdir, &bfile))
					break;

				ldiri = find_diri(ipkg, bdir, diri, NULL);
				if (ldiri == NULL)
					break;

				hlist_for_each_entry(lfile, n, &ldiri->owned_files,
						     diri_files_list) {
					if (apk_blob_compare(APK_BLOB_PTR_LEN(lfile->name, lfile->namelen),
							     bfile) == 0) {
						memcpy(&file->csum, &lfile->csum,
						       sizeof(file->csum));
						break;
					}
				}
			} while (0);

		} else
			memcpy(&file->csum, &ae->csum, sizeof(file->csum));
	} else {
		if (apk_verbosity >= 3)
			apk_message("%s", ae->name);

		if (name.ptr[name.len-1] == '/')
			name.len--;

		if (ctx->diri_node == NULL)
			ctx->diri_node = hlist_tail_ptr(&ipkg->owned_dirs);
		ctx->diri = diri = apk_db_diri_new(db, pkg, name,
						   &ctx->diri_node);
		ctx->file_diri_node = hlist_tail_ptr(&diri->owned_files);

		apk_db_diri_set(diri, ae->mode & 0777, ae->uid, ae->gid);
		apk_db_diri_mkdir(db, diri);
	}
	ctx->installed_size += ctx->current_file_size;

	return r;
}

static void apk_db_purge_pkg(struct apk_database *db,
			     struct apk_installed_package *ipkg,
			     const char *exten)
{
	struct apk_db_dir_instance *diri;
	struct apk_db_file *file;
	struct apk_db_file_hash_key key;
	struct apk_file_info fi;
	struct hlist_node *dc, *dn, *fc, *fn;
	unsigned long hash;
	char name[PATH_MAX];

	hlist_for_each_entry_safe(diri, dc, dn, &ipkg->owned_dirs, pkg_dirs_list) {
		if (exten == NULL)
			diri->dir->flags |= APK_DBDIRF_MODIFIED;

		hlist_for_each_entry_safe(file, fc, fn, &diri->owned_files, diri_files_list) {
			snprintf(name, sizeof(name), "%s/%s%s",
				 diri->dir->name, file->name, exten ?: "");

			key = (struct apk_db_file_hash_key) {
				.dirname = APK_BLOB_PTR_LEN(diri->dir->name, diri->dir->namelen),
				.filename = APK_BLOB_PTR_LEN(file->name, file->namelen),
			};
			hash = apk_blob_hash_seed(key.filename, diri->dir->hash);
			if (!(diri->dir->flags & APK_DBDIRF_PROTECTED) ||
			    (apk_flags & APK_PURGE) ||
			    (file->csum.type != APK_CHECKSUM_NONE &&
			     apk_file_get_info(db->root_fd, name, APK_FI_NOFOLLOW | file->csum.type, &fi) == 0 &&
			     apk_checksum_compare(&file->csum, &fi.csum) == 0))
				unlinkat(db->root_fd, name, 0);
			if (apk_verbosity >= 3)
				apk_message("%s", name);
			__hlist_del(fc, &diri->owned_files.first);
			if (exten == NULL) {
				apk_hash_delete_hashed(&db->installed.files, APK_BLOB_BUF(&key), hash);
				db->installed.stats.files--;
			}
		}
		apk_db_diri_rmdir(db, diri);
		__hlist_del(dc, &ipkg->owned_dirs.first);
		apk_db_diri_free(db, diri);
	}
}


static void apk_db_migrate_files(struct apk_database *db,
				 struct apk_installed_package *ipkg)
{
	struct apk_db_dir_instance *diri;
	struct apk_db_dir *dir;
	struct apk_db_file *file, *ofile;
	struct apk_db_file_hash_key key;
	struct apk_file_info fi;
	struct hlist_node *dc, *dn, *fc, *fn;
	unsigned long hash;
	char name[PATH_MAX], tmpname[PATH_MAX];
	int cstype, r;

	hlist_for_each_entry_safe(diri, dc, dn, &ipkg->owned_dirs, pkg_dirs_list) {
		dir = diri->dir;
		dir->flags |= APK_DBDIRF_MODIFIED;

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

			/* We want to compare checksums only if one exists
			 * in db, and the file is in a protected path */
			cstype = APK_CHECKSUM_NONE;
			if (ofile != NULL &&
			    (diri->dir->flags & APK_DBDIRF_PROTECTED))
				cstype = ofile->csum.type;
			cstype |= APK_FI_NOFOLLOW;

			r = apk_file_get_info(db->root_fd, name, cstype, &fi);
			if (ofile && ofile->diri->pkg->name == NULL) {
				/* File was from overlay, delete the
				 * packages version */
				unlinkat(db->root_fd, tmpname, 0);
			} else if ((diri->dir->flags & APK_DBDIRF_PROTECTED) &&
				   (r == 0) &&
				   (ofile == NULL ||
				    ofile->csum.type == APK_CHECKSUM_NONE ||
				    apk_checksum_compare(&ofile->csum, &fi.csum) != 0)) {
				/* Protected directory, with file without
				 * db entry, or local modifications.
				 *
				 * Delete the apk-new if it's identical with the
				 * existing file */
				if (ofile == NULL ||
				    ofile->csum.type != file->csum.type)
					apk_file_get_info(db->root_fd, name,
						APK_FI_NOFOLLOW | file->csum.type, &fi);
				if ((apk_flags & APK_CLEAN_PROTECTED) ||
				    (file->csum.type != APK_CHECKSUM_NONE &&
				     apk_checksum_compare(&file->csum, &fi.csum) == 0))
					unlinkat(db->root_fd, tmpname, 0);
			} else {
				/* Overwrite the old file */
				renameat(db->root_fd, tmpname,
					 db->root_fd, name);
			}

			/* Claim ownership of the file in db */
			if (ofile != file) {
				if (ofile != NULL) {
					hlist_del(&ofile->diri_files_list,
						&ofile->diri->owned_files);
					apk_hash_delete_hashed(&db->installed.files,
							       APK_BLOB_BUF(&key), hash);
				} else
					db->installed.stats.files++;

				apk_hash_insert_hashed(&db->installed.files, file, hash);
			}
		}
	}
}

static int apk_db_unpack_pkg(struct apk_database *db,
			     struct apk_installed_package *ipkg,
			     int upgrade, int reinstall,
			     apk_progress_cb cb, void *cb_ctx,
			     char **script_args)
{
	struct install_ctx ctx;
	struct apk_bstream *bs = NULL;
	struct apk_istream *tar;
	struct apk_package *pkg = ipkg->pkg;
	char file[PATH_MAX];
	int r, need_copy = FALSE;

	if (pkg->filename == NULL) {
		struct apk_repository *repo;

		repo = apk_db_select_repo(db, pkg);
		if (repo == NULL) {
			apk_error("%s-%s: package is not currently available",
				  pkg->name->name, pkg->version);
			return -1;
		}

		if (apk_db_cache_active(db) && apk_repo_is_remote(repo)) {
			apk_pkg_format_cache(pkg, APK_BLOB_BUF(file));
			bs = apk_bstream_from_file(db->cache_fd, file);
		}

		if (bs == NULL) {
			apk_pkg_format_plain(pkg, APK_BLOB_BUF(file));
			bs = apk_repo_file_open(repo, file);
			if (apk_repo_is_remote(repo))
				need_copy = TRUE;
		}
	} else {
		bs = apk_bstream_from_file(AT_FDCWD, pkg->filename);
		need_copy = TRUE;
	}
	if (!apk_db_cache_active(db))
		need_copy = FALSE;
	if (need_copy) {
		apk_pkg_format_cache(pkg, APK_BLOB_BUF(file));
		bs = apk_bstream_tee(bs, db->cachetmp_fd, file);
	}

	if (bs == NULL) {
		apk_error("%s: %s", file, strerror(errno));
		return errno;
	}

	ctx = (struct install_ctx) {
		.db = db,
		.pkg = pkg,
		.ipkg = ipkg,
		.script = upgrade ?
			APK_SCRIPT_PRE_UPGRADE : APK_SCRIPT_PRE_INSTALL,
		.script_args = script_args,
		.cb = cb,
		.cb_ctx = cb_ctx,
	};
	apk_sign_ctx_init(&ctx.sctx, APK_SIGN_VERIFY_IDENTITY, &pkg->csum, db->keys_fd);
	tar = apk_bstream_gunzip_mpart(bs, apk_sign_ctx_mpart_cb, &ctx.sctx);
	r = apk_tar_parse(tar, apk_db_install_archive_entry, &ctx, TRUE);
	apk_sign_ctx_free(&ctx.sctx);
	tar->close(tar);
	if (ctx.replaces)
		free(ctx.replaces);

	if (need_copy) {
		if (r == 0)
			renameat(db->cachetmp_fd, file, db->cache_fd, file);
		else
			unlinkat(db->cachetmp_fd, file, 0);
	}

	if (r != 0) {
		apk_error("%s-%s: %s",
			  pkg->name->name, pkg->version,
			  apk_error_str(r));
		goto err;
	}
	r = apk_db_run_pending_script(&ctx);
	if (r != 0)
		goto err;

	apk_db_migrate_files(db, ipkg);

	return 0;
err:
	if (!reinstall)
		apk_db_purge_pkg(db, ipkg, ".apk-new");
	return r;
}

int apk_db_install_pkg(struct apk_database *db,
		       struct apk_package *oldpkg,
		       struct apk_package *newpkg,
		       apk_progress_cb cb, void *cb_ctx)
{
	char *script_args[] = { NULL, NULL, NULL, NULL };
	struct apk_installed_package *ipkg;
	int r;

	/* Upgrade script gets two args: <new-pkg> <old-pkg> */
	if (oldpkg != NULL && newpkg != NULL) {
		script_args[1] = newpkg->version;
		script_args[2] = oldpkg->version;
	} else {
		script_args[1] = oldpkg ? oldpkg->version : newpkg->version;
	}

	/* Just purging? */
	if (oldpkg != NULL && newpkg == NULL) {
		ipkg = oldpkg->ipkg;

		if (ipkg == NULL)
			return 0;

		r = apk_ipkg_run_script(ipkg, db->root_fd,
					APK_SCRIPT_PRE_DEINSTALL, script_args);
		if (r != 0)
			return r;

		apk_db_purge_pkg(db, ipkg, NULL);
		r = apk_ipkg_run_script(ipkg, db->root_fd,
					APK_SCRIPT_POST_DEINSTALL, script_args);
		apk_pkg_uninstall(db, oldpkg);

		return r;
	}

	/* Install the new stuff */
	ipkg = apk_pkg_install(db, newpkg);
	if (newpkg->installed_size != 0) {
		r = apk_db_unpack_pkg(db, ipkg, (oldpkg != NULL),
				      (oldpkg == newpkg), cb, cb_ctx,
				      script_args);
		if (r != 0) {
			apk_pkg_uninstall(db, newpkg);
			return r;
		}
	}

	ipkg->flags |= APK_IPKGF_RUN_ALL_TRIGGERS;
	if (ipkg->triggers) {
		list_del(&ipkg->trigger_pkgs_list);
		free(ipkg->triggers);
		ipkg->triggers = NULL;
	}

	if (oldpkg != NULL && oldpkg != newpkg && oldpkg->ipkg != NULL) {
		apk_db_purge_pkg(db, oldpkg->ipkg, NULL);
		apk_pkg_uninstall(db, oldpkg);
	}

	r = apk_ipkg_run_script(ipkg, db->root_fd,
				(oldpkg == NULL) ?
				APK_SCRIPT_POST_INSTALL : APK_SCRIPT_POST_UPGRADE,
				script_args);
	if (r != 0) {
		apk_error("%s-%s: Failed to execute post-install/upgrade script",
			  newpkg->name->name, newpkg->version);
	}
	return r;
}
