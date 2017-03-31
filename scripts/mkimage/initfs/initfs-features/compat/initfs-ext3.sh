# Initifs feature ext3.

initfs_ext3() {
	return 0
}

_initfs_ext3_modules() {
	cat <<-'EOF'
		kernel/fs/ext4
	EOF
}

_initfs_ext3_files() {
	return 0
}
