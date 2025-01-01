#!/bin/sh

for i in "$@"; do
	if [ -d "$i" ]; then
		# redirect stdin to workaround problem with
		# apk add --overlay-from-stdin
		# https://gitlab.alpinelinux.org/alpine/aports/-/issues/16692
		PKGSYSTEM_ENABLE_FSYNC=0 \
		update-mime-database "$i" < /dev/null > /dev/null 2>&1
	fi
done
