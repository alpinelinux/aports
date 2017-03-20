initfs_fs_btrfs() {
	return 0
}

_initfs_fs_btrfs_modules() {
	# FIXME: Crypto should be on its own.
	cat <<-'EOF'
		kernel/arch/*/crypto/crc32*
		kernel/crypto/crc32*
		kernel/fs/btrfs
	EOF
}
