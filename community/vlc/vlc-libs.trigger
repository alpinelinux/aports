#!/bin/sh

if [ -f /usr/lib/vlc/vlc-cache-gen ]; then
	exec /usr/lib/vlc/vlc-cache-gen "$@"  >&/dev/null
fi
exit 0

