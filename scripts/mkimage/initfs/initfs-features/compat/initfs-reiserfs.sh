# Initifs feature reiserfs.

initfs_reiserfs() {
	return 0
}

_initfs_reiserfs_modules() {
	cat <<-'EOF'
		kernel/fs/reiserfs
	EOF
}

_initfs_reiserfs_files() {
	return 0
}
