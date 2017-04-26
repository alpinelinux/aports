# Initifs feature ext2.

initfs_ext2() {
	return 0
}

_initfs_ext2_modules() {
	cat <<-'EOF'
		kernel/fs/ext2
	EOF
}

_initfs_ext2_files() {
	return 0
}
