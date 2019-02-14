#include <err.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <tls.h>

#define BUFSIZE 16384

#define TLS_DEBUG 0

#if TLS_DEBUG
# define dbg(...) fprintf(stderr, __VA_ARGS__)
#else
# define dbg(...) ((void)0)
#endif

static void copy_from_stdin_to_tls(struct tls *ctx, int *fd)
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
		ssize_t r = tls_write(ctx, &buf[i], n);
		if (r == TLS_WANT_POLLIN || r == TLS_WANT_POLLOUT)
			continue;
		if (r < 0)
			err(1, "tls_write: %s", tls_error(ctx));
		i += r;
		n -= r;
	}
}

static int copy_from_tls_to_stdout(struct tls *ctx)
{
	static size_t buf[BUFSIZE];
	ssize_t n,r;
	int i = 0;

	dbg("DEBUG: data from TLS\n");
	do {
		n = tls_read(ctx, buf, sizeof(buf));
	} while (n == TLS_WANT_POLLIN || r == TLS_WANT_POLLOUT);
	if (n < 0)
		err(1, "tls read: %s", tls_error(ctx));

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

int do_poll(struct pollfd *fds, int nfds)
{
	int r;
	while ((r = poll(fds, nfds, -1)) < 0) {
		if (errno != EINTR && errno != ENOMEM)
			err(1, "poll");
	}
	return r;
}

static void copy_loop(struct tls *ctx, int sfd, int eofexit)
{
	struct pollfd fds[2] = {
		{ .fd = STDIN_FILENO,	.events = POLLIN },
		{ .fd = sfd,		.events = POLLIN },
	};

	while (1) {
		int r = do_poll(fds, 2);
		if (fds[0].revents) {
			copy_from_stdin_to_tls(ctx, &fds[0].fd);
			if (eofexit && fds[0].fd == -1)
				break;
		}

		if (fds[1].revents && copy_from_tls_to_stdout(ctx))
			break;
	}
}

void usage(const char *prog, int ret) {
	printf("usage: %s [-s FD] [-I] [-e] -n SNI\n", prog);
	exit(ret);
}

int main(int argc, char *argv[])
{
	int c, sfd = 1;;
	const char *sni = NULL;
	struct tls_config *tc;
	struct tls *ctx;
	int insecure = 0;
	int localeofexit = 0;

	while ((c = getopt(argc, argv, "ehs:n:I")) != -1) {
		switch (c) {
		case 'e':
			localeofexit = 1;
			break;
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

	if (tls_init() == -1)
		errx(1, "tls_init() failed");

	if ((ctx = tls_client()) == NULL)
		errx(1, "tls_client() failed");

	if (insecure) {
		if ((tc = tls_config_new()) == NULL)
			errx(1, "tls_config_new() failed");
		tls_config_insecure_noverifycert(tc);
		tls_config_insecure_noverifyname(tc);
		tls_config_insecure_noverifytime(tc);
		if (tls_configure(ctx, tc) == -1)
			err(1, "tls_configure: %s", tls_error(ctx));
		tls_config_free(tc);
	}

	if (tls_connect_fds(ctx, sfd, sfd, sni) == -1)
		errx(1, "%s: TLS connect failed", sni);

	if (tls_handshake(ctx) == -1)
		errx(1, "%s: %s", sni, tls_error(ctx));

	copy_loop(ctx, sfd, localeofexit);
	tls_close(ctx);
	return 0;
}
