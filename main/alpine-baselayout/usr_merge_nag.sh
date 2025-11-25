#!/bin/sh
if [ ! -L /lib ] && [ "$1" = "post-commit" ]; then
	cat << __EOF__
INFO: The current system is not /usr-merged.
In the future, the /usr-merge will be compulsory and the migration will happen
automatically. You can help testing the migration path by running it manually
before, but there might be known and unknown bugs at the time of migration:
https://gitlab.alpinelinux.org/groups/alpine/-/milestones/16

See https://alpinelinux.org/posts/2025-10-01-usr-merge.html and
https://gitlab.alpinelinux.org/alpine/aports/-/issues/17624#note_551336
for more details.

You can disable this message by masking the usr-merge-nag package:
  apk add '!usr-merge-nag'
__EOF__
fi
