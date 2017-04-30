initfs_feature_fs_jfs() { return 0 ; }

_initfs_feature_fs_jfs_modules() {
	cat <<-'EOF'
		kernel/fs/jfs/
	EOF
}

