#!/bin/sh
#
# This trigger creates/updates symlinks for default resolver and storage based
# on installed packages.
#

ELEKTRA_LIBS='/usr/lib/elektra'
RESOLVER_LINK="$ELEKTRA_LIBS/libelektra-resolver.so"
STORAGE_LINK="$ELEKTRA_LIBS/libelektra-storage.so"

PREFERRED_RESOLVER='elektra-resolver_fm_hpu_b'
PREFERRED_STORAGE='elektra-dump'


# Prints name of some installed elektra package that *provides* $1
# (e.g. resolver, storage). If package $2 is installed, then it prints that.
# Otherwise it prints name of the first found package based on alphabetical
# order.
find_installed_provider() {
	local provider="$1"
	local preferred="$2"
	local pkgname

	if [ -e "$ELEKTRA_LIBS/lib$preferred.so" ]; then
		echo "$preferred"
	else
		# NOTE: `apk info` doesn't work correctly here, probably
		# due to apk DB lock or something like that.
		for pkgname in $(apk search -aqx elektra-$provider | sort | uniq); do
			if [ -e "$ELEKTRA_LIBS/lib$pkgname.so" ]; then
				echo "$pkgname"
				return 0
			fi
		done
		return 1
	fi
}

if ! [ -e "$RESOLVER_LINK" ]; then
	if resolver=$(find_installed_provider resolver $PREFERRED_RESOLVER); then
		echo "elektra: Switching default resolver to ${resolver#elektra-}" >&2
		ln -sf "lib$resolver.so" "$RESOLVER_LINK"
	else
		echo "elektra: No resolver provider found!" >&2
	fi
fi

if ! [ -e "$STORAGE_LINK" ]; then
	if storage=$(find_installed_provider storage $PREFERRED_STORAGE); then
		echo "elektra: Switching default storage to ${storage#elektra-}" >&2
		ln -sf "lib$storage.so" "$STORAGE_LINK"
	else
		echo "elektra: No storage provider found!" >&2
	fi
fi

exit 0
