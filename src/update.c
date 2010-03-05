/* update.c - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2005-2008 Natanael Copa <n@tanael.org>
 * Copyright (C) 2008 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#include <stdio.h>
#include "apk_defines.h"
#include "apk_applet.h"
#include "apk_database.h"
#include "apk_version.h"
#include "apk_print.h"

static int update_main(void *ctx, struct apk_database *db, int argc, char **argv)
{
	struct apk_repository *repo;
	int i;

	if (apk_verbosity < 1)
		return 0;

	for (i = 0; i < db->num_repos; i++) {
		repo = &db->repos[i];

		if (APK_BLOB_IS_NULL(repo->description))
			continue;

		apk_message("%.*s [%s]",
			    repo->description.len,
			    repo->description.ptr,
			    db->repos[i].url);
	}

	return 0;
}

static struct apk_applet apk_update = {
	.name = "update",
	.help = "Update repository indexes from all remote repositories.",
	.open_flags = APK_OPENF_WRITE,
	.forced_flags = APK_UPDATE_CACHE,
	.main = update_main,
};

APK_DEFINE_APPLET(apk_update);

