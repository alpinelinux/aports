# Initifs feature ext4.

initfs_ext4() {
	return 0
}

_initfs_ext4_modules() {
	cat <<-'EOF'
		kernel/fs/ext4
	EOF
}

_initfs_ext4_files() {
	return 0
}
