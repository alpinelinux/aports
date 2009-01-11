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

#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#include "apk_defines.h"
#include "apk_archive.h"
#include "apk_package.h"
#include "apk_database.h"
#include "apk_state.h"

struct apk_package *apk_pkg_new(void)
{
	struct apk_package *pkg;

	pkg = calloc(1, sizeof(struct apk_package));
	if (pkg != NULL)
		list_init(&pkg->installed_pkgs_list);

	return pkg;
}

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

static apk_blob_t trim(apk_blob_t str)
{
	if (str.ptr == NULL || str.len < 1)
		return str;

	if (str.ptr[str.len-1] == '\n') {
		str.ptr[str.len-1] = 0;
		return APK_BLOB_PTR_LEN(str.ptr, str.len-1);
	}

	return str;
}

static void parse_depend(struct apk_database *db,
			 struct apk_dependency_array **depends,
			 apk_blob_t blob)
{
	struct apk_dependency *dep;
	struct apk_name *name;

	while (blob.len && blob.ptr[0] == ' ')
		blob.ptr++, blob.len--;
	while (blob.len && (blob.ptr[blob.len-1] == ' ' ||
			    blob.ptr[blob.len-1] == 0))
		blob.len--;

	if (blob.len == 0)
		return;

	name = apk_db_get_name(db, blob);
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
	for (i = 0; i < blob.len && blob.ptr[i] != '\n'; i++) {
		if (blob.ptr[i] != ' ')
			continue;

		parse_depend(db, depends,
			     APK_BLOB_PTR_PTR(start, &blob.ptr[i-1]));
		start = &blob.ptr[i];
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
			      "%s ",
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

static const char *script_types2[] = {
	[APK_SCRIPT_PRE_INSTALL]	= "pre_install",
	[APK_SCRIPT_POST_INSTALL]	= "post_install",
	[APK_SCRIPT_PRE_DEINSTALL]	= "pre_deinstall",
	[APK_SCRIPT_POST_DEINSTALL]	= "post_deinstall",
	[APK_SCRIPT_PRE_UPGRADE]	= "pre_upgrade",
	[APK_SCRIPT_POST_UPGRADE]	= "post_upgrade",
};

int apk_script_type(const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(script_types); i++)
		if (script_types[i] &&
		    strcmp(script_types[i], name) == 0)
			return i;

	return APK_SCRIPT_INVALID;
}

struct read_info_ctx {
	struct apk_database *db;
	struct apk_package *pkg;
	int has_install;
};

int apk_pkg_add_info(struct apk_database *db, struct apk_package *pkg,
		     char field, apk_blob_t value)
{
	switch (field) {
	case 'P':
		pkg->name = apk_db_get_name(db, value);
		break;
	case 'V':
		pkg->version = apk_blob_cstr(value);
		break;
	case 'T':
		pkg->description = apk_blob_cstr(value);
		break;
	case 'U':
		pkg->url = apk_blob_cstr(value);
		break;
	case 'L':
		pkg->license = apk_blob_cstr(value);
		break;
	case 'D':
		apk_deps_parse(db, &pkg->depends, value);
		break;
	case 'C':
		apk_hexdump_parse(APK_BLOB_BUF(pkg->csum), value);
		break;
	case 'S':
		pkg->size = apk_blob_uint(value, 10);
		break;
	case 'I':
		pkg->installed_size = apk_blob_uint(value, 10);
		break;
	default:
		return -1;
	}
	return 0;
}

static int read_info_line(void *ctx, apk_blob_t line)
{
	static struct {
		const char *str;
		char field;
	} fields[] = {
		{ "pkgname", 'P' },
		{ "pkgver",  'V' },
		{ "pkgdesc", 'T' },
		{ "url",     'U' },
		{ "size",    'I' },
		{ "license", 'L' },
		{ "depend",  'D' },
	};
	struct read_info_ctx *ri = (struct read_info_ctx *) ctx;
	apk_blob_t l, r;
	int i;

	if (line.ptr == NULL || line.len < 1 || line.ptr[0] == '#')
		return 0;

	if (!apk_blob_splitstr(line, " = ", &l, &r))
		return 0;

	for (i = 0; i < ARRAY_SIZE(fields); i++) {
		if (strncmp(fields[i].str, l.ptr, l.len) == 0) {
			apk_pkg_add_info(ri->db, ri->pkg, fields[i].field, r);
			break;
		}
	}

	return 0;
}

static int read_info_entry(void *ctx, const struct apk_file_info *ae,
			   struct apk_istream *is)
{
	static struct {
		const char *str;
		char field;
	} fields[] = {
		{ "DESC",	'T' },
		{ "WW",		'U' },
		{ "LICENSE",	'L' },
		{ "DEPEND", 	'D' },
	};
	struct read_info_ctx *ri = (struct read_info_ctx *) ctx;
	struct apk_database *db = ri->db;
	struct apk_package *pkg = ri->pkg;
	apk_blob_t name, version;
	char *slash;
	int i;

	/* Meta info and scripts */
	if (ae->name[0] == '.') {
		/* APK 2.0 format */
		if (strcmp(ae->name, ".PKGINFO") == 0) {
			apk_blob_t blob = apk_blob_from_istream(is, ae->size);
			apk_blob_for_each_segment(blob, "\n", read_info_line, ctx);
			free(blob.ptr);
			return 1;
		}
		/* FIXME: Check for .INSTALL */
	} else if (strncmp(ae->name, "var/db/apk/", 11) == 0) {
		/* APK 1.0 format */
		if (!S_ISREG(ae->mode))
			return 0;

		slash = strchr(&ae->name[11], '/');
		if (slash == NULL)
			return 0;

		if (apk_pkg_parse_name(APK_BLOB_PTR_PTR(&ae->name[11], slash-1),
				       &name, &version) < 0)
			return -1;

		if (pkg->name == NULL)
			pkg->name = apk_db_get_name(db, name);
		if (pkg->version == NULL)
			pkg->version = apk_blob_cstr(version);

		for (i = 0; i < ARRAY_SIZE(fields); i++) {
			if (strcmp(fields[i].str, slash+1) == 0) {
				apk_blob_t blob = apk_blob_from_istream(is, ae->size);
				apk_pkg_add_info(ri->db, ri->pkg, fields[i].field,
						 trim(blob));
				free(blob.ptr);
				break;
			}
		}
		if (apk_script_type(slash+1) == APK_SCRIPT_POST_INSTALL ||
		    apk_script_type(slash+1) == APK_SCRIPT_PRE_INSTALL)
			ri->has_install = 1;
	} else {
		pkg->installed_size += apk_calc_installed_size(ae->size);
	}

	return 0;
}

struct apk_package *apk_pkg_read(struct apk_database *db, const char *file)
{
	struct read_info_ctx ctx;
	struct apk_bstream *bs;

	ctx.pkg = apk_pkg_new();
	if (ctx.pkg == NULL)
		return NULL;

	bs = apk_bstream_from_file(file);
	if (bs == NULL)
		goto err;

	ctx.db = db;
	ctx.has_install = 0;
	if (apk_parse_tar_gz(bs, read_info_entry, &ctx) < 0) {
		apk_error("File %s is not an APK archive", file);
		bs->close(bs, NULL, NULL);
		goto err;
	}
	bs->close(bs, ctx.pkg->csum, &ctx.pkg->size);

	if (ctx.pkg->name == NULL) {
		apk_error("File %s is corrupted", file);
		goto err;
	}

	/* Add implicit busybox dependency if there is scripts */
	if (ctx.has_install) {
		struct apk_dependency dep = {
			.name = apk_db_get_name(db, APK_BLOB_STR("busybox")),
		};
		apk_deps_add(&ctx.pkg->depends, &dep);
	}
	ctx.pkg->filename = strdup(file);

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

	if (pkg->depends)
		free(pkg->depends);
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
	if (list_hashed(&pkg->installed_pkgs_list))
		return APK_STATE_INSTALL;
	return APK_STATE_NO_INSTALL;
}

void apk_pkg_set_state(struct apk_database *db, struct apk_package *pkg, int state)
{
	switch (state) {
	case APK_STATE_INSTALL:
		if (!list_hashed(&pkg->installed_pkgs_list)) {
			db->installed.stats.packages++;
			list_add_tail(&pkg->installed_pkgs_list,
				      &db->installed.packages);
		}
		break;
	case APK_STATE_NO_INSTALL:
		if (list_hashed(&pkg->installed_pkgs_list)) {
			db->installed.stats.packages--;
			list_del(&pkg->installed_pkgs_list);
		}
		break;
	}
}

int apk_pkg_add_script(struct apk_package *pkg, struct apk_istream *is,
		       unsigned int type, unsigned int size)
{
	struct apk_script *script;
	int r;

	script = malloc(sizeof(struct apk_script) + size);
	script->type = type;
	script->size = size;
	r = is->read(is, script->script, size);
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
		if (script->type != type &&
		    script->type != APK_SCRIPT_GENERIC)
			continue;

		snprintf(fn, sizeof(fn),
			"tmp/%s-%s.%s",
			pkg->name->name, pkg->version,
			script_types[type]);
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
			if (chroot(".") < 0) {
				apk_error("chroot: %s", strerror(errno));
			} else if (script->type == APK_SCRIPT_GENERIC) {
				execle(fn, "INSTALL", script_types2[type],
				       pkg->version, "", NULL, environment);
			} else {
				execle(fn, script_types[type],
				       pkg->version, "", NULL, environment);
			}
			exit(1);
		}
		waitpid(pid, &status, 0);
		unlink(fn);
		if (WIFEXITED(status))
			return WEXITSTATUS(status);
		return -1;
	}

	return 0;
}

