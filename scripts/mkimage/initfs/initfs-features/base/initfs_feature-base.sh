initfs_feature_base() { return 0 ; }

_initfs_feature_base_modules() {
	cat <<-'EOF'
		kernel/drivers/block/loop.ko
		kernel/fs/overlayfs
	EOF
}

_initfs_feature_pkg_busybox() {
	if [ "$BBSTATIC" ] ; then echo 'busybox.static'
	else echo 'busybox' ; fi
}

_initfs_feature_bin_busybox() {
	if [ "$BBSTATIC" ] ; then echo '/bin/busybox.static'
	else echo '/bin/busybox' ; fi
}

_initfs_feature_base_pkgs() {
	cat <<-'EOF'
		alpine-base
		mkinitfs
	EOF
	_initfs_feature_pkg_busybox
}

_initfs_feature_base_files() {
	_initfs_feature_bin_busybox
	cat <<-'EOF'
		/bin/sh
		/lib/mdev
		/sbin/apk
		/etc/passwd
		/etc/group
		/etc/fstab
		/etc/modprobe.d/i386.conf
		/etc/modprobe.d/aliases.conf
		/etc/modprobe.d/blacklist.conf
		/etc/mdev.conf
		/sbin/nlplug-findfs
	EOF
}

_initfs_feature_base_hostcfg() {
	cat <<-'EOF'
		/etc/modprobe.d/i386.conf
		/etc/modprobe.d/aliases.conf
		/etc/modprobe.d/blacklist.conf
		/etc/mdev.conf
	EOF
}

