/* vi: set sw=4 ts=4: */
/*
 * nologin implementation for busybox
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

//config:config NOLOGIN
//config:	bool "nologin"
//config:	default n
//config:	help
//config:	  nologin is a tool that is supposed to be the shell for user accounts
//config:	  that are not supposed to login.

//applet:IF_NOLOGIN(APPLET(nologin, BB_DIR_SBIN, BB_SUID_DROP))
//kbuild:lib-$(CONFIG_NOLOGIN) += nologin.o

//usage:#define nologin_trivial_usage
//usage:       ""
//usage:#define nologin_full_usage "\n\n"
//usage:       "politely refuse a login\n"

#include "libbb.h"
#include <syslog.h>

#define _NOLOGIN_TXT "/etc/nologin.txt"

int nologin_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int nologin_main(int argc UNUSED_PARAM, char **argv)
{
	int fd;
	fd = open(_NOLOGIN_TXT, O_RDONLY);
	if (bb_copyfd_eof(fd, STDOUT_FILENO) == -1)
		bb_error_msg_and_die("this account is not available");
	close(fd);
	return 1;
}

