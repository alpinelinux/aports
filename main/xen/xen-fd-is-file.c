#include <sys/stat.h>

#include <stdio.h>
#include <fcntl.h>
#include <err.h>

/*

this is to be used as:

    while true; do
        eval "exec $_lockfd<>$_lockfile"
        flock -x $_lockfd || return $?
        if xen-fd-is-file $_lockfd $_lockfile; then break; fi
        eval "exec $_lockfd<&-"
    done

instead of:

    local rightfile
    while true; do
        eval "exec $_lockfd<>$_lockfile"
        flock -x $_lockfd || return $?
        # We can't just stat /dev/stdin or /proc/self/fd/$_lockfd or
        # use bash's test -ef because those all go through what is
        # actually a synthetic symlink in /proc and we aren't
        # guaranteed that our stat(2) won't lose the race with an
        # rm(1) between reading the synthetic link and traversing the
        # file system to find the inum.  Perl is very fast so use that.
        rightfile=$( perl -e '
            open STDIN, "<&'$_lockfd'" or die $!;
            my $fd_inum = (stat STDIN)[1]; die $! unless defined $fd_inum;
            my $file_inum = (stat $ARGV[0])[1];
            print "y\n" if $fd_inum eq $file_inum;
                             ' "$_lockfile" )
        if [ x$rightfile = xy ]; then break; fi
        # Some versions of bash appear to be buggy if the same
        # $_lockfile is opened repeatedly. Close the current fd here.
        eval "exec $_lockfd<&-"
    done
*/

int main(int argc, char *argv[])
{
	int lockfd;
	const char *filename;
	struct stat fdst, filest;

	if (argc <= 2)
		errx(1, "usage: %s FDNUM FILENAME\n", argv[0]);

	lockfd = atoi(argv[1]);
	filename = argv[2];

	if (fstat(lockfd, &fdst) < 0)
		err(2, "fstat(%i)", lockfd);

	if (stat(filename, &filest) < 0)
		err(3, "stat(%s)", filename);

	return (fdst.st_ino != filest.st_ino);
}
