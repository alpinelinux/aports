#!/bin/sh

for i in "$@"; do
	if ! [ -e "$i" ]; then
		continue
	fi
	case "$i" in
	*/modules|*gtk-4.0)
		/usr/bin/gio-querymodules "$i"
		;;
	*/schemas)
		/usr/bin/glib-compile-schemas "$i"
		;;
	esac
done

