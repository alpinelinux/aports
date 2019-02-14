#!/bin/sh

# safety. if nlplug-findfs is missing in the initramfs image we may end up
# with an unbootable system.

if ! grep -q -w /sbin/nlplug-findfs /etc/mkinitfs/features.d/base.files; then
	echo "/sbin/nlplug-findfs" >> /etc/mkinitfs/features.d/base.files
fi
