#!/bin/sh
# adapted from main/mkinitfs/mkinitfs.trigger

for i in "$@"; do
	# get last element in path
	flavor=${i##*/}
	if ! [ -f "$i"/kernel.release ]; then
		# kernel was uninstalled
		rm -f $( readlink -f /boot/booster-$flavor ) \
			/boot/booster-$flavor /boot/vmlinuz-$flavor \
			/boot/$flavor /boot/$flavor.gz /$flavor /$flavor.gz
		continue
	fi
	read abi_release < "$i"/kernel.release
	initfs=booster-$flavor
	booster build --force --kernel-version $abi_release /boot/$initfs
done

# extlinux will use path relative partition, so if /boot is on a
# separate partition we want /boot/<kernel> resolve to /<kernel>
if ! [ -e /boot/boot ]; then
	ln -sf . /boot/boot 2>/dev/null # silence error in case of FAT
fi

sync
exit 0
