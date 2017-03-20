initfs_fs_xfs() {
	return 0
}

_initfs_fs_xfs_modules() {
	# FIXME: Crypto should be on its own.
	cat <<-'EOF'
		kernel/arch/*/crypto/crc32*
		kernel/crypto/crc32*
		kernel/fs/xfs
	EOF
}
