#!/bin/sh

# generate file lists for each subpackage that has /usr/bin or /usr/sbin

for i in pkg/*/usr/bin/*  pkg/*/usr/sbin/*  pkg/*/usr/lib/samba/*/*.so; do
	[ -f "$i" ] || continue
	n=${i#pkg/}
	n=${n%%/*}

	f=${i#pkg/*/}
	echo "$n $f"
done
