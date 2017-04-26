initfs_fs_xfs() {
	return 0
}

_initfs_fs_xfs_modules() {
	cat <<-'EOF'
		kernel/fs/xfs/
	EOF
	_initfs_crypto_crc32_modules
}

_initfs_fs_xfs_files() { return 0 ; }
