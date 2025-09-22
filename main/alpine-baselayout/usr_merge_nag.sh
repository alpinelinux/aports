#!/bin/sh
if [ ! -L /lib ] && [ "$1" = "post-commit" ]; then
	cat << __EOF__
* WARNING: The current system is not /usr-merged. You are encouraged to
* migrate manually to ensure the best-possible support. See
* https://alpinelinux.org/posts/2025-10-01-usr-merge.html for more details
__EOF__
fi
