#!/bin/sh

if grep -q '^disable_trigger=1' /etc/update-extlinux.conf 2>/dev/null; then
	exit 0
fi

update-extlinux --warn-only
