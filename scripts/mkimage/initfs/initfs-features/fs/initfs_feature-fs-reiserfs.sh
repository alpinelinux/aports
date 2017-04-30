initfs_feature_fs_reiserfs() { return 0 ; }

_initfs_feature_fs_reiserfs_modules() {
	cat <<-'EOF'
		kernel/fs/reiserfs/
	EOF
}

