# Initifs feature xfs.

initfs_feature_xfs() { return 0 ; }

_initfs_feature_xfs_modules() {
	cat <<-'EOF'
		kernel/arch/*/crypto/crc32*
		kernel/arch/*/crypto/crc32*
		kernel/crypto/crc32*
		kernel/fs/xfs/
	EOF
}

