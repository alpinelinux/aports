# Initifs feature ubifs.

initfs_ubifs() {
	return 0
}

_initfs_ubifs_modules() {
	cat <<-'EOF'
		kernel/fs/ubifs
	EOF
}

_initfs_ubifs_files() {
	return 0
}
