/* database.c - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2005-2008 Natanael Copa <n@tanael.org>
 * Copyright (C) 2008-2011 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <mntent.h>
#include <limits.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <fnmatch.h>
#include <sys/vfs.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

#include "apk_defines.h"
#include "apk_package.h"
#include "apk_database.h"
#include "apk_applet.h"
#include "apk_archive.h"
#include "apk_print.h"

static const apk_spn_match_def apk_spn_repo_separators = {
	[4] = (1<<0) /* */,
	[7] = (1<<2) /*:*/,
};

enum {
	APK_DISALLOW_RMDIR = 0,
	APK_ALLOW_RMDIR = 1
};

int apk_verbosity = 1;
unsigned int apk_flags = 0;

static apk_blob_t tmpprefix = { .len=8, .ptr = ".apknew." };

static const char * const apkindex_tar_gz = "APKINDEX.tar.gz";

static const char * const apk_static_cache_dir = "var/cache/apk";
static const char * const apk_linked_cache_dir = "etc/apk/cache";

static const char * const apk_lock_file = "var/lock/apkdb";

static const char * const apk_world_file = "etc/apk/world";
static const char * const apk_world_file_tmp = "etc/apk/world.new";
static const char * const apk_world_file_old = "var/lib/apk/world";
static const char * const apk_arch_file = "etc/apk/arch";

static const char * const apk_scripts_file = "lib/apk/db/scripts.tar";
static const char * const apk_scripts_file_tmp = "lib/apk/db/scripts.tar.new";
static const char * const apk_scripts_file_old = "var/lib/apk/scripts.tar";

static const char * const apk_triggers_file = "lib/apk/db/triggers";
static const char * const apk_triggers_file_tmp = "lib/apk/db/triggers.new";
static const char * const apk_triggers_file_old = "var/lib/apk/triggers";

const char * const apk_installed_file = "lib/apk/db/installed";
static const char * const apk_installed_file_tmp = "lib/apk/db/installed.new";
static const char * const apk_installed_file_old = "var/lib/apk/installed";

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
	apk_provider_array_free(&name->providers);
	apk_name_array_free(&name->rdepends);
	apk_name_array_free(&name->rinstall_if);
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
	apk_provider_array_init(&pn->providers);
	apk_name_array_init(&pn->rdepends);
	apk_name_array_init(&pn->rinstall_if);
	apk_hash_insert_hashed(&db->available.names, pn, hash);

	return pn;
}

static void apk_db_dir_mkdir(struct apk_database *db, struct apk_db_dir *dir)
{
	if (apk_flags & APK_SIMULATE)
		return;

	/* Don't mess with root, as no package provides it directly */
	if (dir->namelen == 0)
		return;

	if ((dir->refs == 1) ||
	    (fchmodat(db->root_fd, dir->name, dir->mode, 0) != 0 &&
	     errno == ENOENT))
		if ((mkdirat(db->root_fd, dir->name, dir->mode) != 0 &&
		     errno == EEXIST))
			if (fchmodat(db->root_fd, dir->name, dir->mode, 0) != 0)
				;

	if (fchownat(db->root_fd, dir->name, dir->uid, dir->gid, 0) != 0)
		;
}

void apk_db_dir_unref(struct apk_database *db, struct apk_db_dir *dir, int allow_rmdir)
{
	dir->refs--;
	if (dir->refs > 0) {
		if (allow_rmdir) {
			dir->recalc_mode = 1;
			dir->mode = 0;
			dir->uid = (uid_t) -1;
			dir->gid = (gid_t) -1;
		}
		return;
	}

	db->installed.stats.dirs--;

	if (allow_rmdir) {
		/* The final instance of this directory was removed,
		 * so this directory gets deleted in reality too. */
		dir->recalc_mode = 0;
		dir->mode = 0;
		dir->uid = (uid_t) -1;
		dir->gid = (gid_t) -1;

		if (dir->namelen)
			unlinkat(db->root_fd, dir->name, AT_REMOVEDIR);
	} else if (dir->recalc_mode) {
		/* Directory permissions need a reset. */
		apk_db_dir_mkdir(db, dir);
	}

	if (dir->parent != NULL)
		apk_db_dir_unref(db, dir->parent, allow_rmdir);
}

struct apk_db_dir *apk_db_dir_ref(struct apk_db_dir *dir)
{
	dir->refs++;
	return dir;
}

struct apk_db_dir *apk_db_dir_query(struct apk_database *db,
				    apk_blob_t name)
{
	return (struct apk_db_dir *) apk_hash_get(&db->installed.dirs, name);
}

struct apk_db_dir *apk_db_dir_get(struct apk_database *db, apk_blob_t name)
{
	struct apk_db_dir *dir;
	struct apk_protected_path_array *ppaths;
	struct apk_protected_path *ppath;
	apk_blob_t bparent;
	unsigned long hash = apk_hash_from_key(&db->installed.dirs, name);
	char *relative_name;

	if (name.len && name.ptr[name.len-1] == '/')
		name.len--;

	dir = (struct apk_db_dir *) apk_hash_get_hashed(&db->installed.dirs, name, hash);
	if (dir != NULL)
		return apk_db_dir_ref(dir);

	db->installed.stats.dirs++;
	dir = malloc(sizeof(*dir) + name.len + 1);
	memset(dir, 0, sizeof(*dir));
	dir->refs = 1;
	dir->uid = (uid_t) -1;
	dir->gid = (gid_t) -1;
	dir->rooted_name[0] = '/';
	memcpy(dir->name, name.ptr, name.len);
	dir->name[name.len] = 0;
	dir->namelen = name.len;
	dir->hash = hash;
	apk_protected_path_array_init(&dir->protected_paths);
	apk_hash_insert_hashed(&db->installed.dirs, dir, hash);

	if (name.len == 0) {
		dir->parent = NULL;
		dir->has_protected_children = 1;
		ppaths = NULL;
	} else if (apk_blob_rsplit(name, '/', &bparent, NULL)) {
		dir->parent = apk_db_dir_get(db, bparent);
		dir->protected = dir->parent->protected;
		dir->has_protected_children = dir->protected;
		dir->symlinks_only = dir->parent->symlinks_only;
		ppaths = dir->parent->protected_paths;
	} else {
		dir->parent = apk_db_dir_get(db, APK_BLOB_NULL);
		ppaths = db->protected_paths;
	}

	if (ppaths == NULL)
		return dir;

