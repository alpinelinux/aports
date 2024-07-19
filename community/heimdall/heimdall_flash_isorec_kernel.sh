#!/bin/sh
set -e

usage()
{
	echo "Flash initramfs and kernel to separate partitions."
	echo "The kernel needs to have its own minimal initramfs, that loads the"
	echo "real initramfs from the other partition (\"isorec\")."
	echo ""
	echo "Usage: $(basename "$0") <initfs> <initfs partition> <kernel> <kernel partition> <dtb>"
	exit 1
}

# Sanity checks
[ "$#" != 5 ] && usage
INITFS="$1"
INITFS_PARTITION="$2"
KERNEL="$3"
KERNEL_PARTITION="$4"
DTB="$5"

for file in "$INITFS" "$KERNEL"; do
	[ -e "$file" ] && continue
	echo "ERROR: File $file does not exist!"
	exit 1
done

echo "Flash initramfs to the '$INITFS_PARTITION' partition (isorec-style) and"
echo "kernel to the '$KERNEL_PARTITION' partition"
gunzip -c "$INITFS" | lzop > /tmp/initramfs.lzo
cat "$KERNEL" "$DTB" > /tmp/vmlinuz-dtb
heimdall flash --wait --"$INITFS_PARTITION" /tmp/initramfs.lzo --"$KERNEL_PARTITION" /tmp/vmlinuz-dtb
rm /tmp/initramfs.lzo
rm /tmp/vmlinuz-dtb
