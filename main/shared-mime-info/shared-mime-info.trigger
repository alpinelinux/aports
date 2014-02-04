#!/bin/sh

for i in "$@"; do
	if [ -d "$i" ]; then
		update-mime-database "$i" > /dev/null 2>&1
		sync
	fi
done