static int parse_index_line(void *ctx, apk_blob_t line)
{
	struct read_info_ctx *ri = (struct read_info_ctx *) ctx;

	if (line.len < 3 || line.ptr[1] != ':')
		return 0;

	apk_pkg_add_info(ri->db, ri->pkg, line.ptr[0], APK_BLOB_PTR_LEN(line.ptr+2, line.len-2));
	return 0;
}

struct apk_package *apk_pkg_parse_index_entry(struct apk_database *db, apk_blob_t blob)
{
	struct read_info_ctx ctx;

	ctx.pkg = apk_pkg_new();
	if (ctx.pkg == NULL)
		return NULL;

	ctx.db = db;
	ctx.has_install = 0;

	apk_blob_for_each_segment(blob, "\n", parse_index_line, &ctx);

	if (ctx.pkg->name == NULL) {
		apk_pkg_free(ctx.pkg);
		apk_error("Failed to parse index entry: %.*s",
			  blob.len, blob.ptr);
		ctx.pkg = NULL;
	}

	return ctx.pkg;
}

apk_blob_t apk_pkg_format_index_entry(struct apk_package *info, int size,
				      char *buf)
{
	int n = 0;

	n += snprintf(&buf[n], size-n,
		      "P:%s\n"
		      "V:%s\n"
		      "S:%zu\n"
		      "I:%zu\n"
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
