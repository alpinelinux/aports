#!/bin/sh

for i in "$@"; do
	if ! [ -e "$i" ]; then
		continue
	fi
	/usr/bin/gio-querymodules "$i"
done

