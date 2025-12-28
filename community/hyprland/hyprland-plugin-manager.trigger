#!/bin/sh

# warn users about broken hyprland-plugin-manager going away

cat >&2 <<-EOF
*
* You have hyprland-plugin-manager (hyprpm) installed.
* This package is not working properly as of Alpine 3.23 and will be removed in a future Alpine release
* Please remove it and prefer to use plugins installed from packages instead
* For more information see https://gitlab.alpinelinux.org/alpine/aports/-/issues/17576
*
EOF

exit 0
