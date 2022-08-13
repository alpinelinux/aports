#!/bin/sh

# Allow the user to override command-line flags
# This is based on Debian's chromium-browser package, and is intended
# to be consistent with Debian.
for f in /etc/electron/*.conf; do
    [ -f ${f} ] && . "${f}"
done

# Prefer user defined ELECTRON_USER_FLAGS (from env) over system
# default ELECTRON_FLAGS (from /etc/electron/default.conf).
ELECTRON_FLAGS=${ELECTRON_USER_FLAGS:-"$ELECTRON_FLAGS"}

exec "/usr/lib/electron/electron" "$@" ${ELECTRON_FLAGS}
