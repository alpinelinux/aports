# Initifs feature btrfs.

initfs_btrfs() {
	return 0
}

_initfs_btrfs_modules() {
	cat <<-'EOF'
		kernel/arch/*/crypto/crc32*
		kernel/crypto/crc32*
		kernel/fs/btrfs
	EOF
}

_initfs_btrfs_files() {
	return 0
}
