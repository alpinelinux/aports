initfs_feature_fs_xfs() { return 0 ; }

_initfs_feature_fs_xfs_modules() {
	cat <<-'EOF'
		kernel/fs/xfs/
	EOF
	_initfs_feature_crypto_crc32_modules
}

