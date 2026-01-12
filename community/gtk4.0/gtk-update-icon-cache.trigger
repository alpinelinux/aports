#!/bin/sh

for i in "$@"; do
	[ -d "$i" ] || continue
	gtk-update-icon-cache -q -t -f "$i"
	rmdir "$1" 2>/dev/null || :
done
