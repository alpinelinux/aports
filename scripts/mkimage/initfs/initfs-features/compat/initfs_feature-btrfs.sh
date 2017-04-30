# Initifs feature btrfs.

initfs_feature_btrfs() { return 0 ; }

_initfs_feature_btrfs_modules() {
	cat <<-'EOF'
		kernel/arch/*/crypto/crc32*
		kernel/crypto/crc32*
		kernel/fs/btrfs
	EOF
}
