# Initifs feature jfs.

initfs_jfs() {
	return 0
}

_initfs_jfs_modules() {
	cat <<-'EOF'
		kernel/fs/jfs
	EOF
}

_initfs_jfs_files() {
	return 0
}
