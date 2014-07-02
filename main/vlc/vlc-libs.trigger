#!/bin/sh

exec /usr/lib/vlc/vlc-cache-gen "$@"  >&/dev/null
exit 0

