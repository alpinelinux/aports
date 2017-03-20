initfs_base() {
	return 0
}

_initfs_base_modules() {
	cat <<-'EOF'
		kernel/drivers/block/loop.ko
		kernel/fs/overlayfs
	EOF
}

_initfs_base_files() {
	cat <<-'EOF'
		/bin/busybox
		/bin/sh
		/lib/mdev
		/sbin/apk
		/etc/modprobe.d/*.conf
		/etc/mdev.conf
		/sbin/nlplug-findfs
	EOF
}

