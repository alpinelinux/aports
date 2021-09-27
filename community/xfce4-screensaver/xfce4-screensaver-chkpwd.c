 /*
Copyright (c) 2021 Natanael Copa <ncopa@alpinelinux.org>

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include <crypt.h>
#include <err.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <shadow.h>

static void sighandler(int sig)
{
	if (sig > 0)
		errx(sig, "caught signal %d", sig);
}

static void setup_signals(void)
{
	struct sigaction action;

	memset((void *) &action, 0, sizeof(action));
	action.sa_handler = sighandler;
	action.sa_flags = SA_RESETHAND;
	sigaction(SIGILL, &action, NULL);
	sigaction(SIGTRAP, &action, NULL);
	sigaction(SIGBUS, &action, NULL);
	sigaction(SIGSEGV, &action, NULL);
	action.sa_handler = SIG_IGN;
	action.sa_flags = 0;
	sigaction(SIGTERM, &action, NULL);
	sigaction(SIGHUP, &action, NULL);
	sigaction(SIGINT, &action, NULL);
	sigaction(SIGQUIT, &action, NULL);
	sigaction(SIGALRM, &action, NULL);
}

int
main (int argc, const char *argv[]) {
	char pass[8192];
	char *encrypted;
	int fd;

	/* Make sure standard file descriptors are connected */
	while ((fd = open("/dev/null", O_RDWR)) <= 2);
	close(fd);

	setup_signals();

	char *user = getlogin();
	if (user == NULL)
		err (1, "failed to get login name");

	int npass = read(STDIN_FILENO, pass, sizeof(pass)-1);
	if (npass < 0)
		err(1, "error reading password");
	pass[npass] = '\0';
	
	/* First check the shadow passwords. */
	struct spwd *p = getspnam((char *)user);
	if (p && p->sp_pwdp && p->sp_pwdp[0] != '*') {
		encrypted = p->sp_pwdp;
	} else {
		/* Check non-shadow passwords too. */
		struct passwd *p = getpwnam (user);
		if (p && p->pw_passwd) {
			encrypted = p->pw_passwd;
		}
	}

	if (encrypted == NULL)
		errx(1, "failed to get encrypted password");

	char *s = crypt(pass, encrypted);
	if (s == NULL)
		err(1, "failed to encrypt password");

	return strcmp(s, encrypted);
}
