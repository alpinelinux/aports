/* url.c - Alpine Package Keeper (APK)
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
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#include "apk_io.h"

const char *apk_url_local_file(const char *url)
{
	if (strncmp(url, "file:", 5) == 0)
		return &url[5];

	if (strncmp(url, "http:", 5) != 0 &&
	    strncmp(url, "https:", 6) != 0 &&
	    strncmp(url, "ftp:", 4) != 0)
		return url;

	return NULL;
}

static int fork_wget(const char *url)
{
	pid_t pid;
	int fds[2];

	if (pipe(fds) < 0)
		return -1;

	pid = fork();
	if (pid == -1) {
		close(fds[0]);
		close(fds[1]);
		return -1;
	}

	if (pid == 0) {
		setsid();
		close(fds[0]);
		dup2(open("/dev/null", O_RDONLY), STDIN_FILENO);
		dup2(fds[1], STDOUT_FILENO);
		execlp("wget", "wget", "-q", "-O", "-", url, NULL);
		exit(0);
	}

	close(fds[1]);
	return fds[0];
}

struct apk_istream *apk_istream_from_url(const char *url)
{
	if (apk_url_local_file(url) != NULL)
		return apk_istream_from_file(apk_url_local_file(url));

	return apk_istream_from_fd(fork_wget(url));
}

struct apk_istream *apk_istream_from_url_gz(const char *file)
{
	return apk_bstream_gunzip(apk_bstream_from_url(file), TRUE);
}

struct apk_bstream *apk_bstream_from_url(const char *url)
{
	if (apk_url_local_file(url))
		return apk_bstream_from_file(url);

	return apk_bstream_from_fd(fork_wget(url));
}

int apk_url_download(const char *url, const char *file)
{
	pid_t pid;
	int status;
	char tmp[256];

	pid = fork();
	if (pid == -1)
		return -1;

	if (pid == 0) {
		setsid();
		dup2(open("/dev/null", O_RDONLY), STDIN_FILENO);
		snprintf(tmp, sizeof(tmp), "%s.backup", file);
		rename(file, tmp);
		execlp("wget", "wget", "-q", "-O", file, url, NULL);
		exit(0);
	}

	waitpid(pid, &status, 0);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		unlink(file);
		return -1;
	}

	return 0;
}

