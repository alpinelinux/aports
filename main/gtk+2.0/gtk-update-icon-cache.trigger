#!/bin/sh

for i in "$@"; do
	if ! [ -e "$i" ]; then
		continue
	fi
	gtk-update-icon-cache -q -t -f $i
done

