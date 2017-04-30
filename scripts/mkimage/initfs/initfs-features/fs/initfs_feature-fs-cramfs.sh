initfs_feature_fs_cramfs() { return 0 ; }

_initfs_feature_fs_cramfs_modules() {
	cat <<-'EOF'
		kernel/fs/cramfs/
	EOF
}

