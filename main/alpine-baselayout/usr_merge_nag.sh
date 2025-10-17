#!/bin/sh
if [ ! -L /lib ] && [ "$1" = "post-commit" ]; then
	cat << __EOF__
* WARNING: The current system is not /usr-merged.
* You are encouraged to migrate manually to ensure system reliability.
*
* See https://alpinelinux.org/posts/2025-10-01-usr-merge.html for more
* details.
*
* You can disable this message by masking the usr-merge-nag package:
*   apk add '!usr-merge-nag'
__EOF__
fi
