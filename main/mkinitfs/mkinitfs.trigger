#!/bin/sh

CONFIG='/etc/mkinitfs/mkinitfs.conf'
if [ -f "$CONFIG" ]; then
	case "$(. "$CONFIG" >/dev/null; printf %s "$disable_trigger")" in
		yes | YES | true | TRUE | 1) exit 0;;
	esac
fi

for i in "$@"; do
	[ -f "$i" ] && continue
	# get last element in path
	abi_release=${i##*/}

	suffix="$(cat "$i"/kernel-suffix 2>/dev/null)" || {
		# clean up on uninstall
		suffix="$(cat "$i/initramfs-suffix" 2>/dev/null)" || {
			# fallback suffix
			flavor="${abi_release##*[0-9]-}"
			if [ "$flavor" != "$abi_release" ]; then
				suffix="-$flavor"
			fi
		}

		rm -f "$i"/initramfs-suffix
		rmdir "$i" 2>/dev/null
		if ! [ -e "/boot/vmlinuz$suffix" ]; then
			# kernel was removed
			rm -v "/boot/initramfs$suffix"
			continue
		fi

		# upgrading
		if ! [ -e "$i"/modules.order ]; then
			continue
		fi
	}

	# store the initramfs suffix for removal
	echo "$suffix" > "$i"/initramfs-suffix
	initramfs="/boot/initramfs$suffix"
	mkinitfs -o "$initramfs" "$abi_release" || {
		echo "  mkinitfs failed!" >&2
		echo "  your system may not be bootable" >&2
		exit 1
	}
done

# extlinux will use path relative partition, so if /boot is on a
# separate partition we want /boot/<kernel> resolve to /<kernel>
if ! [ -e /boot/boot ]; then
	ln -sf . /boot/boot 2>/dev/null # silence error in case of FAT
fi

# sync only the filesystem on /boot as that is where we are writing the initfs.
sync -f /boot
exit 0
