# Initifs feature 9p.

initfs_9p() {
	return 0
}

_initfs_9p_modules() {
	cat <<-'EOF'
		kernel/net/9p
		kernel/fs/9p
	EOF
}

_initfs_9p_files() {
	return 0
}
