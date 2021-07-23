#!/bin/sh

CONFIG='/etc/mkinitfs/mkinitfs.conf'
if [ -f "$CONFIG" ]; then
	case "$(. "$CONFIG" >/dev/null; printf %s "$disable_trigger")" in
		yes | YES | true | TRUE | 1) exit 0;;
	esac
fi

for i in "$@"; do
	# get last element in path
	flavor=${i##*/}
	if ! [ -f "$i"/kernel.release ]; then
		# kernel was uninstalled
		rm -f $( readlink -f /boot/initramfs-$flavor ) \
			/boot/initramfs-$flavor /boot/vmlinuz-$flavor \
			/boot/$flavor /boot/$flavor.gz /$flavor /$flavor.gz
		continue
	fi
	abi_release=$(cat "$i"/kernel.release)
	initfs=initramfs-$flavor
	mkinitfs -o /boot/$initfs $abi_release
done

# extlinux will use path relative partition, so if /boot is on a
# separate partition we want /boot/<kernel> resolve to /<kernel>
if ! [ -e /boot/boot ]; then
	ln -sf . /boot/boot 2>/dev/null # silence error in case of FAT
fi

# cleanup unused initramfs
for i in /boot/initramfs-[0-9]*; do
	[ -f $i ] || continue
	if ! [ -f /boot/vmlinuz-${i#/boot/initramfs-} ]; then
		rm "$i"
	fi
done

sync
exit 0
