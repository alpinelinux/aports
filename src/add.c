/* add.c - Alpine Package Keeper (APK)
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
#include <zlib.h>
#include "apk_applet.h"
#include "apk_database.h"
#include "apk_state.h"

struct add_ctx {
	unsigned int open_flags;
	const char *virtpkg;
};

static int add_parse(void *ctx, int optch, int optindex, const char *optarg)
{
	struct add_ctx *actx = (struct add_ctx *) ctx;

	switch (optch) {
	case 0x10000:
		actx->open_flags |= APK_OPENF_CREATE;
		break;
	case 'u':
		apk_flags |= APK_UPGRADE;
		break;
	case 't':
		actx->virtpkg = optarg;
		break;
	default:
		return -1;
	}
	return 0;
}

static int cup(void)
{
	/* compressed/uncompressed size is 259/1213 */
	static unsigned char z[] = {
		0x78,0x9c,0x9d,0x94,0x3d,0x8e,0xc4,0x20,0x0c,0x85,0xfb,0x9c,
		0xc2,0x72,0x43,0x46,0x8a,0x4d,0x3f,0x67,0x89,0x64,0x77,0x2b,
		0x6d,0xbb,0x6d,0x0e,0x3f,0xc6,0x84,0x4d,0x08,0x84,0x55,0xd6,
		0xa2,0xe0,0xef,0x7b,0x36,0xe1,0x11,0x80,0x6e,0xcc,0x53,0x7f,
		0x3e,0xc5,0xeb,0xcf,0x1d,0x20,0x22,0xcc,0x3c,0x53,0x8e,0x17,
		0xd9,0x80,0x6d,0xee,0x0e,0x61,0x42,0x3c,0x8b,0xcf,0xc7,0x12,
		0x22,0x71,0x8b,0x31,0x05,0xd5,0xb0,0x11,0x4b,0xa7,0x32,0x2f,
		0x80,0x69,0x6b,0xb0,0x98,0x40,0xe2,0xcd,0xba,0x6a,0xba,0xe4,
		0x65,0xed,0x61,0x23,0x44,0xb5,0x95,0x06,0x8b,0xde,0x6c,0x61,
		0x70,0xde,0x0e,0xb6,0xed,0xc4,0x43,0x0c,0x56,0x6f,0x8f,0x31,
		0xd0,0x35,0xb5,0xc7,0x58,0x06,0xff,0x81,0x49,0x84,0xb8,0x0e,
		0xb1,0xd8,0xc1,0x66,0x31,0x0e,0x46,0x5c,0x43,0xc9,0xef,0xe5,
		0xdc,0x63,0xb1,0xdc,0x67,0x6d,0x31,0xb3,0xc9,0x69,0x74,0x87,
		0xc7,0xa3,0x1b,0x6a,0xb3,0xbd,0x2f,0x3b,0xd5,0x0c,0x57,0x3b,
		0xce,0x7c,0x5e,0xe5,0x48,0xd0,0x48,0x01,0x92,0x49,0x8b,0xf7,
		0xfc,0x58,0x67,0xb3,0xf7,0x14,0x20,0x5c,0x4c,0x9e,0xcc,0xeb,
		0x78,0x7e,0x64,0xa6,0xa1,0xf5,0xb2,0x70,0x38,0x09,0x7c,0x7f,
		0xfd,0xc0,0x8a,0x4e,0xc8,0x55,0xe8,0x12,0xe2,0x9f,0x1a,0xb1,
		0xb9,0x82,0x52,0x02,0x7a,0xe5,0xf9,0xd9,0x88,0x47,0x79,0x3b,
		0x46,0x61,0x27,0xf9,0x51,0xb1,0x17,0xb0,0x2c,0x0e,0xd5,0x39,
		0x2d,0x96,0x25,0x27,0xd6,0xd1,0x3f,0xa5,0x08,0xe1,0x9e,0x4e,
		0xa7,0xe9,0x03,0xb1,0x0a,0xb6,0x75
	};
	unsigned char buf[1213];
	unsigned long len = sizeof(buf);

	uncompress(buf, &len, z, sizeof(z));
	write(STDOUT_FILENO, buf, len);

	return 0;
}


