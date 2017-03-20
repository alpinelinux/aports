initfs_fs_f2fs() {
	return 0
}

_initfs_fs_f2fs_modules() {
	cat <<-'EOF'
		kernel/fs/f2fs
	EOF
}
