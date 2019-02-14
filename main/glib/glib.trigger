#!/bin/sh

for i in "$@"; do
	if ! [ -e "$i" ]; then
		continue
	fi
	case "$i" in
	*/modules)
		/usr/bin/gio-querymodules "$i"
		;;
	*/schemas)
		/usr/bin/glib-compile-schemas "$i"
		;;
	esac
done

