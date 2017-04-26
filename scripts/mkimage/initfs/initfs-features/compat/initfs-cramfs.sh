# Initifs feature cramfs.

initfs_cramfs() {
	return 0
}

_initfs_cramfs_modules() {
	cat <<-'EOF'
		kernel/fs/cramfs
	EOF
}

_initfs_cramfs_files() {
	return 0
}
