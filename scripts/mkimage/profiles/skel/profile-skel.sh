# Skeleton profile - Absolute minimum required root filesystem for apk to function.
#   This will only include bootable content (kernel, initramfs, bootloader, etc.)
#   if $profile_is_bootable is non-empty and not equal to 'false'.

__profile_skel_onload() {
	var_alias profile_is_bootable
	var_alias hostname
}

profile_skel() {
	add_rootfs_apks "alpine-baselayout alpine-keys apk-tools busybox libressl zlib libc-utils"
	set_hostname_if_is_empty "alpine"
	profile_is_bootable_not_empty && profile_is_bootable_not_equal_to "false" && _profile_skel_make_bootable
}

# Bootable skeleton profile - Load skeleton profile with boot content enabled.
#   This profile will include the absolute minimum boot-time content required
#   to bring up a minimal bootable environment, but no drivers or other support.

profile_skel_bootable() {
	set_profile_is_bootable "true"
	profile_skel
}

# Make bootable profile - Minimum bootable content included by bootable skel profile when enabled.
_profile_skel_make_bootable() {
	set_kernel_flavors_if_empty "vanilla"
	add_initfs_load_modules "loop squashfs lz4"
	add_initfs_features "base fs-squashfs"
	add_initfs_apks "alpine-baselayout alpine-keys apk-tools busybox busybox-initscripts libressl zlib libc-utils mkinitfs"
	add_rootfs_apks "busybox-initscripts busybox-suid"
}