	relative_name = strrchr(dir->rooted_name, '/') + 1;
	foreach_array_item(ppath, ppaths) {
		char *slash = strchr(ppath->relative_pattern, '/');
		if (slash != NULL) {
			*slash = 0;
			if (fnmatch(ppath->relative_pattern, relative_name, FNM_PATHNAME) != 0) {
				*slash = '/';
				continue;
			}
			*slash = '/';

			*apk_protected_path_array_add(&dir->protected_paths) = (struct apk_protected_path) {
				.relative_pattern = slash + 1,
				.protected = ppath->protected,
				.symlinks_only = ppath->symlinks_only,
			};
		} else {
			if (fnmatch(ppath->relative_pattern, relative_name, FNM_PATHNAME) != 0)
				continue;

			dir->protected = ppath->protected;
			dir->symlinks_only = ppath->symlinks_only;
		}
		dir->has_protected_children |= ppath->protected;
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

static void apk_db_dir_apply_diri_permissions(struct apk_db_dir_instance *diri)
{
	struct apk_db_dir *dir = diri->dir;

	if (diri->uid < dir->uid ||
	    (diri->uid == dir->uid && diri->gid < dir->gid)) {
		dir->uid = diri->uid;
		dir->gid = diri->gid;
		dir->mode = diri->mode;
	} else if (diri->uid == dir->uid && diri->gid == dir->gid) {
		dir->mode &= diri->mode;
	}
}

static void apk_db_diri_set(struct apk_db_dir_instance *diri, mode_t mode,
			    uid_t uid, gid_t gid)
{
	diri->mode = mode & 07777;
	diri->uid = uid;
	diri->gid = gid;
	apk_db_dir_apply_diri_permissions(diri);
}

static void apk_db_diri_free(struct apk_database *db,
			     struct apk_db_dir_instance *diri,
			     int allow_rmdir)
{
	if (allow_rmdir == APK_DISALLOW_RMDIR &&
	    diri->dir->recalc_mode)
		apk_db_dir_apply_diri_permissions(diri);

	apk_db_dir_unref(db, diri->dir, allow_rmdir);
	free(diri);
}

struct apk_db_file *apk_db_file_query(struct apk_database *db,
				      apk_blob_t dir,
				      apk_blob_t name)
{
	struct apk_db_file_hash_key key;

	if (dir.len && dir.ptr[dir.len-1] == '/')
		dir.len--;

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

static void apk_db_file_set(struct apk_db_file *file, mode_t mode, uid_t uid, gid_t gid)
{
	file->mode = mode & 07777;
	file->uid = uid;
	file->gid = gid;
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
	struct apk_name *rname, **rd;
	struct apk_dependency *d;

	foreach_array_item(d, pkg->depends) {
		rname = d->name;
		foreach_array_item(rd, rname->rdepends)
			if (*rd == pkg->name)
				goto rdeps_done;
		*apk_name_array_add(&rname->rdepends) = pkg->name;
rdeps_done: ;
	}
	foreach_array_item(d, pkg->install_if) {
		rname = d->name;
		foreach_array_item(rd, rname->rinstall_if)
			if (*rd == pkg->name)
				goto riif_done;
		*apk_name_array_add(&rname->rinstall_if) = pkg->name;
riif_done: ;
	}
	return;
}

static inline void add_provider(struct apk_name *name, struct apk_provider p)
{
	*apk_provider_array_add(&name->providers) = p;
}

struct apk_package *apk_db_pkg_add(struct apk_database *db, struct apk_package *pkg)
{
	struct apk_package *idb;
	struct apk_dependency *dep;

	if (pkg->license == NULL)
		pkg->license = apk_blob_atomize(APK_BLOB_NULL);

	/* Set as "cached" if installing from specified file, and
	 * for virtual packages */
	if (pkg->filename != NULL || pkg->installed_size == 0)
		pkg->repos |= BIT(APK_REPOSITORY_CACHED);

	idb = apk_hash_get(&db->available.packages, APK_BLOB_CSUM(pkg->csum));
	if (idb == NULL) {
		idb = pkg;
		apk_hash_insert(&db->available.packages, pkg);
		add_provider(pkg->name, APK_PROVIDER_FROM_PACKAGE(pkg));
		foreach_array_item(dep, pkg->provides)
			add_provider(dep->name, APK_PROVIDER_FROM_PROVIDES(pkg, dep));
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

static int apk_pkg_format_cache_pkg(apk_blob_t to, struct apk_package *pkg)
{
	/* pkgname-1.0_alpha1.12345678.apk */
	apk_blob_push_blob(&to, APK_BLOB_STR(pkg->name->name));
	apk_blob_push_blob(&to, APK_BLOB_STR("-"));
	apk_blob_push_blob(&to, *pkg->version);
	apk_blob_push_blob(&to, APK_BLOB_STR("."));
	apk_blob_push_hexdump(&to, APK_BLOB_PTR_LEN((char *) pkg->csum.data,
						    APK_CACHE_CSUM_BYTES));
	apk_blob_push_blob(&to, APK_BLOB_STR(".apk"));
	apk_blob_push_blob(&to, APK_BLOB_PTR_LEN("", 1));
	if (APK_BLOB_IS_NULL(to))
		return -ENOBUFS;
	return 0;
}

int apk_repo_format_cache_index(apk_blob_t to, struct apk_repository *repo)
{
	/* APKINDEX.12345678.tar.gz */
	apk_blob_push_blob(&to, APK_BLOB_STR("APKINDEX."));
	apk_blob_push_hexdump(&to, APK_BLOB_PTR_LEN((char *) repo->csum.data, APK_CACHE_CSUM_BYTES));
	apk_blob_push_blob(&to, APK_BLOB_STR(".tar.gz"));
	apk_blob_push_blob(&to, APK_BLOB_PTR_LEN("", 1));
	if (APK_BLOB_IS_NULL(to))
		return -ENOBUFS;
	return 0;
}

int apk_repo_format_real_url(struct apk_database *db, struct apk_repository *repo,
			     struct apk_package *pkg, char *buf, size_t len)
{
	int r;

	if (pkg != NULL)
		r = snprintf(buf, len, "%s%s" BLOB_FMT "/"  PKG_FILE_FMT,
			     repo->url, repo->url[strlen(repo->url)-1] == '/' ? "" : "/",
			     BLOB_PRINTF(*db->arch), PKG_FILE_PRINTF(pkg));
	else
		r = snprintf(buf, len, "%s%s" BLOB_FMT "/%s",
			     repo->url, repo->url[strlen(repo->url)-1] == '/' ? "" : "/",
			     BLOB_PRINTF(*db->arch), apkindex_tar_gz);
	if (r >= len)
		return -ENOBUFS;
	return 0;
}

int apk_repo_format_item(struct apk_database *db, struct apk_repository *repo, struct apk_package *pkg,
			 int *fd, char *buf, size_t len)
{
	if (repo->url == apk_linked_cache_dir) {
		*fd = db->cache_fd;
		return apk_pkg_format_cache_pkg(APK_BLOB_PTR_LEN(buf, len), pkg);
	} else {
		*fd = AT_FDCWD;
		return apk_repo_format_real_url(db, repo, pkg, buf, len);
	}
}

int apk_cache_download(struct apk_database *db, struct apk_repository *repo,
		       struct apk_package *pkg, int verify,
		       apk_progress_cb cb, void *cb_ctx)
{
	struct apk_istream *is;
	struct apk_bstream *bs;
	struct apk_sign_ctx sctx;
	char url[PATH_MAX];
	char tmpcacheitem[128], *cacheitem = &tmpcacheitem[tmpprefix.len];
	apk_blob_t b = APK_BLOB_BUF(tmpcacheitem);
	int r, fd;

	apk_blob_push_blob(&b, tmpprefix);
	if (pkg != NULL)
		r = apk_pkg_format_cache_pkg(b, pkg);
	else
		r = apk_repo_format_cache_index(b, repo);
	if (r < 0)
		return r;

	r = apk_repo_format_real_url(db, repo, pkg, url, sizeof(url));
	if (r < 0)
		return r;

	apk_message("fetch %s", url);

	if (apk_flags & APK_SIMULATE)
		return 0;

	if (cb)
		cb(cb_ctx, 0);

	if (verify != APK_SIGN_NONE) {
		apk_sign_ctx_init(&sctx, APK_SIGN_VERIFY, NULL, db->keys_fd);
		bs = apk_bstream_from_url(url);
		bs = apk_bstream_tee(bs, db->cache_fd, tmpcacheitem, cb, cb_ctx);
		is = apk_bstream_gunzip_mpart(bs, apk_sign_ctx_mpart_cb, &sctx);
		r = apk_tar_parse(is, apk_sign_ctx_verify_tar, &sctx, FALSE, &db->id_cache);
		apk_sign_ctx_free(&sctx);
	} else {
		is = apk_istream_from_url(url);
		fd = openat(db->cache_fd, tmpcacheitem, O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
		if (fd >= 0) {
			r = apk_istream_splice(is, fd, APK_SPLICE_ALL, cb, cb_ctx);
			close(fd);
		} else {
			r = -errno;
		}
	}
	is->close(is);
	if (r < 0) {
		unlinkat(db->cache_fd, tmpcacheitem, 0);
		return r;
	}

	if (renameat(db->cache_fd, tmpcacheitem, db->cache_fd, cacheitem) < 0)
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
			(void) apk_db_file_get(db, diri, bfile, &file_diri_node);
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
	mode_t mode;
	uid_t uid;
	gid_t gid;
	int field, r;

	while (!APK_BLOB_IS_NULL(l = bs->read(bs, token))) {
		if (l.len < 2 || l.ptr[1] != ':') {
			if (pkg == NULL)
				continue;

			if (repo >= 0) {
				pkg->repos |= BIT(repo);
			} else if (repo == -1 && ipkg == NULL) {
				/* Installed package without files */
				ipkg = apk_pkg_install(db, pkg);
			}

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
		r = apk_pkg_add_info(db, pkg, field, l);
		if (r == 0) {
			continue;
		}
		if (r == 1 && repo == -1 && ipkg == NULL) {
			/* Instert to installed database; this needs to
			 * happen after package name has been read, but
			 * before first FDB entry. */
			ipkg = apk_pkg_install(db, pkg);
			diri_node = hlist_tail_ptr(&ipkg->owned_dirs);
		}
		if (repo != -1 || ipkg == NULL)
			continue;

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
		case 'a':
			if (file == NULL) {
				apk_error("FDB file attribute metadata entry before file entry");
				return -1;
			}
		case 'M':
			if (diri == NULL) {
				apk_error("FDB directory metadata entry before directory entry");
				return -1;
			}
			uid = apk_blob_pull_uint(&l, 10);
			apk_blob_pull_char(&l, ':');
			gid = apk_blob_pull_uint(&l, 10);
			apk_blob_pull_char(&l, ':');
			mode = apk_blob_pull_uint(&l, 8);
			if (field == 'M')
				apk_db_diri_set(diri, mode, uid, gid);
			else
				apk_db_file_set(file, mode, uid, gid);
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
		case 'r':
			apk_blob_pull_deps(&l, db, &ipkg->replaces);
			break;
		case 'q':
			ipkg->replaces_priority = apk_blob_pull_uint(&l, 10);
			break;
		case 'p':
		case 's':
			ipkg->repository_tag = apk_db_get_tag_id(db, l);
			break;
		case 'f':
			for (r = 0; r < l.len; r++) {
				switch (l.ptr[r]) {
				case 'f': ipkg->broken_files = 1; break;
				case 's': ipkg->broken_script = 1; break;
				default:
					if (!(apk_flags & APK_FORCE))
						goto old_apk_tools;
				}
			}
			break;
		default:
			if (r != 0 && !(apk_flags & APK_FORCE))
				goto old_apk_tools;
			/* Installed. So mark the package as installable. */
			pkg->filename = NULL;
			continue;
		}
		if (APK_BLOB_IS_NULL(l)) {
			apk_error("FDB format error in entry '%c'", field);
			return -1;
		}
	}
	return 0;
old_apk_tools:
	/* Installed db should not have unsupported fields */
	apk_error("This apk-tools is too old to handle installed packages");
	return -1;
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

		if (ipkg->replaces->num) {
			apk_blob_push_blob(&bbuf, APK_BLOB_STR("r:"));
			apk_blob_push_deps(&bbuf, db, ipkg->replaces);
			apk_blob_push_blob(&bbuf, APK_BLOB_STR("\n"));
		}
		if (ipkg->replaces_priority) {
			apk_blob_push_blob(&bbuf, APK_BLOB_STR("q:"));
			apk_blob_push_uint(&bbuf, ipkg->replaces_priority, 10);
			apk_blob_push_blob(&bbuf, APK_BLOB_STR("\n"));
		}
		if (ipkg->repository_tag) {
			apk_blob_push_blob(&bbuf, APK_BLOB_STR("s:"));
			apk_blob_push_blob(&bbuf, db->repo_tags[ipkg->repository_tag].plain_name);
			apk_blob_push_blob(&bbuf, APK_BLOB_STR("\n"));
		}
		if (ipkg->broken_files || ipkg->broken_script) {
			apk_blob_push_blob(&bbuf, APK_BLOB_STR("f:"));
			if (ipkg->broken_files)
				apk_blob_push_blob(&bbuf, APK_BLOB_STR("f"));
			if (ipkg->broken_script)
				apk_blob_push_blob(&bbuf, APK_BLOB_STR("s"));
			apk_blob_push_blob(&bbuf, APK_BLOB_STR("\n"));
		}
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
				if (file->mode != 0 || file->uid != 0 || file->gid != 0) {
					apk_blob_push_blob(&bbuf, APK_BLOB_STR("\na:"));
					apk_blob_push_uint(&bbuf, file->uid, 10);
					apk_blob_push_blob(&bbuf, APK_BLOB_STR(":"));
					apk_blob_push_uint(&bbuf, file->gid, 10);
					apk_blob_push_blob(&bbuf, APK_BLOB_STR(":"));
					apk_blob_push_uint(&bbuf, file->mode, 8);
				}
				if (file->csum.type != APK_CHECKSUM_NONE) {
					apk_blob_push_blob(&bbuf, APK_BLOB_STR("\nZ:"));
					apk_blob_push_csum(&bbuf, &file->csum);
				}
				apk_blob_push_blob(&bbuf, APK_BLOB_STR("\n"));

				if (os->write(os, buf, bbuf.ptr - buf) != bbuf.ptr - buf)
					return -EIO;
				bbuf = APK_BLOB_BUF(buf);
			}
			if (os->write(os, buf, bbuf.ptr - buf) != bbuf.ptr - buf)
				return -EIO;
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
			apk_blob_push_blob(&bfn, *pkg->version);
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
	char **trigger;

	list_for_each_entry(ipkg, &db->installed.triggers, trigger_pkgs_list) {
		bfn = APK_BLOB_BUF(buf);
		apk_blob_push_csum(&bfn, &ipkg->pkg->csum);
		bfn = apk_blob_pushed(APK_BLOB_BUF(buf), bfn);
		os->write(os, bfn.ptr, bfn.len);

		foreach_array_item(trigger, ipkg->triggers) {
			os->write(os, " ", 1);
			apk_ostream_write_string(os, *trigger);
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
		if (ipkg->triggers->num != 0 &&
		    !list_hashed(&ipkg->trigger_pkgs_list))
			list_add_tail(&ipkg->trigger_pkgs_list,
				      &db->installed.triggers);
	}
}

static int apk_db_read_state(struct apk_database *db, int flags)
{
	struct apk_istream *is;
	struct apk_bstream *bs;
	apk_blob_t blob, world;
	int r;

	/* Read:
	 * 1. /etc/apk/world
	 * 2. installed packages db
	 * 3. triggers db
	 * 4. scripts db
	 */
	if (!(flags & APK_OPENF_NO_WORLD)) {
		blob = world = apk_blob_from_file(db->root_fd, apk_world_file);
		if (APK_BLOB_IS_NULL(blob))
			return -ENOENT;
		blob = apk_blob_trim(blob);
		if (apk_blob_chr(blob, ' '))
			db->compat_old_world = 1;
		apk_blob_pull_deps(&blob, db, &db->world);
		free(world.ptr);
	}

	if (!(flags & APK_OPENF_NO_INSTALLED)) {
		bs = apk_bstream_from_file(db->root_fd, apk_installed_file);
		if (bs != NULL) {
			r = apk_db_index_read(db, bs, -1);
			bs->close(bs, NULL);
			if (r != 0)
				return -1;
		}

		bs = apk_bstream_from_file(db->root_fd, apk_triggers_file);
		if (bs != NULL) {
			apk_db_triggers_read(db, bs);
			bs->close(bs, NULL);
		}
	}

	if (!(flags & APK_OPENF_NO_SCRIPTS)) {
		is = apk_istream_from_file(db->root_fd, apk_scripts_file);
		if (is != NULL) {
			apk_tar_parse(is, apk_read_script_archive_entry, db,
				      FALSE, &db->id_cache);
			is->close(is);
		}
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
		return -EIO;

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
		return -EIO;

	ctx.os = os;
	list_for_each_entry(ipkg, &db->installed.packages, installed_pkgs_list) {
		struct apk_package *pkg = ipkg->pkg;
		if (pkg->repos != BIT(APK_REPOSITORY_CACHED))
			continue;
		r = write_index_entry(pkg, &ctx);
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
	int r;

	r = apk_hash_foreach(&db->available.packages, write_index_entry, &ctx);
	if (r < 0)
		return r;

	return ctx.count;
}

static int add_protected_path(void *ctx, apk_blob_t blob)
{
	struct apk_database *db = (struct apk_database *) ctx;
	int protected = 0, symlinks_only = 0;

	/* skip empty lines and comments */
	if (blob.len == 0)
		return 0;

	switch (blob.ptr[0]) {
	case '#':
		return 0;
	case '-':
		blob.ptr++;
		blob.len--;
		break;
	case '@':
		protected = 1;
		symlinks_only = 1;
		blob.ptr++;
		blob.len--;
		break;
	case '+':
		protected = 1;
		blob.ptr++;
		blob.len--;
		break;
	default:
		protected = 1;
		break;
	}

	/* skip leading and trailing path separators */
	while (blob.len && blob.ptr[0] == '/')
		blob.ptr++, blob.len--;
	while (blob.len && blob.ptr[blob.len-1] == '/')
		blob.len--;

	*apk_protected_path_array_add(&db->protected_paths) = (struct apk_protected_path) {
		.relative_pattern = apk_blob_cstr(blob),
		.protected = protected,
		.symlinks_only = symlinks_only,
	};

	return 0;
}

static int file_ends_with_dot_list(const char *file)
{
	const char *ext = strrchr(file, '.');
	if (ext == NULL || strcmp(ext, ".list") != 0)
		return FALSE;
	return TRUE;
}

static int add_protected_paths_from_file(void *ctx, int dirfd, const char *file)
{
	struct apk_database *db = (struct apk_database *) ctx;
	apk_blob_t blob;

	if (!file_ends_with_dot_list(file))
		return 0;

	blob = apk_blob_from_file(dirfd, file);
	if (APK_BLOB_IS_NULL(blob))
		return 0;

	apk_blob_for_each_segment(blob, "\n", add_protected_path, db);
	free(blob.ptr);

	return 0;
}

static int apk_db_create(struct apk_database *db)
{
	int fd;

	mkdirat(db->root_fd, "tmp", 01777);
	mkdirat(db->root_fd, "dev", 0755);
	mknodat(db->root_fd, "dev/null", S_IFCHR | 0666, makedev(1, 3));
	mkdirat(db->root_fd, "etc", 0755);
	mkdirat(db->root_fd, "etc/apk", 0755);
	mkdirat(db->root_fd, "lib", 0755);
	mkdirat(db->root_fd, "lib/apk", 0755);
	mkdirat(db->root_fd, "lib/apk/db", 0755);
	mkdirat(db->root_fd, "var", 0755);
	mkdirat(db->root_fd, "var/cache", 0755);
	mkdirat(db->root_fd, "var/cache/apk", 0755);
	mkdirat(db->root_fd, "var/cache/misc", 0755);
	mkdirat(db->root_fd, "var/lock", 0755);

	fd = openat(db->root_fd, apk_world_file, O_CREAT|O_RDWR|O_TRUNC|O_CLOEXEC, 0644);
	if (fd < 0)
		return -errno;
	close(fd);

	return 0;
}

static void handle_alarm(int sig)
{
}

static char *find_mountpoint(int atfd, const char *rel_path)
{
	struct mntent *me;
	struct stat64 st;
	FILE *f;
	char *ret = NULL;
	dev_t dev;

	if (fstatat64(atfd, rel_path, &st, 0) != 0)
		return NULL;
	dev = st.st_dev;

	f = setmntent("/proc/mounts", "r");
	if (f == NULL)
		return NULL;
	while ((me = getmntent(f)) != NULL) {
		if (strcmp(me->mnt_fsname, "rootfs") == 0)
			continue;
		if (fstatat64(atfd, me->mnt_dir, &st, 0) == 0 &&
		    st.st_dev == dev) {
			ret = strdup(me->mnt_dir);
			break;
		}
	}
	endmntent(f);

	return ret;
}

static int do_remount(const char *path, const char *option)
{
	pid_t pid;
	int status;

	pid = fork();
	if (pid < 0)
		return -errno;

	if (pid == 0) {
		execl("/bin/mount", "mount", "-o", "remount", "-o",
		      option, path, NULL);
		return 1;
	}

	waitpid(pid, &status, 0);
	if (!WIFEXITED(status))
		return -1;

	return WEXITSTATUS(status);
}

static void relocate_database(struct apk_database *db)
{
	mkdirat(db->root_fd, "etc", 0755);
	mkdirat(db->root_fd, "etc/apk", 0755);
	mkdirat(db->root_fd, "lib", 0755);
	mkdirat(db->root_fd, "lib/apk", 0755);
	mkdirat(db->root_fd, "lib/apk/db", 0755);
	mkdirat(db->root_fd, "var", 0755);
	mkdirat(db->root_fd, "var/cache", 0755);
	mkdirat(db->root_fd, "var/cache/apk", 0755);
	mkdirat(db->root_fd, "var/cache/misc", 0755);
	mkdirat(db->root_fd, "var/lock", 0755);
	apk_move_file(db->root_fd, apk_world_file_old, apk_world_file);
	apk_move_file(db->root_fd, apk_scripts_file_old, apk_scripts_file);
	apk_move_file(db->root_fd, apk_triggers_file_old, apk_triggers_file);
	apk_move_file(db->root_fd, apk_installed_file_old, apk_installed_file);
}

static void mark_in_cache(struct apk_database *db, int dirfd, const char *name, struct apk_package *pkg)
{
	if (pkg == NULL)
		return;

	pkg->repos |= BIT(APK_REPOSITORY_CACHED);
}

static int add_repos_from_file(void *ctx, int dirfd, const char *file)
{
	struct apk_database *db = (struct apk_database *) ctx;
	apk_blob_t blob;

	if (dirfd != db->root_fd) {
		/* loading from repositories.d; check extension */
		if (!file_ends_with_dot_list(file))
			return 0;
	}

	blob = apk_blob_from_file(dirfd, file);
	if (APK_BLOB_IS_NULL(blob))
		return 0;

	apk_blob_for_each_segment(blob, "\n", apk_db_add_repository, db);
	free(blob.ptr);

	return 0;
}

static void apk_db_setup_repositories(struct apk_database *db)
{
	db->repos[APK_REPOSITORY_CACHED] = (struct apk_repository) {
		.url = apk_linked_cache_dir,
		.csum.data = {
			0xb0,0x35,0x92,0x80,0x6e,0xfa,0xbf,0xee,0xb7,0x09,
			0xf5,0xa7,0x0a,0x7c,0x17,0x26,0x69,0xb0,0x05,0x38 },
		.csum.type = APK_CHECKSUM_SHA1,
	};

	db->num_repos = APK_REPOSITORY_FIRST_CONFIGURED;
	db->local_repos |= BIT(APK_REPOSITORY_CACHED);
	db->available_repos |= BIT(APK_REPOSITORY_CACHED);

	db->num_repo_tags = 1;
}

int apk_db_open(struct apk_database *db, struct apk_db_options *dbopts)
{
	const char *msg = NULL;
	struct apk_repository_list *repo = NULL;
	struct apk_bstream *bs;
	struct statfs stfs;
	struct statvfs stvfs;
	apk_blob_t blob;
	int r, fd, write_arch = FALSE;

	memset(db, 0, sizeof(*db));
	if (apk_flags & APK_SIMULATE) {
		dbopts->open_flags &= ~(APK_OPENF_CREATE | APK_OPENF_WRITE);
		dbopts->open_flags |= APK_OPENF_READ;
	}
	if (dbopts->open_flags == 0) {
		msg = "Invalid open flags (internal error)";
		r = -1;
		goto ret_r;
	}

	apk_hash_init(&db->available.names, &pkg_name_hash_ops, 4000);
	apk_hash_init(&db->available.packages, &pkg_info_hash_ops, 10000);
	apk_hash_init(&db->installed.dirs, &dir_hash_ops, 5000);
	apk_hash_init(&db->installed.files, &file_hash_ops, 100000);
	list_init(&db->installed.packages);
	list_init(&db->installed.triggers);
	apk_dependency_array_init(&db->world);
	apk_protected_path_array_init(&db->protected_paths);
	db->permanent = 1;

	apk_db_setup_repositories(db);

	db->root = strdup(dbopts->root ?: "/");
	db->root_fd = openat(AT_FDCWD, db->root, O_RDONLY | O_CLOEXEC);
	if (db->root_fd < 0 && (dbopts->open_flags & APK_OPENF_CREATE)) {
		mkdirat(AT_FDCWD, db->root, 0755);
		db->root_fd = openat(AT_FDCWD, db->root, O_RDONLY | O_CLOEXEC);
	}
	if (db->root_fd < 0) {
		msg = "Unable to open root";
		goto ret_errno;
	}
	if (fstatfs(db->root_fd, &stfs) == 0 &&
	    stfs.f_type == 0x01021994 /* TMPFS_MAGIC */)
		db->permanent = 0;

	if (dbopts->root && dbopts->arch) {
		db->arch = apk_blob_atomize(APK_BLOB_STR(dbopts->arch));
		write_arch = TRUE;
	} else {
		apk_blob_t arch;
		arch = apk_blob_from_file(db->root_fd, apk_arch_file);
		if (!APK_BLOB_IS_NULL(arch)) {
			db->arch = apk_blob_atomize_dup(apk_blob_trim(arch));
			free(arch.ptr);
		} else {
			db->arch = apk_blob_atomize(APK_BLOB_STR(APK_DEFAULT_ARCH));
			write_arch = TRUE;
		}
	}

	apk_id_cache_init(&db->id_cache, db->root_fd);

	if (dbopts->open_flags & APK_OPENF_WRITE) {
		if (faccessat(db->root_fd, apk_installed_file_old, F_OK, 0) == 0)
			relocate_database(db);

		db->lock_fd = openat(db->root_fd, apk_lock_file,
				     O_CREAT | O_RDWR | O_CLOEXEC, 0400);
		if (db->lock_fd < 0 && errno == ENOENT &&
		    (dbopts->open_flags & APK_OPENF_CREATE)) {
			r = apk_db_create(db);
			if (r != 0) {
				msg = "Unable to create database";
				goto ret_r;
			}
			db->lock_fd = openat(db->root_fd, apk_lock_file,
					     O_CREAT | O_RDWR | O_CLOEXEC, 0400);
		}
		if (db->lock_fd < 0 ||
		    flock(db->lock_fd, LOCK_EX | LOCK_NB) < 0) {
			msg = "Unable to lock database";
			if (dbopts->lock_wait) {
				struct sigaction sa, old_sa;

				apk_message("Waiting for repository lock");
				memset(&sa, 0, sizeof sa);
				sa.sa_handler = handle_alarm;
				sa.sa_flags   = SA_ONESHOT;
				sigaction(SIGALRM, &sa, &old_sa);

				alarm(dbopts->lock_wait);
				if (flock(db->lock_fd, LOCK_EX) < 0)
					goto ret_errno;

				alarm(0);
				sigaction(SIGALRM, &old_sa, NULL);
			} else
				goto ret_errno;
		}
		if (write_arch)
			apk_blob_to_file(db->root_fd, apk_arch_file, *db->arch, APK_BTF_ADD_EOL);
	}

	blob = APK_BLOB_STR("+etc\n" "@etc/init.d\n");
	apk_blob_for_each_segment(blob, "\n", add_protected_path, db);

	apk_dir_foreach_file(openat(db->root_fd, "etc/apk/protected_paths.d", O_RDONLY | O_CLOEXEC),
			     add_protected_paths_from_file, db);

	/* figure out where to have the cache */
	fd = openat(db->root_fd, apk_linked_cache_dir, O_RDONLY | O_CLOEXEC);
	if (fd >= 0 && fstatfs(fd, &stfs) == 0 && stfs.f_type != 0x01021994 /* TMPFS_MAGIC */) {
		db->cache_dir = apk_linked_cache_dir;
		db->cache_fd = fd;
		if ((dbopts->open_flags & (APK_OPENF_WRITE | APK_OPENF_CACHE_WRITE)) &&
		    fstatvfs(fd, &stvfs) == 0 && (stvfs.f_flag & ST_RDONLY) != 0) {
			/* remount cache read/write */
			db->cache_remount_dir = find_mountpoint(db->root_fd, db->cache_dir);
			if (db->cache_remount_dir == NULL) {
				apk_warning("Unable to find cache directory mount point");
			} else if (do_remount(db->cache_remount_dir, "rw") != 0) {
				free(db->cache_remount_dir);
				db->cache_remount_dir = NULL;
				apk_error("Unable to remount cache read/write");
				r = EROFS;
				goto ret_r;
			}
		}
	} else {
		if (fd >= 0)
			close(fd);
		db->cache_dir = apk_static_cache_dir;
		db->cache_fd = openat(db->root_fd, db->cache_dir, O_RDONLY | O_CLOEXEC);
	}

	db->keys_fd = openat(db->root_fd,
			     dbopts->keys_dir ?: "etc/apk/keys",
			     O_RDONLY | O_CLOEXEC);

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
		list_for_each_entry(repo, &dbopts->repository_list, list)
			apk_db_add_repository(db, APK_BLOB_STR(repo->url));

		if (dbopts->repositories_file == NULL) {
			add_repos_from_file(db, db->root_fd, "etc/apk/repositories");
			apk_dir_foreach_file(openat(db->root_fd, "etc/apk/repositories.d", O_RDONLY | O_CLOEXEC),
					     add_repos_from_file, db);
		} else {
			add_repos_from_file(db, db->root_fd, dbopts->repositories_file);
		}

		if (apk_flags & APK_UPDATE_CACHE)
			apk_db_index_write_nr_cache(db);
	}

	if (apk_db_cache_active(db))
		apk_db_cache_foreach_item(db, mark_in_cache);

	if (db->compat_newfeatures) {
		apk_warning("This apk-tools is OLD! Some packages %s.",
			    db->compat_notinstallable ?
			    "are not installable" :
			    "might not function properly");
	}

	return 0;

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
				 apk_world_file,
				 apk_world_file_tmp,
				 0644);
	if (os == NULL)
		return -1;

	apk_deps_write(db, db->world, os, APK_BLOB_PTR_LEN(db->compat_old_world ? " " : "\n", 1));
	os->write(os, "\n", 1);
	r = os->close(os);
	if (r < 0)
		return r;

	os = apk_ostream_to_file(db->root_fd,
				 apk_installed_file,
				 apk_installed_file_tmp,
				 0644);
	if (os == NULL)
		return -1;
	apk_db_write_fdb(db, os);
	r = os->close(os);
	if (r < 0)
		return r;

	os = apk_ostream_to_file(db->root_fd,
				 apk_scripts_file,
				 apk_scripts_file_tmp,
				 0644);
	if (os == NULL)
		return -1;
	apk_db_scriptdb_write(db, os);
	r = os->close(os);
	if (r < 0)
		return r;

	apk_db_index_write_nr_cache(db);

	os = apk_ostream_to_file(db->root_fd,
				 apk_triggers_file,
				 apk_triggers_file_tmp,
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
	struct apk_protected_path *ppath;
	struct hlist_node *dc, *dn;
	int i;

	apk_id_cache_free(&db->id_cache);

	/* Cleaning up the directory tree will cause mode, uid and gid
	 * of all modified (package providing that directory got removed)
	 * directories to be reset. */
	list_for_each_entry(ipkg, &db->installed.packages, installed_pkgs_list) {
		hlist_for_each_entry_safe(diri, dc, dn, &ipkg->owned_dirs, pkg_dirs_list) {
			apk_db_diri_free(db, diri, APK_DISALLOW_RMDIR);
		}
	}

	for (i = APK_REPOSITORY_FIRST_CONFIGURED; i < db->num_repos; i++) {
		free((void*) db->repos[i].url);
		free(db->repos[i].description.ptr);
	}
	foreach_array_item(ppath, db->protected_paths)
		free(ppath->relative_pattern);
	apk_protected_path_array_free(&db->protected_paths);

	apk_dependency_array_free(&db->world);

	apk_hash_free(&db->available.packages);
	apk_hash_free(&db->available.names);
	apk_hash_free(&db->installed.files);
	apk_hash_free(&db->installed.dirs);

	if (db->keys_fd)
		close(db->keys_fd);
	if (db->cache_fd)
		close(db->cache_fd);
	if (db->root_fd)
		close(db->root_fd);
	if (db->lock_fd)
		close(db->lock_fd);
	if (db->root != NULL)
		free(db->root);

	if (db->cache_remount_dir) {
		do_remount(db->cache_remount_dir, "ro");
		free(db->cache_remount_dir);
		db->cache_remount_dir = NULL;
	}
}

int apk_db_get_tag_id(struct apk_database *db, apk_blob_t tag)
{
	int i;

	if (APK_BLOB_IS_NULL(tag))
		return APK_DEFAULT_REPOSITORY_TAG;

	if (tag.ptr[0] == '@') {
		for (i = 1; i < db->num_repo_tags; i++)
			if (apk_blob_compare(db->repo_tags[i].tag, tag) == 0)
				return i;
	} else {
		for (i = 1; i < db->num_repo_tags; i++)
			if (apk_blob_compare(db->repo_tags[i].plain_name, tag) == 0)
				return i;
	}
	if (i >= ARRAY_SIZE(db->repo_tags))
		return -1;

	db->num_repo_tags++;

	if (tag.ptr[0] == '@') {
		db->repo_tags[i].tag = *apk_blob_atomize_dup(tag);
	} else {
		char *tmp = alloca(tag.len + 1);
		tmp[0] = '@';
		memcpy(&tmp[1], tag.ptr, tag.len);
		db->repo_tags[i].tag = *apk_blob_atomize_dup(APK_BLOB_PTR_LEN(tmp, tag.len+1));
	}

	db->repo_tags[i].plain_name = db->repo_tags[i].tag;
	apk_blob_pull_char(&db->repo_tags[i].plain_name, '@');

	return i;
}

static int fire_triggers(apk_hash_item item, void *ctx)
{
	struct apk_database *db = (struct apk_database *) ctx;
	struct apk_db_dir *dbd = (struct apk_db_dir *) item;
	struct apk_installed_package *ipkg;
	int i;

	list_for_each_entry(ipkg, &db->installed.triggers, trigger_pkgs_list) {
		if (!ipkg->run_all_triggers && !dbd->modified)
			continue;

		for (i = 0; i < ipkg->triggers->num; i++) {
			if (ipkg->triggers->item[i][0] != '/')
				continue;

			if (fnmatch(ipkg->triggers->item[i], dbd->rooted_name,
				    FNM_PATHNAME) != 0)
				continue;

			/* And place holder for script name */
			if (ipkg->pending_triggers->num == 0) {
				*apk_string_array_add(&ipkg->pending_triggers) =
					NULL;
				db->pending_triggers++;
			}
			*apk_string_array_add(&ipkg->pending_triggers) =
				dbd->rooted_name;
			break;
		}
	}

	return 0;
}

int apk_db_fire_triggers(struct apk_database *db)
{
	apk_hash_foreach(&db->installed.dirs, fire_triggers, db);
	return db->pending_triggers;
}

int apk_db_cache_active(struct apk_database *db)
{
	return db->cache_dir != apk_static_cache_dir;
}

struct foreach_cache_item_ctx {
	struct apk_database *db;
	apk_cache_item_cb cb;
};

static int foreach_cache_file(void *pctx, int dirfd, const char *name)
{
	struct foreach_cache_item_ctx *ctx = (struct foreach_cache_item_ctx *) pctx;
	struct apk_database *db = ctx->db;
	struct apk_package *pkg = NULL;
	struct apk_provider *p0;
	apk_blob_t b = APK_BLOB_STR(name), bname, bver;

	if (apk_pkg_parse_name(b, &bname, &bver) == 0) {
		/* Package - search for it */
		struct apk_name *name = apk_db_get_name(db, bname);
		char tmp[PATH_MAX];
		if (name == NULL)
			goto no_pkg;

		foreach_array_item(p0, name->providers) {
			if (p0->pkg->name != name)
				continue;

			apk_pkg_format_cache_pkg(APK_BLOB_BUF(tmp), p0->pkg);
			if (apk_blob_compare(b, APK_BLOB_STR(tmp)) == 0) {
				pkg = p0->pkg;
				break;
			}
		}
	}
no_pkg:
	ctx->cb(db, dirfd, name, pkg);

	return 0;
}

int apk_db_cache_foreach_item(struct apk_database *db, apk_cache_item_cb cb)
{
	struct foreach_cache_item_ctx ctx = { db, cb };

	return apk_dir_foreach_file(dup(db->cache_fd), foreach_cache_file, &ctx);
}

int apk_db_permanent(struct apk_database *db)
{
	return db->permanent;
}

int apk_db_check_world(struct apk_database *db, struct apk_dependency_array *world)
{
	struct apk_dependency *dep;
	int bad = 0, tag;

	if (apk_flags & APK_FORCE)
		return 0;

	foreach_array_item(dep, world) {
		tag = dep->repository_tag;
		if (tag == 0 || db->repo_tags[tag].allowed_repos != 0)
			continue;
		if (tag < 0)
			tag = 0;
		apk_warning("The repository tag for world dependency '%s" BLOB_FMT "' does not exist",
			    dep->name->name, BLOB_PRINTF(db->repo_tags[tag].tag));
		bad++;
	}

	return bad;
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

	if (!apk_blob_rsplit(filename, '/', &key.dirname, &key.filename)) {
		key.dirname = APK_BLOB_NULL;
		key.filename = filename;
	}

	dbf = (struct apk_db_file *) apk_hash_get(&db->installed.files,
						  APK_BLOB_BUF(&key));
	if (dbf == NULL)
		return NULL;

	return dbf->diri->pkg;
}

unsigned int apk_db_get_pinning_mask_repos(struct apk_database *db, unsigned short pinning_mask)
{
	unsigned int repository_mask = 0;
	int i;

	for (i = 0; i < db->num_repo_tags && pinning_mask; i++) {
		if (!(BIT(i) & pinning_mask))
			continue;
		pinning_mask &= ~BIT(i);
		repository_mask |= db->repo_tags[i].allowed_repos;
	}
	return repository_mask;
}

struct apk_repository *apk_db_select_repo(struct apk_database *db,
					  struct apk_package *pkg)
{
	unsigned int repos;
	int i;

	/* Select repositories to use */
	repos = pkg->repos & db->available_repos;
	if (repos == 0)
		return NULL;

	if (repos & db->local_repos)
		repos &= db->local_repos;

	/* Pick first repository providing this package */
	for (i = APK_REPOSITORY_FIRST_CONFIGURED; i < APK_MAX_REPOS; i++) {
		if (repos & BIT(i))
			return &db->repos[i];
	}
	return &db->repos[APK_REPOSITORY_CACHED];
}

static int apk_repository_update(struct apk_database *db, struct apk_repository *repo)
{
	int r, verify = (apk_flags & APK_ALLOW_UNTRUSTED) ? APK_SIGN_NONE : APK_SIGN_VERIFY;

	r = apk_cache_download(db, repo, NULL, verify, NULL, NULL);
	if (r != 0)
		apk_error("%s: %s", repo->url, apk_error_str(r));

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
	int r;

	r = apk_sign_ctx_process_file(&ctx->sctx, fi, is);
	if (r <= 0)
		return r;

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
		r = apk_tar_parse(is, load_apkindex, &ctx, FALSE, &db->id_cache);
		is->close(is);
		apk_sign_ctx_free(&ctx.sctx);

		if (r >= 0 && ctx.found == 0)
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

int apk_db_add_repository(apk_database_t _db, apk_blob_t _repository)
{
	struct apk_database *db = _db.db;
	struct apk_bstream *bs = NULL;
	struct apk_repository *repo;
	apk_blob_t brepo, btag;
	int repo_num, r, targz = 1, tag_id = 0;
	char buf[PATH_MAX], *url;

	brepo = _repository;
	btag = APK_BLOB_NULL;
	if (brepo.ptr == NULL || brepo.len == 0 || *brepo.ptr == '#')
		return 0;

	if (brepo.ptr[0] == '@') {
		apk_blob_cspn(brepo, apk_spn_repo_separators, &btag, &brepo);
		apk_blob_spn(brepo, apk_spn_repo_separators, NULL, &brepo);
		tag_id = apk_db_get_tag_id(db, btag);
	}

	url = apk_blob_cstr(brepo);
	for (repo_num = 0; repo_num < db->num_repos; repo_num++) {
		repo = &db->repos[repo_num];
		if (strcmp(url, repo->url) == 0) {
			db->repo_tags[tag_id].allowed_repos |=
				BIT(repo_num) & db->available_repos;
			free(url);
			return 0;
		}
	}
	if (db->num_repos >= APK_MAX_REPOS) {
		free(url);
		return -1;
	}

	repo_num = db->num_repos++;
	repo = &db->repos[repo_num];
	*repo = (struct apk_repository) {
		.url = url,
	};

	apk_blob_checksum(brepo, apk_checksum_default(), &repo->csum);

	if (apk_url_local_file(repo->url) == NULL) {
		if (!(apk_flags & APK_NO_NETWORK)) {
			db->available_repos |= BIT(repo_num);
			if (apk_flags & APK_UPDATE_CACHE)
				apk_repository_update(db, repo);
		}
		r = apk_repo_format_cache_index(APK_BLOB_BUF(buf), repo);
	} else {
		db->local_repos |= BIT(repo_num);
		db->available_repos |= BIT(repo_num);
		r = apk_repo_format_real_url(db, repo, NULL, buf, sizeof(buf));
	}
	if (r == 0) {
		bs = apk_bstream_from_fd_url(db->cache_fd, buf);
		if (bs != NULL)
			r = load_index(db, bs, targz, repo_num);
		else
			r = -ENOENT;
	}

	if (r != 0) {
		apk_warning("Ignoring %s: %s", buf, apk_error_str(r));
		db->available_repos &= ~BIT(repo_num);
		r = 0;
	} else {
		db->repo_tags[tag_id].allowed_repos |= BIT(repo_num) & db->available_repos;
	}

	return 0;
}

static void extract_cb(void *_ctx, size_t bytes_done)
{
	struct install_ctx *ctx = (struct install_ctx *) _ctx;
	if (!ctx->cb)
		return;
	ctx->cb(ctx->cb_ctx, min(ctx->installed_size + bytes_done, ctx->pkg->installed_size));
}

static void apk_db_run_pending_script(struct install_ctx *ctx)
{
	if (ctx->script_pending && ctx->sctx.control_verified) {
		ctx->script_pending = FALSE;
		apk_ipkg_run_script(ctx->ipkg, ctx->db, ctx->script, ctx->script_args);
	}
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
		apk_blob_pull_deps(&r, db, &ipkg->replaces);
	} else if (apk_blob_compare(APK_BLOB_STR("replaces_priority"), l) == 0) {
		ipkg->replaces_priority = apk_blob_pull_uint(&r, 10);
	} else if (apk_blob_compare(APK_BLOB_STR("triggers"), l) == 0) {
		apk_string_array_resize(&ipkg->triggers, 0);
		apk_blob_for_each_segment(r, " ", parse_triggers, ctx->ipkg);

		if (ctx->ipkg->triggers->num != 0 &&
		    !list_hashed(&ipkg->trigger_pkgs_list))
			list_add_tail(&ipkg->trigger_pkgs_list,
				      &db->installed.triggers);
	} else {
		apk_sign_ctx_parse_pkginfo_line(&ctx->sctx, line);
	}
	return 0;
}

static struct apk_db_dir_instance *apk_db_install_directory_entry(struct install_ctx * ctx, apk_blob_t dir)
{
	struct apk_database *db = ctx->db;
	struct apk_package *pkg = ctx->pkg;
	struct apk_installed_package *ipkg = pkg->ipkg;
	struct apk_db_dir_instance *diri;

	if (ctx->diri_node == NULL)
		ctx->diri_node = hlist_tail_ptr(&ipkg->owned_dirs);
	ctx->diri = diri = apk_db_diri_new(db, pkg, dir, &ctx->diri_node);
	ctx->file_diri_node = hlist_tail_ptr(&diri->owned_files);

	return diri;
}

static int apk_db_install_archive_entry(void *_ctx,
					const struct apk_file_info *ae,
					struct apk_istream *is)
{
	struct install_ctx *ctx = (struct install_ctx *) _ctx;
	struct apk_database *db = ctx->db;
	struct apk_package *pkg = ctx->pkg, *opkg;
	struct apk_dependency *dep;
	struct apk_installed_package *ipkg = pkg->ipkg;
	apk_blob_t name = APK_BLOB_STR(ae->name), bdir, bfile;
	struct apk_db_dir_instance *diri = ctx->diri;
	struct apk_db_file *file;
	int r, type = APK_SCRIPT_INVALID;

	r = apk_sign_ctx_process_file(&ctx->sctx, ae, is);
	if (r <= 0)
		return r;

	/* Package metainfo and script processing */
	r = 0;
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
	}

	/* Handle script */
	if (type != APK_SCRIPT_INVALID) {
		apk_ipkg_add_script(ipkg, is, type, ae->size);
		if (type == ctx->script)
			ctx->script_pending = TRUE;
		apk_db_run_pending_script(ctx);
		return 0;
	}
	apk_db_run_pending_script(ctx);

	/* Installable entry */
	ctx->current_file_size = apk_calc_installed_size(ae->size);
	if (!S_ISDIR(ae->mode)) {
		if (!apk_blob_rsplit(name, '/', &bdir, &bfile)) {
			bdir = APK_BLOB_NULL;
			bfile = name;
		}

		if (bfile.len > 6 && memcmp(bfile.ptr, ".keep_", 6) == 0)
			return 0;

		/* Make sure the file is part of the cached directory tree */
		diri = find_diri(ipkg, bdir, diri, &ctx->file_diri_node);
		if (diri == NULL) {
			if (!APK_BLOB_IS_NULL(bdir)) {
				apk_error(PKG_VER_FMT": "BLOB_FMT": no dirent in archive\n",
					  pkg, BLOB_PRINTF(name));
				ipkg->broken_files = 1;
				return 0;
			}
			diri = apk_db_install_directory_entry(ctx, bdir);
		}

		opkg = NULL;
		file = apk_db_file_query(db, bdir, bfile);
		if (file != NULL) {
			opkg = file->diri->pkg;
			do {
				int opkg_prio = -1, pkg_prio = -1;

				/* Overlay file? */
				if (opkg->name == NULL)
					break;
				/* Upgrading package? */
				if (opkg->name == pkg->name)
					break;
				/* Does the original package replace the new one? */
				foreach_array_item(dep, opkg->ipkg->replaces) {
					if (apk_dep_is_materialized(dep, pkg)) {
						opkg_prio = opkg->ipkg->replaces_priority;
						break;
					}
				}
				/* Does the new package replace the original one? */
				foreach_array_item(dep, ctx->ipkg->replaces) {
					if (apk_dep_is_materialized(dep, opkg)) {
						pkg_prio = ctx->ipkg->replaces_priority;
						break;
					}
				}
				/* If the original package is more important,
				 * skip this file */
				if (opkg_prio > pkg_prio)
					return 0;
				/* If the new package has valid 'replaces', we
				 * will overwrite the file without warnings. */
				if (pkg_prio >= 0)
					break;

				if (!(apk_flags & APK_FORCE)) {
					apk_error(PKG_VER_FMT": trying to overwrite %s owned by "PKG_VER_FMT".",
						  PKG_VER_PRINTF(pkg), ae->name, PKG_VER_PRINTF(opkg));
					ipkg->broken_files = 1;
					return 0;
				}
				apk_warning(PKG_VER_FMT": overwriting %s owned by "PKG_VER_FMT".",
					    PKG_VER_PRINTF(pkg), ae->name, PKG_VER_PRINTF(opkg));
			} while (0);
		}

		if (opkg != pkg) {
			/* Create the file entry without adding it to hash */
			file = apk_db_file_new(diri, bfile, &ctx->file_diri_node);
		}

		if (apk_verbosity >= 3)
			apk_message("%s", ae->name);

		/* Extract the file as name.apk-new */
		apk_db_file_set(file, ae->mode, ae->uid, ae->gid);
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

		diri = apk_db_install_directory_entry(ctx, name);
		apk_db_diri_set(diri, ae->mode, ae->uid, ae->gid);
		apk_db_dir_mkdir(db, diri->dir);
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
			diri->dir->modified = 1;

		hlist_for_each_entry_safe(file, fc, fn, &diri->owned_files, diri_files_list) {
			snprintf(name, sizeof(name),
				 DIR_FILE_FMT "%s",
				 DIR_FILE_PRINTF(diri->dir, file),
				 exten ?: "");

			key = (struct apk_db_file_hash_key) {
				.dirname = APK_BLOB_PTR_LEN(diri->dir->name, diri->dir->namelen),
				.filename = APK_BLOB_PTR_LEN(file->name, file->namelen),
			};
			hash = apk_blob_hash_seed(key.filename, diri->dir->hash);
			if ((!diri->dir->protected) ||
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
		__hlist_del(dc, &ipkg->owned_dirs.first);
		apk_db_diri_free(db, diri, APK_ALLOW_RMDIR);
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
		dir->modified = 1;

		hlist_for_each_entry_safe(file, fc, fn, &diri->owned_files, diri_files_list) {
			snprintf(name, sizeof(name), DIR_FILE_FMT,
				 DIR_FILE_PRINTF(diri->dir, file));
			snprintf(tmpname, sizeof(tmpname), DIR_FILE_FMT ".apk-new",
				 DIR_FILE_PRINTF(diri->dir, file));

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
			if (ofile != NULL && diri->dir->protected)
				cstype = ofile->csum.type;
			cstype |= APK_FI_NOFOLLOW;

			r = apk_file_get_info(db->root_fd, name, cstype, &fi);
			if (ofile && ofile->diri->pkg->name == NULL) {
				/* File was from overlay, delete the
				 * packages version */
				unlinkat(db->root_fd, tmpname, 0);
			} else if ((diri->dir->protected) &&
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
			     int upgrade, apk_progress_cb cb, void *cb_ctx,
			     char **script_args)
{
	struct install_ctx ctx;
	struct apk_bstream *bs = NULL, *cache_bs;
	struct apk_istream *tar;
	struct apk_repository *repo;
	struct apk_package *pkg = ipkg->pkg;
	char file[PATH_MAX];
	char tmpcacheitem[128], *cacheitem = &tmpcacheitem[tmpprefix.len];
	int r, filefd = AT_FDCWD, need_copy = FALSE;

	if (pkg->filename == NULL) {
		repo = apk_db_select_repo(db, pkg);
		if (repo == NULL) {
			r = -ENOPKG;
			goto err_msg;
		}
		r = apk_repo_format_item(db, repo, pkg, &filefd, file, sizeof(file));
		if (r < 0)
			goto err_msg;
		if (!(pkg->repos & db->local_repos))
			need_copy = TRUE;
	} else {
		strncpy(file, pkg->filename, sizeof(file));
		need_copy = TRUE;
	}
	if (!apk_db_cache_active(db))
		need_copy = FALSE;

	bs = apk_bstream_from_fd_url(filefd, file);
	if (bs == NULL) {
		r = -errno;
		goto err_msg;
	}
	if (need_copy) {
		apk_blob_t b = APK_BLOB_BUF(tmpcacheitem);
		apk_blob_push_blob(&b, tmpprefix);
		apk_pkg_format_cache_pkg(b, pkg);
		cache_bs = apk_bstream_tee(bs, db->cache_fd, tmpcacheitem, NULL, NULL);
		if (cache_bs != NULL)
			bs = cache_bs;
		else
			apk_warning(PKG_VER_FMT": unable to cache: %s",
				    PKG_VER_PRINTF(pkg), apk_error_str(errno));
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
	r = apk_tar_parse(tar, apk_db_install_archive_entry, &ctx, TRUE, &db->id_cache);
	apk_sign_ctx_free(&ctx.sctx);
	tar->close(tar);

	if (need_copy) {
		if (r == 0) {
			renameat(db->cache_fd, tmpcacheitem, db->cache_fd, cacheitem);
			pkg->repos |= BIT(APK_REPOSITORY_CACHED);
		} else {
			unlinkat(db->cache_fd, tmpcacheitem, 0);
		}
	}
	if (r != 0)
		goto err_msg;

	apk_db_run_pending_script(&ctx);
	return 0;
err_msg:
	apk_error(PKG_VER_FMT": %s", PKG_VER_PRINTF(pkg), apk_error_str(r));
	return r;
}

int apk_db_install_pkg(struct apk_database *db, struct apk_package *oldpkg,
		       struct apk_package *newpkg, apk_progress_cb cb, void *cb_ctx)
{
	char *script_args[] = { NULL, NULL, NULL, NULL };
	struct apk_installed_package *ipkg;
	int r = 0;

	/* Upgrade script gets two args: <new-pkg> <old-pkg> */
	if (oldpkg != NULL && newpkg != NULL) {
		script_args[1] = apk_blob_cstr(*newpkg->version);
		script_args[2] = apk_blob_cstr(*oldpkg->version);
	} else {
		script_args[1] = apk_blob_cstr(*(oldpkg ? oldpkg->version : newpkg->version));
	}

	/* Just purging? */
	if (oldpkg != NULL && newpkg == NULL) {
		ipkg = oldpkg->ipkg;
		if (ipkg == NULL)
			goto ret_r;
		apk_ipkg_run_script(ipkg, db, APK_SCRIPT_PRE_DEINSTALL, script_args);
		apk_db_purge_pkg(db, ipkg, NULL);
		apk_ipkg_run_script(ipkg, db, APK_SCRIPT_POST_DEINSTALL, script_args);
		apk_pkg_uninstall(db, oldpkg);
		goto ret_r;
	}

	/* Install the new stuff */
	ipkg = apk_pkg_install(db, newpkg);
	ipkg->run_all_triggers = 1;
	ipkg->broken_script = 0;
	ipkg->broken_files = 0;
	if (ipkg->triggers->num != 0) {
		list_del(&ipkg->trigger_pkgs_list);
		list_init(&ipkg->trigger_pkgs_list);
		apk_string_array_free(&ipkg->triggers);
	}

	if (newpkg->installed_size != 0) {
		r = apk_db_unpack_pkg(db, ipkg, (oldpkg != NULL),
				      cb, cb_ctx, script_args);
		if (r != 0) {
			if (oldpkg != newpkg)
				apk_db_purge_pkg(db, ipkg, ".apk-new");
			apk_pkg_uninstall(db, newpkg);
			goto ret_r;
		}
		apk_db_migrate_files(db, ipkg);
	}

	if (oldpkg != NULL && oldpkg != newpkg && oldpkg->ipkg != NULL) {
		apk_db_purge_pkg(db, oldpkg->ipkg, NULL);
		apk_pkg_uninstall(db, oldpkg);
	}

	apk_ipkg_run_script(
		ipkg, db,
		(oldpkg == NULL) ? APK_SCRIPT_POST_INSTALL : APK_SCRIPT_POST_UPGRADE,
		script_args);

	if (ipkg->broken_files || ipkg->broken_script)
		r = -1;
ret_r:
	free(script_args[1]);
	free(script_args[2]);
	return r;
}

struct match_ctx {
	struct apk_database *db;
	struct apk_string_array *filter;
	unsigned int match;
	void (*cb)(struct apk_database *db, const char *match, struct apk_name *name, void *ctx);
	void *cb_ctx;
};

static int match_names(apk_hash_item item, void *pctx)
{
	struct match_ctx *ctx = (struct match_ctx *) pctx;
	struct apk_name *name = (struct apk_name *) item;
	unsigned int genid = ctx->match & APK_FOREACH_GENID_MASK;
	char **pmatch;

	if (genid) {
		if (name->foreach_genid >= genid)
			return 0;
		name->foreach_genid = genid;
	}

	if (ctx->filter->num == 0) {
		ctx->cb(ctx->db, NULL, name, ctx->cb_ctx);
		return 0;
	}

	foreach_array_item(pmatch, ctx->filter) {
		if (fnmatch(*pmatch, name->name, FNM_CASEFOLD) == 0) {
			ctx->cb(ctx->db, *pmatch, name, ctx->cb_ctx);
			if (genid)
				break;
		}
	}

	return 0;
}

void apk_name_foreach_matching(struct apk_database *db, struct apk_string_array *filter, unsigned int match,
			       void (*cb)(struct apk_database *db, const char *match, struct apk_name *name, void *ctx),
			       void *ctx)
{
	char **pmatch;
	unsigned int genid = match & APK_FOREACH_GENID_MASK;
	struct apk_name *name;
	struct match_ctx mctx = {
		.db = db,
		.filter = filter,
		.match = match,
		.cb = cb,
		.cb_ctx = ctx,
	};

	if (filter == NULL || filter->num == 0) {
		if (!(match & APK_FOREACH_NULL_MATCHES_ALL))
			return;
		apk_string_array_init(&mctx.filter);
		goto all;
	}
	foreach_array_item(pmatch, filter)
		if (strchr(*pmatch, '*') != NULL)
			goto all;

	foreach_array_item(pmatch, filter) {
		name = (struct apk_name *) apk_hash_get(&db->available.names, APK_BLOB_STR(*pmatch));
		if (name == NULL)
			continue;
		if (genid) {
			if (name->foreach_genid >= genid)
				continue;
			name->foreach_genid = genid;
		}
		cb(db, *pmatch, name, ctx);
	}
	return;

all:
	apk_hash_foreach(&db->available.names, match_names, &mctx);
}
