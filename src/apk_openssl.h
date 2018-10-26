#ifndef APK_SSL_COMPAT_H
#define APK_SSL_COMPAT_H

#include <openssl/opensslv.h>
#include <openssl/evp.h>

#if OPENSSL_VERSION_NUMBER < 0x1010000fL || (defined(LIBRESSL_VERSION_NUMBER) && LIBRESSL_VERSION_NUMBER < 0x2070000fL)

static inline EVP_MD_CTX *EVP_MD_CTX_new(void)
{
	return EVP_MD_CTX_create();
}

static inline void EVP_MD_CTX_free(EVP_MD_CTX *mdctx)
{
	return EVP_MD_CTX_destroy(mdctx);
}

#endif

#endif
