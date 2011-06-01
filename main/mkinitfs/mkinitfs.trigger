#!/bin/sh

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
	initfs=initramfs-$abi_release
	mkinitfs -o /boot/$initfs $abi_release
	ln -sf $initfs /boot/initramfs-$flavor
	ln -sf vmlinuz-$abi_release /boot/vmlinuz-$flavor

	#this is for compat. to be removed eventually...
	ln -sf vmlinuz-$flavor /boot/$flavor
	ln -sf initramfs-$flavor /boot/$flavor.gz
	ln -sf /boot/vmlinuz-$flavor /$flavor
	ln -sf /boot/initramfs-$flavor /$flavor.gz

	# Update the /boot/extlinux.conf to point to correct kernel
	f=/boot/extlinux.conf
	if [ -f $f ] && grep -q -- "kernel /$flavor" $f; then
		sed -i -e "s:kernel /$flavor:kernel /boot/vmlinuz-$flavor:" \
		  -e "s:initrd=/$flavor.gz:initrd=/boot/initramfs-$flavor:" \
		  -e "s:initrd /$flavor.gz:initrd /boot/initramfs-$flavor:" \
		  $f
	fi

done

# extlinux will use path relative partition, so if /boot is on a
# separate partition we want /boot/<kernel> resolve to /<kernel>
if ! [ -e /boot/boot ]; then
	ln -sf / /boot/boot
fi

# cleanup unused initramfs
for i in /boot/initramfs-[0-9]*; do
	[ -f $i ] || continue
	if ! [ -f /boot/vmlinuz-${i#/boot/initramfs-} ]; then
		rm "$i"
	fi
done

exit 0
