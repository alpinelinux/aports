initfs_fs_ocfs2() {
	return 0
}

_initfs_fs_ocfs2_modules() {
	cat <<-'EOF'
		kernel/fs/ocfs2/
	EOF
}

_initfs_fs_ocfs2_files() { return 0 ; }
