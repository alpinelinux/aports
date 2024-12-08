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
		# Suppressing output since it shows warnings about bad dconf paths
		# that aren't useful to end users.
		# Can be removed once all known applications (gsettings-desktop-schemas,
		# seahorse, ibus, gcr3) have migrated.
		( /usr/bin/glib-compile-schemas "$i" 2>&1 1>&3 | \
			sed -e '/are deprecated/d' 1>&2 ) 3>&1
		;;
	esac
done
