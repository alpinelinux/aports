#!/bin/sh

# Allow the user to override command-line flags
# This is based on Debian's chromium-browser package, and is intended
# to be consistent with Debian.
for f in /etc/electron/*.conf; do
    [ -f ${f} ] && . "${f}"
done

# Prefer user defined ELECTRON_USER_FLAGS (from env) over system
# default ELECTRON_FLAGS (from /etc/electron/default.conf).
export ELECTRON_FLAGS="$ELECTRON_FLAGS ${ELECTRON_USER_FLAGS:-"$ELECTRON_USER_FLAGS"}"
# Re-export, for it to be accessible by the process
export ELECTRON_OZONE_PLATFORM_HINT="${ELECTRON_OZONE_PLATFORM_HINT}"

if [ "$ELECTRON_RUN_AS_NODE" == "1" ] && [ "$ELECTRON_STILL_PASS_THE_DEFAULT_FLAGS" != "1" ]; then
	exec "/usr/lib/electron/electron" "$@"
fi

exec "/usr/lib/electron/electron" "$@" ${ELECTRON_FLAGS}
