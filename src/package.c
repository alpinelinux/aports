/* package.c - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2005-2008 Natanael Copa <n@tanael.org>
 * Copyright (C) 2008 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#include <fcntl.h>
#include <ctype.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/wait.h>

#include "apk_defines.h"
#include "apk_archive.h"
#include "apk_package.h"
#include "apk_database.h"
#include "apk_state.h"

int apk_pkg_parse_name(apk_blob_t apkname,
		       apk_blob_t *name,
		       apk_blob_t *version)
{
	int i, dash = 0;

	for (i = apkname.len - 2; i >= 0; i--) {
		if (apkname.ptr[i] != '-')
			continue;
		if (isdigit(apkname.ptr[i+1]))
			break;
		if (++dash >= 2)
			return -1;
	}
	if (name != NULL)
		*name = APK_BLOB_PTR_LEN(apkname.ptr, i);
	if (version != NULL)
		*version = APK_BLOB_PTR_PTR(&apkname.ptr[i+1],
					    &apkname.ptr[apkname.len-1]);

	return 0;
}

static char *trim(apk_blob_t str)
{
	if (str.ptr == NULL || str.len < 1)
		return NULL;

	if (str.ptr[str.len-2] == '\n')
		str.ptr[str.len-2] = 0;

	return str.ptr;
}

static void parse_depend(struct apk_database *db,
			 struct apk_dependency_array **depends,
			 apk_blob_t blob)
{
	struct apk_dependency *dep;
	struct apk_name *name;
	char *cname;

	while (blob.len && blob.ptr[0] == ' ')
		blob.ptr++, blob.len--;
	while (blob.len && (blob.ptr[blob.len-1] == ' ' ||
			    blob.ptr[blob.len-1] == 0))
		blob.len--;

	if (blob.len == 0)
		return;

	cname = apk_blob_cstr(blob);
	name = apk_db_get_name(db, cname);
	free(cname);

	dep = apk_dependency_array_add(depends);
	*dep = (struct apk_dependency){
		.prefer_upgrade = 0,
		.version_mask = 0,
		.name = name,
		.version = NULL,
	};
}

int apk_deps_add(struct apk_dependency_array **depends,
		 struct apk_dependency *dep)
{
	struct apk_dependency_array *deps = *depends;
	int i;

	if (deps != NULL) {
		for (i = 0; i < deps->num; i++) {
			if (deps->item[i].name == dep->name)
				return 0;
		}
	}

	*apk_dependency_array_add(depends) = *dep;
	return 0;
}
void apk_deps_parse(struct apk_database *db,
		    struct apk_dependency_array **depends,
		    apk_blob_t blob)
{
	char *start;
	int i;

	start = blob.ptr;
	for (i = 0; i < blob.len; i++) {
		if (blob.ptr[i] != ',' && blob.ptr[i] != '\n')
			continue;

		parse_depend(db, depends,
			     APK_BLOB_PTR_PTR(start, &blob.ptr[i-1]));
		start = &blob.ptr[i+1];
	}
	parse_depend(db, depends,
		     APK_BLOB_PTR_PTR(start, &blob.ptr[i-1]));
}

int apk_deps_format(char *buf, int size,
		    struct apk_dependency_array *depends)
{
	int i, n = 0;

	if (depends == NULL)
		return 0;

	for (i = 0; i < depends->num - 1; i++)
		n += snprintf(&buf[n], size-n,
			      "%s, ",
			      depends->item[i].name->name);
	n += snprintf(&buf[n], size-n,
		      "%s\n",
		      depends->item[i].name->name);
	return n;
}

static const char *script_types[] = {
	[APK_SCRIPT_PRE_INSTALL]	= "pre-install",
	[APK_SCRIPT_POST_INSTALL]	= "post-install",
	[APK_SCRIPT_PRE_DEINSTALL]	= "pre-deinstall",
	[APK_SCRIPT_POST_DEINSTALL]	= "post-deinstall",
	[APK_SCRIPT_PRE_UPGRADE]	= "pre-upgrade",
	[APK_SCRIPT_POST_UPGRADE]	= "post-upgrade",
};

int apk_script_type(const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(script_types); i++)
		if (script_types[i] &&
		    strcmp(script_types[i], name) == 0)
			return i;

	return -1;
}

struct read_info_ctx {
	struct apk_database *db;
	struct apk_package *pkg;
	int has_install;
};

static int read_info_entry(struct apk_archive_entry *ae, void *ctx)
{
	struct read_info_ctx *ri = (struct read_info_ctx *) ctx;
	struct apk_database *db = ri->db;
	struct apk_package *pkg = ri->pkg;
	const int bsize = 4 * 1024;
	apk_blob_t name, version;
	char *slash, *str;

	if (strncmp(ae->name, "var/db/apk/", 11) != 0) {
		pkg->installed_size += (ae->size + bsize - 1) & ~(bsize - 1);
		return 0;
	}

	if (!S_ISREG(ae->mode))
		return 0;

	slash = strchr(&ae->name[11], '/');
	if (slash == NULL)
		return 0;

	if (apk_pkg_parse_name(APK_BLOB_PTR_PTR(&ae->name[11], slash-1),
			       &name, &version) < 0)
		return -1;

	if (pkg->name == NULL) {
		str = apk_blob_cstr(name);
		pkg->name = apk_db_get_name(db, str);
		free(str);
	}
	if (pkg->version == NULL)
		pkg->version = apk_blob_cstr(version);

	if (strcmp(slash, "/DEPEND") == 0) {
		apk_blob_t blob = apk_archive_entry_read(ae);
		if (blob.ptr) {
			apk_deps_parse(db, &pkg->depends, blob);
			free(blob.ptr);
		}
	} else if (strcmp(slash, "/DESC") == 0) {
		pkg->description = trim(apk_archive_entry_read(ae));
	} else if (strcmp(slash, "/WWW") == 0) {
		pkg->url = trim(apk_archive_entry_read(ae));
	} else if (strcmp(slash, "/LICENSE") == 0) {
		pkg->license = trim(apk_archive_entry_read(ae));
	} else if (apk_script_type(slash+1) == APK_SCRIPT_POST_INSTALL ||
		   apk_script_type(slash+1) == APK_SCRIPT_PRE_INSTALL)
		ri->has_install = 1;

	return 0;
}

struct apk_package *apk_pkg_read(struct apk_database *db, const char *file)
{
	struct read_info_ctx ctx;
	struct stat st;
	pthread_t tid;
	int fd;

	ctx.pkg = calloc(1, sizeof(struct apk_package));
	if (ctx.pkg == NULL)
		return NULL;

	fd = open(file, O_RDONLY);
	if (fd < 0)
		goto err;

	fstat(fd, &st);
	fcntl(fd, F_SETFD, FD_CLOEXEC);

	tid = apk_checksum_and_tee(&fd, ctx.pkg->csum);
	if (fd < 0)
		goto err;

	ctx.db = db;
	ctx.pkg->size = st.st_size;
	ctx.has_install = 0;
	if (apk_parse_tar_gz(fd, read_info_entry, &ctx) != 0) {
		pthread_join(tid, NULL);
		goto err;
	}
	pthread_join(tid, NULL);

	if (ctx.pkg->name == NULL)
		goto err;

	close(fd);

	/* Add implicit busybox dependency if there is scripts */
	if (ctx.has_install) {
		struct apk_dependency dep = {
			.name = apk_db_get_name(db, "busybox"),
		};
		apk_deps_add(&ctx.pkg->depends, &dep);
	}

	return ctx.pkg;
