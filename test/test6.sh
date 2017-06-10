#!/bin/sh -e

# desc: test triggers in kernel package

$APK add --root $ROOT --initdb -U --repository $PWD/repo1 \
	--repository $SYSREPO alpine-keys linux-vanilla

test -L "$ROOT"/boot/vmlinuz

test -L "$ROOT"/boot/initramfs-vanilla

