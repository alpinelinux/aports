#!/bin/sh

loaders=
for i in "$@"; do
	case "$i" in
		/usr/lib/gtk-2.0/2.10.0/loaders)
			loaders=1
			;;
		/usr/share/icons/*)
			gtk-update-icon-cache -q -t -f $i
			;;
	esac
done

if [ -n "$loaders" ]; then
	gdk-pixbuf-query-loaders > etc/gtk-2.0/gdk-pixbuf.loaders
fi
