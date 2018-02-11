#!/bin/sh

for tree in distro-modules distro-modules-common; do
	luarocks-admin-5.3 make-manifest --local-tree --tree=$tree >/dev/null 2>&1
done

exit 0
