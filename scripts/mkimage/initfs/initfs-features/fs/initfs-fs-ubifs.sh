initfs_fs_ubifs() {
	return 0
}

_initfs_fs_ubifs_modules() {
	cat <<-'EOF'
		kernel/fs/ubifs
	EOF
}

