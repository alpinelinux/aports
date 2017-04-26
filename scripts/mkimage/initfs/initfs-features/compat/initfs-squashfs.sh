# Initifs feature squashfs.

initfs_squashfs() {
	return 0
}

_initfs_squashfs_modules() {
	cat <<-'EOF'
		kernel/fs/squashfs
	EOF
}

_initfs_squashfs_files() {
	return 0
}
