#!/bin/sh

do_bb_install=

for i in "$@"; do
	case "$i" in
		/lib/modules/*)
			if [ -d "$i" ]; then
				/bin/busybox depmod ${i#/lib/modules/}
			fi
			;;
		*) do_bb_install=yes;;
	esac
done

if [ -n "$do_bb_install" ]; then
	[ -e /bin/bbsuid ] && /bin/bbsuid --install
	/bin/busybox --install -s
fi
