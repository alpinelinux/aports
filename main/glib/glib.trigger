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
		# Can be removed if all known applications (gsettings-desktop-schemas,
		# seahorse, ibus, gcr3) have migrated.
		if ls -l /bin/sh | grep -q busybox; then
			# busybox sh can handle process substitution like this
			/usr/bin/glib-compile-schemas "$i" 2> >(grep -v "are deprecated")
		else
			# dash or yash cannot, then throw all errors in the bit bucket
			/usr/bin/glib-compile-schemas "$i" 2> /dev/null
		fi
		;;
	esac
done

