#!/bin/sh

umask 022
/usr/bin/appstreamcli refresh-cache --force

# /var/cache/app-info was replaced by /var/cache/swcatalog
# Remove this lines once 0.15.3 or greater is available in two
# consecutive releases
rm -rf /var/cache/app-info/cache
test -d /var/cache/app-info && rmdir --ignore-fail-on-non-empty /var/cache/app-info

exit 0
