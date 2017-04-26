# Initifs feature f2fs.

initfs_f2fs() {
	return 0
}

_initfs_f2fs_modules() {
	cat <<-'EOF'
		kernel/fs/f2fs
	EOF
}

_initfs_f2fs_files() {
	return 0
}
