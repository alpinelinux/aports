/* MIT license

Copyright (C) 2015 Natanael Copa <ncopa@alpinelinux.org>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

*/


#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char *program;
static char lockfile[PATH_MAX] = "";

struct cmdarray {
	size_t argc;
	char *argv[32];
};

void add_opt(struct cmdarray *cmd, char *opt)
{
	cmd->argv[cmd->argc++] = opt;
	cmd->argv[cmd->argc] = NULL;
}

int usage(int eval)
{
	printf("usage: %s [-h] [-d DESTDIR] URL\n", program);
	return eval;
}

int fork_exec(char *argv[], int showerr)
{
	int r = 202;
	int status = 0;
	pid_t childpid = fork();
	if (childpid < 0 )
		err(200, "fork");

	if (childpid == 0) {
		execvp(argv[0], argv);
		if (showerr)
			warn("%s", argv[0]);
		_exit(201);
	}

	/* wait for curl/wget and get the exit code */
	wait(&status);
	if (WIFEXITED(status))
		r = WEXITSTATUS(status);
	return r;
}

/* create or wait for an NFS-safe lockfile and fetch url with curl or wget */
int fetch(char *url, const char *destdir)
{
	int lockfd, status=0;
	char outfile[PATH_MAX], partfile[PATH_MAX];
	char *name, *p;
	struct flock fl = {
		.l_type = F_WRLCK,
		.l_whence = SEEK_SET,
		.l_start = 1,
		.l_len = 0,
	};
	struct cmdarray curlcmd = {
		.argc = 6,
		.argv = { "curl", "-k", "-L", "-f", "-o", partfile, NULL }
	};
	struct cmdarray wgetcmd = {
		.argc = 3,
		.argv = { "wget", "-O", partfile, NULL }
	};

	name = strrchr(url, '/');
	if (name == NULL)
		errx(1, "%s: no '/' in url", url);
	p = strstr(url, "::");
	if (p != NULL) {
		name = url;
		*p = '\0';
		url = p + 2;
	} else {
		name++;
	}

	snprintf(outfile, sizeof(outfile), "%s/%s", destdir, name);
	snprintf(lockfile, sizeof(lockfile), "%s.lock", outfile);
	snprintf(partfile, sizeof(partfile), "%s.part", outfile);

	lockfd = open(lockfile, O_WRONLY|O_CREAT, 0660);
	if (lockfd < 0)
		err(1, "%s", lockfile);

	if (fcntl(lockfd, F_SETLK, &fl) < 0) {
		int i;
		printf("Waiting for %s ...\n", lockfile);
		for (i=0; i<10; i++) {
			int r = fcntl(lockfd, F_SETLKW, &fl);
			if (r == 0)
				break;
			if (r == -1 && errno != ESTALE)
				err(1, "fcntl(F_SETLKW)");
			sleep(1);
		}
	}

	if (access(outfile, F_OK) == 0)
		goto fetch_done;

	if (access(partfile, F_OK) == 0) {
		printf("Partial download found. Trying to resume.\n");
		add_opt(&curlcmd, "-C");
		add_opt(&curlcmd, "-");
		add_opt(&wgetcmd, "-c");
	}

	add_opt(&curlcmd, url);
	add_opt(&wgetcmd, url);

	status = fork_exec(curlcmd.argv, 0);

	/* CURLE_RANGE_ERROR (33)
	   The server does not support or accept range requests. */
	if (status == 33)
		unlink(partfile);

	/* is we failed execute curl, then fallback to wget */
	if (status == 201)
		status = fork_exec(wgetcmd.argv, 1);

	/* only rename completed downloads */
	if (status == 0)
		rename(partfile, outfile);

fetch_done:
	unlink(lockfile);
	close(lockfd);
	lockfile[0] = '\0';
	return status;
}

void sighandler(int sig)
{
	switch(sig) {
	case SIGABRT:
	case SIGINT:
	case SIGQUIT:
	case SIGTERM:
		unlink(lockfile);
		exit(0);
		break;
	default:
		break;
	}
}

/* exit codes get passed through from curl/wget (so we can check in abuild
   whether the server does not support resuming). Additional exit codes:
   200: fork failed
   201: curl/wget could not be started
   202: curl/wget did not terminate normally
   203: usage displayed */
int main(int argc, char *argv[])
{
	int opt;
	char *destdir = "/var/cache/distfiles";

	program = argv[0];
	while ((opt = getopt(argc, argv, "hd:")) != -1) {
		switch (opt) {
		case 'h':
			return usage(0);
			break;
		case 'd':
			destdir = optarg;
			break;
		default:
			printf("Unknown option '%c'\n", opt);
			return usage(1);
			break;
		}
	}

	argv += optind;
	argc -= optind;

	if (argc != 1)
		return usage(203);

	signal(SIGABRT, sighandler);
	signal(SIGINT, sighandler);
	signal(SIGQUIT, sighandler);
	signal(SIGTERM, sighandler);

	return fetch(argv[0], destdir);
}
