#include <string.h>

#include <openssl/opensslv.h>
#include <openssl/x509_vfy.h>

#ifndef LIBTLS_TLS_COMPAT_H
#define LIBTLS_TLS_COMPAT_H

#ifndef X509_V_FLAG_NO_CHECK_TIME
#define X509_V_FLAG_NO_CHECK_TIME 0
#endif

#ifndef SSL_OP_NO_CLIENT_RENEGOTIATION
#define SSL_OP_NO_CLIENT_RENEGOTIATION 0
#endif

int SSL_CTX_load_verify_mem(SSL_CTX *ctx, void *buf, int len);

int ASN1_time_parse(const char *bytes, size_t len, struct tm *tm, int mode);

int SSL_CTX_use_certificate_chain_mem(SSL_CTX *, char *buf, off_t);

#endif