err:
	apk_pkg_free(ctx.pkg);
	return NULL;
}

void apk_pkg_free(struct apk_package *pkg)
{
	struct apk_script *script;
	struct hlist_node *c, *n;

	if (pkg == NULL)
		return;

	hlist_for_each_entry_safe(script, c, n, &pkg->scripts, script_list)
		free(script);

	if (pkg->version)
		free(pkg->version);
	if (pkg->url)
		free(pkg->url);
	if (pkg->description)
		free(pkg->description);
	if (pkg->license)
		free(pkg->license);
	free(pkg);
}

int apk_pkg_get_state(struct apk_package *pkg)
{
	if (hlist_hashed(&pkg->installed_pkgs_list))
		return APK_STATE_INSTALL;
	return APK_STATE_NO_INSTALL;
}

int apk_pkg_add_script(struct apk_package *pkg, int fd,
		       unsigned int type, unsigned int size)
{
	struct apk_script *script;
	int r;

	script = malloc(sizeof(struct apk_script) + size);
	script->type = type;
	script->size = size;
	r = read(fd, script->script, size);
	if (r < 0) {
		free(script);
		return r;
	}

	hlist_add_head(&script->script_list, &pkg->scripts);
	return r;
}

int apk_pkg_run_script(struct apk_package *pkg, int root_fd,
		       unsigned int type)
{
	static const char * const environment[] = {
		"PATH=/usr/sbin:/usr/bin:/sbin:/bin",
		NULL
	};
	struct apk_script *script;
	struct hlist_node *c;
	int fd, status;
	pid_t pid;
	char fn[1024];

	fchdir(root_fd);
	hlist_for_each_entry(script, c, &pkg->scripts, script_list) {
		if (script->type != type)
			continue;

		snprintf(fn, sizeof(fn),
			"tmp/%s-%s.%s",
			pkg->name->name, pkg->version,
			script_types[script->type]);
		fd = creat(fn, 0777);
		if (fd < 0)
			return fd;
		write(fd, script->script, script->size);
		close(fd);

		apk_message("Executing %s", &fn[4]);

		pid = fork();
		if (pid == -1)
			return -1;
		if (pid == 0) {
			chroot(".");
			execle(fn, script_types[script->type],
			       pkg->version, "", NULL, environment);
			exit(1);
		}
		waitpid(pid, &status, 0);
		//unlink(fn);
		if (WIFEXITED(status))
			return WEXITSTATUS(status);
		return -1;
	}

	return 0;
}

