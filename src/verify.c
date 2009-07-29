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
	struct apk_sign_ctx sctx;
	struct apk_istream *is;
	int i, r, ok, rc = 0;

	apk_flags |= APK_ALLOW_UNTRUSTED;
	for (i = 0; i < argc; i++) {
		apk_sign_ctx_init(&sctx, APK_SIGN_VERIFY, NULL);
		is = apk_bstream_gunzip_mpart(apk_bstream_from_file(argv[i]),
					      apk_sign_ctx_mpart_cb, &sctx);
		r = apk_tar_parse(is, apk_sign_ctx_verify_tar, &sctx, FALSE);
		is->close(is);
		ok = sctx.control_verified && sctx.data_verified;
		if (apk_verbosity >= 1)
			apk_message("%s: %d - %s", argv[i], r,
				ok ? "OK" :
				sctx.data_verified ? "UNTRUSTED" : "FAILED");
		if (!ok)
			rc++;
		apk_sign_ctx_free(&sctx);
	}

	return rc;
}

static struct apk_applet apk_verify = {
	.name = "verify",
	.help = "Verify package integrity and signature",
	.arguments = "FILE...",
	.main = verify_main,
};

APK_DEFINE_APPLET(apk_verify);

