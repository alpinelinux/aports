/*
 * Copyright (c) 2016 Tai Chi Minh Ralph Eastwood <tcmreastwood@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <openssl/opensslv.h>
#include <openssl/x509_vfy.h>
#include <openssl/ssl.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>

int SSL_CTX_load_verify_mem(SSL_CTX *ctx, void *buf, int len)
{
    char fname[] = "/tmp/libtlscompatXXXXXX";
    int rc;
    int fd;

    fd = mkstemp(fname);

    if (fd < 0)
        return -1;
    do {
        ssize_t wrote = write(fd, buf, len);
        if(wrote == -1) {
            break;
        } else {
            buf = (char *)buf + wrote;
            len -= wrote;
        }
    } while(len);
    close(fd);
	rc = SSL_CTX_load_verify_locations(ctx, fname, NULL);
    remove(fname);
    return rc;
}

/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include <sys/types.h>

#include <unistd.h>
#include <stdio.h>

#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/objects.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>

int
SSL_CTX_use_certificate_chain_mem(SSL_CTX *ctx, char *buf, off_t len)
{
	int ret;
	BIO*in;
	X509*x;
	X509*ca;
	unsigned long err;

	ret = 0;
	x = ca = NULL;

	if ((in = BIO_new_mem_buf(buf, len)) == NULL) {
		SSLerr(SSL_F_SSL_CTX_USE_CERTIFICATE_CHAIN_FILE, ERR_R_BUF_LIB);
		goto end;
	}

	if ((x = PEM_read_bio_X509(in, NULL,
		    ctx->default_passwd_callback,
		    ctx->default_passwd_callback_userdata)) == NULL) {
		SSLerr(SSL_F_SSL_CTX_USE_CERTIFICATE_CHAIN_FILE, ERR_R_PEM_LIB);
		goto end;
	}

	if (!SSL_CTX_use_certificate(ctx, x) || ERR_peek_error() != 0)
		goto end;

	/* If we could set up our certificate, now proceed to
	 * the CA certificates.
	 */

	if (ctx->extra_certs != NULL) {
		sk_X509_pop_free(ctx->extra_certs, X509_free);
		ctx->extra_certs = NULL;
	}

	while ((ca = PEM_read_bio_X509(in, NULL,
		    ctx->default_passwd_callback,
		    ctx->default_passwd_callback_userdata)) != NULL) {

		if (!SSL_CTX_add_extra_chain_cert(ctx, ca))
			goto end;
	}

	err = ERR_peek_last_error();
	if (ERR_GET_LIB(err) == ERR_LIB_PEM &&
	    ERR_GET_REASON(err) == PEM_R_NO_START_LINE)
		ERR_clear_error();
	else
		goto end;

	ret = 1;
end:
	if (ca != NULL)
		X509_free(ca);
	if (x != NULL)
		X509_free(x);
	if (in != NULL)
		BIO_free(in);
	return (ret);
}

/*
 * Copyright (c) 2015 Bob Beck <beck@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Parse an RFC 5280 format ASN.1 time string.
 *
 * mode must be:
 * 0 if we expect to parse a time as specified in RFC 5280 from an X509 object.
 * V_ASN1_UTCTIME if we wish to parse on RFC5280 format UTC time.
 * V_ASN1_GENERALIZEDTIME if we wish to parse an RFC5280 format Generalized time.
 *
 * Returns:
 * -1 if the string was invalid.
 * V_ASN1_UTCTIME if the string validated as a UTC time string.
 * V_ASN1_GENERALIZEDTIME if the string validated as a Generalized time string.
 *
 * Fills in *tm with the corresponding time if tm is non NULL.
 */

#define GENTIME_LENGTH 15
#define UTCTIME_LENGTH 13

