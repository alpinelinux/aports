#!/bin/sh
if [ ! -L /lib ] && [ "$1" = "post-commit" ]; then
	cat << __EOF__
* WARNING: Alpine Linux is transitioning to a /usr-merged system, and it
* will soon be recommended for everybody to migrate. Help testing is very
* welcomed. For more details, see
* https://gitlab.alpinelinux.org/alpine/infra/alpine-mksite/-/merge_requests/88
__EOF__
fi
