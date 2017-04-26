# Overlay to enable ZFS filesystem support.
overlay_zfs() {
	#_before="nfs samba iscsi"
	#_after="udev"
	#_needs="udev"

	_call="ovl_script_zfs"
}

ovl_script_zfs() {

	# These should move to a common overlay.
	ovl_runlevel_add sysinit devfs dmesg udev
	ovl_runlevel_add boot modules sysfs

	# Run ZFS scripts at boot.
	ovl_runlevel_add boot zfs-import zfs-mount zfs-zed zfs-share

	# Create modules configuration file in /etc/modules-load.d/
	ovl_create_file root:root 0644 /etc/modules-load.d/zfs.conf <<-EOF
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