static int add_main(void *ctx, int argc, char **argv)
{
	struct add_ctx *actx = (struct add_ctx *) ctx;
	struct apk_database db;
	struct apk_state *state = NULL;
	struct apk_dependency_array *pkgs = NULL;
	struct apk_package *virtpkg = NULL;
	struct apk_dependency virtdep;
	int i, r;

	if ((argc > 0) && (apk_verbosity > 1) &&
	    (strcmp(argv[0], "coffee") == 0))
		return cup();

	r = apk_db_open(&db, apk_root, actx->open_flags | APK_OPENF_WRITE);
	if (r != 0)
		return r;

	if (actx->virtpkg) {
		virtpkg = apk_pkg_new();
		if (virtpkg == NULL) {
			apk_error("Failed to allocate virtual meta package");
			goto err;
		}
		virtpkg->name = apk_db_get_name(&db, APK_BLOB_STR(actx->virtpkg));
		apk_blob_checksum(APK_BLOB_STR(virtpkg->name->name),
				  apk_default_checksum(), &virtpkg->csum);
		virtpkg->version = strdup("0");
		virtpkg->description = strdup("virtual meta package");
		virtdep = apk_dep_from_pkg(&db, virtpkg);
		virtdep.name->flags |= APK_NAME_TOPLEVEL | APK_NAME_VIRTUAL;
		virtpkg = apk_db_pkg_add(&db, virtpkg);
	}

	for (i = 0; i < argc; i++) {
		struct apk_dependency dep;

		if (strstr(argv[i], ".apk") != NULL) {
			struct apk_package *pkg;
			struct apk_sign_ctx sctx;

			apk_sign_ctx_init(&sctx, APK_SIGN_VERIFY, NULL);
			pkg = apk_pkg_read(&db, argv[i], &sctx);
			apk_sign_ctx_free(&sctx);
			if (pkg == NULL) {
				apk_error("Unable to read '%s'", argv[i]);
				goto err;
			}

			dep = apk_dep_from_pkg(&db, pkg);
		} else
			dep = apk_dep_from_str(&db, argv[i]);

		if (virtpkg) {
			apk_deps_add(&virtpkg->depends, &dep);
		} else {
			dep.name->flags |= APK_NAME_TOPLEVEL;
		}
		apk_deps_add(&pkgs, &dep);
	}

	if (virtpkg) {
		apk_deps_add(&pkgs, &virtdep);
		apk_deps_add(&db.world, &virtdep);
	}

	state = apk_state_new(&db);
	for (i = 0; (pkgs != NULL) && i < pkgs->num; i++) {
		r = apk_state_lock_dependency(state, &pkgs->item[i]);
		if (r != 0) {
			apk_error("Unable to install '%s'", pkgs->item[i].name->name);
			if (!(apk_flags & APK_FORCE))
				goto err;
		}
		if (!virtpkg)
			apk_deps_add(&db.world, &pkgs->item[i]);
	}
	r = apk_state_commit(state, &db);
err:
	if (state != NULL)
		apk_state_unref(state);
	apk_db_close(&db);
	return r;
}

static struct apk_option add_options[] = {
	{ 0x10000,	"initdb",	"Initialize database" },
	{ 'u',		"upgrade",	"Prefer to upgrade package" },
	{ 't',		"virtual",
	  "Instead of adding all the packages to 'world', create a new virtual "
	  "package with the listed dependencies and add that to 'world'. The "
	  "actions of the command are easily reverted by deleting the virtual "
	  "package.", required_argument, "NAME" },
};

static struct apk_applet apk_add = {
	.name = "add",
	.help = "Add (or update) PACKAGEs to main dependencies and install "
		"them, while ensuring that all dependencies are met.",
	.arguments = "PACKAGE...",
	.context_size = sizeof(struct add_ctx),
	.num_options = ARRAY_SIZE(add_options),
	.options = add_options,
	.parse = add_parse,
	.main = add_main,
};

APK_DEFINE_APPLET(apk_add);

