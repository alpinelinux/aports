/* verify.c - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2009 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation. See http://www.gnu.org/ for details.
 */

#include <stdio.h>
#include <unistd.h>

#include "apk_applet.h"
#include "apk_database.h"

static int verify_main(void *ctx, int argc, char **argv)
{
	struct apk_database db;
	struct apk_sign_ctx sctx;
	int i, ok, rc = 0;

	apk_db_open(&db, NULL, APK_OPENF_NO_STATE);
	for (i = 0; i < argc; i++) {
		apk_sign_ctx_init(&sctx, APK_SIGN_VERIFY);
		apk_pkg_read(&db, argv[i], &sctx);
		ok = sctx.control_verified && sctx.data_verified;
		if (apk_verbosity >= 1)
			apk_message("%s: %s", argv[i],
				ok ? "OK" :
				sctx.data_verified ? "UNTRUSTED" : "FAILED");
		if (!ok)
			rc++;
		apk_sign_ctx_free(&sctx);
	}
	apk_db_close(&db);

	return rc;
}

static struct apk_applet apk_verify = {
	.name = "verify",
	.help = "Verify package integrity and signature",
	.arguments = "FILE...",
	.main = verify_main,
};

APK_DEFINE_APPLET(apk_verify);

