# Overlay to enable ZFS filesystem support.
overlay_zfs() {

	ovl_rc_add devfs sysinit
	ovl_rc_add dmesg sysvinit
	ovl_rc_add udev sysinit

	ovl_rc_add modules boot
	ovl_rc_add sysfs boot
	ovl_rc_add zfs-import boot
	ovl_rc-add zfs-mount boot
	ovl_rc_add zfs-zed boot
	ovl_rc_add zfs-share boot

	ovl_create_file root:root 0644 /etc/modules-load.d/zfs <<-EOF
		# Modules to support ZFS filesystem
		spl
		splat
		zavl
		znvpair
		zcommon
		zunicode
		zfs
	EOF
}

