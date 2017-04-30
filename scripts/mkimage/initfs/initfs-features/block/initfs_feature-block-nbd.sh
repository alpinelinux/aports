initfs_feature_block_nbd() { return 0 ; }

_initfs_feature_block_nbd_modules() {
	cat <<-'EOF'
		nbd
	EOF
}

_initfs_feature_block_nbd_pkgs() {
	_initfs_feature_pkg_busybox
}

_initfs_feature_block_nbd_files() {
	cat <<-'EOF'
		/usr/sbin/nbd-client
	EOF
}

