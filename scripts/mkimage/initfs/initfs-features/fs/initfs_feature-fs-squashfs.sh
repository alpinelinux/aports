initfs_feature_fs_squashfs() { return 0 ; }

_initfs_feature_fs_squashfs_modules() {
	cat <<-'EOF'
		kernel/fs/squashfs/
		kernel/lib/lz4/
		kernel/crypto/lz4.ko
		kernel/crypto/lz4hc.ko
	EOF
}

