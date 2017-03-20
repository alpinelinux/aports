initfs_fs_btrfs() {
	return 0
}

_initfs_fs_btrfs_modules() {
	cat <<-'EOF'
		kernel/fs/btrfs/
	EOF
	_initfs_crypto_crc32_modules
}
