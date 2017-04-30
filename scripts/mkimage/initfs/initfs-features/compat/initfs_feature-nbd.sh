# Initifs feature nbd.

initfs_feature_nbd() { return 0 ; }

_initfs_feature_nbd_modules() {
	cat <<-'EOF'
		kernel/drivers/block/nbd.ko
	EOF
}

_initfs_feature_nbd_pkgs() {
	_initfs_feature_pkg_busybox
}

_initfs_feature_nbd_files() {
	cat <<-'EOF'
		/usr/sbin/nbd-client
	EOF
}
