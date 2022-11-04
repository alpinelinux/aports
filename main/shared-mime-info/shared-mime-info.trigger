#!/bin/sh

for i in "$@"; do
	if [ -d "$i" ]; then
		PKGSYSTEM_ENABLE_FSYNC=0 \
		update-mime-database "$i" > /dev/null 2>&1
	fi
done
