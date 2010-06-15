#!/bin/sh

# desc: test triggers in kernel package

$APK add --root $ROOT --initdb -U --repository $PWD/repo1 \
	--repository $SYSREPO linux-grsec

test -L "$ROOT"/boot/vmlinuz-grsec

test -L "$ROOT"/boot/initramfs-grsec

