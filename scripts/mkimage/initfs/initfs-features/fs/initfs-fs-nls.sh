initfs_fs_nls() {
	initfs_fs_nls_all
}


_initfs_fs_nls_modules() {
	_initfs_fs_nls_all_modules
}

_initfs_fs_nls_files() { return 0 ; }



initfs_fs_nls_all() {
	return 0
}

_initfs_fs_nls_all_modules() {
	cat <<-'EOF'
		kernel/fs/nls/
	EOF
}

_initfs_fs_nls_all_files() { return 0 ; }
