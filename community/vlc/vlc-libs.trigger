#!/bin/sh

if [ -f /usr/lib/vlc/vlc-cache-gen ]; then
	exec /usr/lib/vlc/vlc-cache-gen "$@" >/dev/null 2>&1
fi
exit 0

