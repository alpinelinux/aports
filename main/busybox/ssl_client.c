#include <err.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>

#define BUFSIZE 16384

#define TLS_DEBUG 0

#if TLS_DEBUG
# define dbg(...) fprintf(stderr, __VA_ARGS__)
#else
# define dbg(...) ((void)0)
#endif

static void ssl_fatal(const char *msg)
{
	ERR_print_errors_fp(stderr);
	errx(1, "%s", msg);
}

static void copy_from_stdin_to_tls(SSL *ssl, int *fd)
{
	static size_t buf[BUFSIZE];
	ssize_t n;
	int i = 0;
	dbg("DEBUG: data from STDIN\n");
	do {
		n = read(STDIN_FILENO, buf, sizeof(buf));
		dbg("read %zu\n", n);
	} while (n < 0 && errno == EINTR);

	if (n < 1) {
		*fd = -1;
		return;
	}

	while (n > 0) {
		ssize_t r = SSL_write(ssl, &buf[i], n);
		if (r < 0) {
			if (SSL_get_error(ssl, r) == SSL_ERROR_WANT_WRITE) {
				ERR_clear_error();
				continue;
			}
			ssl_fatal("SSL_write");
		}
		i += r;
		n -= r;
	}
}

static int should_retry_read(SSL *ssl, int n)
{
	if (n >= 0 || SSL_get_error(ssl, n) != SSL_ERROR_WANT_READ)
		return 0;
	ERR_clear_error();
	return 1;
}

static int copy_from_tls_to_stdout(SSL *ssl)
{
	static size_t buf[BUFSIZE];
	ssize_t n,r;
	int i = 0;

	dbg("DEBUG: data from TLS\n");
	do {
		n = SSL_read(ssl, buf, sizeof(buf));

	} while (should_retry_read(ssl, n));
	if (n < 0)
		ssl_fatal("SSL_read");

	if (n == 0)
		return 1;

	while (n) {
		r = write(STDOUT_FILENO, &buf[i], n);
		if (r < 0)
			err(1, "write");
		i += r;
		n -= r;
	}
	return 0;
}

void do_poll(struct pollfd *fds, int nfds)
{
	while (poll(fds, nfds, -1) < 0) {
		if (errno != EINTR && errno != ENOMEM)
			err(1, "poll");
	}
}

static void copy_loop(SSL *ssl, int sfd)
{
	struct pollfd fds[2] = {
		{ .fd = STDIN_FILENO,	.events = POLLIN },
		{ .fd = sfd,		.events = POLLIN },
	};

	while (1) {
		do_poll(fds, 2);
		if (fds[0].revents) {
			copy_from_stdin_to_tls(ssl, &fds[0].fd);
		}

		if (fds[1].revents && copy_from_tls_to_stdout(ssl))
			break;
	}
}

void usage(const char *prog, int ret) {
	printf("usage: %s [-s FD] [-I] -n SNI\n", prog);
	exit(ret);
}

int main(int argc, char *argv[])
{
	int c, sfd = 1;;
	const char *sni = NULL;
	int insecure = 0;
	SSL_CTX *ctx;
	SSL *ssl = NULL;

	while ((c = getopt(argc, argv, "hs:n:I")) != -1) {
		switch (c) {
		case 'h':
			usage(argv[0], 0);
			break;
		case 's':
			sfd = atoi(optarg);
			break;
		case 'n':
			sni = optarg;
			break;
		case 'I':
			insecure = 1;
			break;
		case '?':
			usage(argv[0], 1);
		}
	}

	OPENSSL_init_ssl(0, NULL);

	if ((ctx = SSL_CTX_new(TLS_client_method())) == NULL)
		ssl_fatal("SSL_CTX_new");

	SSL_CTX_set_default_verify_paths(ctx);

	if ((ssl = SSL_new(ctx)) == NULL)
		ssl_fatal("SSL_new");

	SSL_set_fd(ssl, sfd);
	SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);
	SSL_set_hostflags(ssl, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);

	if (SSL_set_tlsext_host_name(ssl, sni) != 1)
		ssl_fatal("SSL_set_tlsext_host_name");

	if (SSL_set1_host(ssl, sni) != 1)
		ssl_fatal(sni);

	if (!insecure) {
		SSL_set_verify(ssl, SSL_VERIFY_PEER, NULL);
	}

	ERR_clear_error();
	if (SSL_connect(ssl) != 1)
		ssl_fatal("SSL_connect");

	copy_loop(ssl, sfd);

	SSL_CTX_free(ctx);
	return 0;
}
