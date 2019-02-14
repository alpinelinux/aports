#!/bin/sh

for i in "$@"; do
	if [ -d "$i" ]; then
		/sbin/depmod ${i#/lib/modules/}
		fi
done