#define	ATOI2(ar)	((ar) += 2, ((ar)[-2] - '0') * 10 + ((ar)[-1] - '0'))
int
ASN1_time_parse(const char *bytes, size_t len, struct tm *tm, int mode)
{
	size_t i;
	int type = 0;
	struct tm ltm;
	struct tm *lt;
	const char *p;

	if (bytes == NULL)
		return (-1);

	/* Constrain to valid lengths. */
	if (len != UTCTIME_LENGTH && len != GENTIME_LENGTH)
		return (-1);

	lt = tm;
	if (lt == NULL) {
		memset(&ltm, 0, sizeof(ltm));
		lt = &ltm;
	}

	/* Timezone is required and must be GMT (Zulu). */
	if (bytes[len - 1] != 'Z')
		return (-1);

	/* Make sure everything else is digits. */
	for (i = 0; i < len - 1; i++) {
		if (isdigit((unsigned char)bytes[i]))
			continue;
		return (-1);
	}

	/*
	 * Validate and convert the time
	 */
	p = bytes;
	switch (len) {
	case GENTIME_LENGTH:
		if (mode == V_ASN1_UTCTIME)
			return (-1);
		lt->tm_year = (ATOI2(p) * 100) - 1900;	/* cc */
		type = V_ASN1_GENERALIZEDTIME;
		/* FALLTHROUGH */
	case UTCTIME_LENGTH:
		if (type == 0) {
			if (mode == V_ASN1_GENERALIZEDTIME)
				return (-1);
			type = V_ASN1_UTCTIME;
		}
		lt->tm_year += ATOI2(p);		/* yy */
		if (type == V_ASN1_UTCTIME) {
			if (lt->tm_year < 50)
				lt->tm_year += 100;
		}
		lt->tm_mon = ATOI2(p) - 1;		/* mm */
		if (lt->tm_mon < 0 || lt->tm_mon > 11)
			return (-1);
		lt->tm_mday = ATOI2(p);			/* dd */
		if (lt->tm_mday < 1 || lt->tm_mday > 31)
			return (-1);
		lt->tm_hour = ATOI2(p);			/* HH */
		if (lt->tm_hour < 0 || lt->tm_hour > 23)
			return (-1);
		lt->tm_min = ATOI2(p);			/* MM */
		if (lt->tm_min < 0 || lt->tm_min > 59)
			return (-1);
		lt->tm_sec = ATOI2(p);			/* SS */
		/* Leap second 60 is not accepted. Reconsider later? */
		if (lt->tm_sec < 0 || lt->tm_sec > 59)
			return (-1);
		break;
	default:
		return (-1);
	}

	return (type);
}

int
SSL_CTX_set1_groups(SSL_CTX *ctx, int *glist, int glistlen)
{
	return 1;
}

/* $OpenBSD: a_time_tm.c,v 1.14 2017/08/28 17:42:47 jsing Exp $ */
/*
 * Copyright (c) 2015 Bob Beck <beck@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <openssl/asn1t.h>
#include <openssl/err.h>

#define RFC5280 0
#define GENTIME_LENGTH 15
#define UTCTIME_LENGTH 13

int
ASN1_time_tm_cmp(struct tm *tm1, struct tm *tm2)
{
	if (tm1->tm_year < tm2->tm_year)
		return (-1);
	if (tm1->tm_year > tm2->tm_year)
		return (1);
	if (tm1->tm_mon < tm2->tm_mon)
		return (-1);
	if (tm1->tm_mon > tm2->tm_mon)
		return (1);
	if (tm1->tm_mday < tm2->tm_mday)
		return (-1);
	if (tm1->tm_mday > tm2->tm_mday)
		return (1);
	if (tm1->tm_hour < tm2->tm_hour)
		return (-1);
	if (tm1->tm_hour > tm2->tm_hour)
		return (1);
	if (tm1->tm_min < tm2->tm_min)
		return (-1);
	if (tm1->tm_min > tm2->tm_min)
		return (1);
	if (tm1->tm_sec < tm2->tm_sec)
		return (-1);
	if (tm1->tm_sec > tm2->tm_sec)
		return (1);
	return 0;
}

int
ASN1_time_tm_clamp_notafter(struct tm *tm)
{
	if (sizeof(time_t) < 8) {
		struct tm broken_os_epoch_tm;
		time_t broken_os_epoch_time = INT_MAX;

		if (gmtime_r(&broken_os_epoch_time, &broken_os_epoch_tm) == NULL)
			return 0;

		if (ASN1_time_tm_cmp(tm, &broken_os_epoch_tm) == 1)
			memcpy(tm, &broken_os_epoch_tm, sizeof(*tm));
	}

	return 1;
}
