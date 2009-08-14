#!/bin/sh

do_bb_install=

for i in "$@"; do
	case "$i" in
		/lib/modules/*)	/bin/busybox depmod ${i#/lib/modules/};;
		*) do_bb_install=yes;;
	esac
done

if [ -n "$do_bb_install" ]; then
	/bin/busybox --install -s
fi

