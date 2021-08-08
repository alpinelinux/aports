#!/bin/sh

CFG_FILE='/etc/akms.conf'

if ! [ -f "$CFG_FILE" ]; then
	echo "$CFG_FILE does not exist, skipping akms trigger" >&2
	exit 0
fi

. "$CFG_FILE"

case "$disable_trigger" in
	yes | true | 1) exit 0;;
esac

for srcdir in "$@"; do
	[ -f "$srcdir"/AKMBUILD ] || continue
	akms install "$srcdir"
done

# Triggers exiting with non-zero status cause headaches. APK marks the
# corresponding package and the world as broken and starts exiting with
# status 1 even after e.g. successful installation of a new package.
exit 0
