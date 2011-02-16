#!/bin/sh

for i in "$@"; do
	case "$i" in
	*/modules)
		/usr/bin/gio-querymodules "$i"
		;;
	*/schemas)
		/usr/bin/glib-compile-schemas "$i"
		;;
	esac
done