static int parse_index_line(struct apk_database *db, struct apk_package *pkg,
			    apk_blob_t blob)
{
	apk_blob_t d;
	char *str;

	if (blob.len < 2 || blob.ptr[1] != ':')
		return -1;

	d = APK_BLOB_PTR_LEN(blob.ptr+2, blob.len-2);
	switch (blob.ptr[0]) {
	case 'P':
		str = apk_blob_cstr(d);
		pkg->name = apk_db_get_name(db, str);
		free(str);
		break;
	case 'V':
		pkg->version = apk_blob_cstr(d);
		break;
	case 'T':
		pkg->description = apk_blob_cstr(d);
		break;
	case 'U':
		pkg->url = apk_blob_cstr(d);
		break;
	case 'L':
		pkg->license = apk_blob_cstr(d);
		break;
	case 'D':
		apk_deps_parse(db, &pkg->depends, d);
		break;
	case 'C':
		apk_hexdump_parse(APK_BLOB_BUF(pkg->csum), d);
		break;
	case 'S':
		pkg->size = apk_blob_uint(d, 10);
		break;
	case 'I':
		pkg->installed_size = apk_blob_uint(d, 10);
		break;
	}
	return 0;
}

struct apk_package *apk_pkg_parse_index_entry(struct apk_database *db, apk_blob_t blob)
{
	struct apk_package *pkg;
	apk_blob_t l, r;

	pkg = calloc(1, sizeof(struct apk_package));
	if (pkg == NULL)
		return NULL;

	r = blob;
	while (apk_blob_splitstr(r, "\n", &l, &r))
		parse_index_line(db, pkg, l);
	parse_index_line(db, pkg, r);

	if (pkg->name == NULL) {
		apk_pkg_free(pkg);
		printf("%.*s\n", blob.len, blob.ptr);
		pkg = NULL;
	}

	return pkg;
}

apk_blob_t apk_pkg_format_index_entry(struct apk_package *info, int size,
				      char *buf)
{
	int n = 0;

	n += snprintf(&buf[n], size-n,
		      "P:%s\n"
		      "V:%s\n"
		      "S:%u\n"
		      "I:%u\n"
		      "T:%s\n"
		      "U:%s\n"
		      "L:%s\n",
		      info->name->name, info->version,
		      info->size, info->installed_size,
		      info->description, info->url, info->license);

	if (info->depends != NULL) {
		n += snprintf(&buf[n], size-n, "D:");
		n += apk_deps_format(&buf[n], size-n, info->depends);
	}
	n += snprintf(&buf[n], size-n, "C:");
	n += apk_hexdump_format(size-n, &buf[n],
				APK_BLOB_BUF(info->csum));
	n += snprintf(&buf[n], size-n,
		      "\n\n");

	return APK_BLOB_PTR_LEN(buf, n);
}
