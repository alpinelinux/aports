#!/bin/sh

if grep -q '^disable_trigger=1' /etc/update-grub.conf 2>/dev/null; then
	exit 0
fi

exec /usr/sbin/update-grub
