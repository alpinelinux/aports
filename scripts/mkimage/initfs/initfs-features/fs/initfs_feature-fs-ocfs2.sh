initfs_feature_fs_ocfs2() { return 0 ; }

_initfs_feature_fs_ocfs2_modules() {
	cat <<-'EOF'
		kernel/fs/ocfs2/
	EOF
}

