/* policy.c - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2013 Timo Teräs <timo.teras@iki.fi>
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

extern const char * const apk_installed_file;

static void print_policy(struct apk_database *db, const char *match, struct apk_name *name, void *ctx)
{
	struct apk_provider *p;
	struct apk_repository *repo;
	int i, j, num = 0;

	if (!name) return;

/*
zlib1g policy:
  2.0:
    @testing http://nl.alpinelinux.org/alpine/edge/testing
  1.7:
    @edge http://nl.alpinelinux.org/alpine/edge/main
  1.2.3.5 (upgradeable):
    http://nl.alpinelinux.org/alpine/v2.6/main
  1.2.3.4 (installed):
    /media/cdrom/...
    http://nl.alpinelinux.org/alpine/v2.5/main
  1.1:
    http://nl.alpinelinux.org/alpine/v2.4/main
*/
	foreach_array_item(p, name->providers) {
		if (p->pkg->name != name)
			continue;
		if (num++ == 0)
			printf("%s policy:\n", name->name);
		printf("  " BLOB_FMT ":\n", BLOB_PRINTF(*p->version));
		if (p->pkg->ipkg != NULL)
			printf("    %s\n", apk_installed_file);
		for (i = 0; i < db->num_repos; i++) {
			repo = &db->repos[i];
			if (!(BIT(i) & p->pkg->repos))
				continue;
			for (j = 0; j < db->num_repo_tags; j++) {
				if (db->repo_tags[j].allowed_repos & p->pkg->repos)
					printf("    "BLOB_FMT"%s%s\n",
						BLOB_PRINTF(db->repo_tags[j].tag),
						j == 0 ? "" : " ",
						repo->url);
			}
		}
	}
}

static int policy_main(void *ctx, struct apk_database *db, struct apk_string_array *args)
{
	apk_name_foreach_matching(db, args, apk_foreach_genid(), print_policy, NULL);
	return 0;
}

static struct apk_applet apk_policy = {
	.name = "policy",
	.help = "Show repository policy for packages",
	.open_flags = APK_OPENF_READ,
	.command_groups = APK_COMMAND_GROUP_QUERY,
	.main = policy_main,
};

APK_DEFINE_APPLET(apk_policy);


