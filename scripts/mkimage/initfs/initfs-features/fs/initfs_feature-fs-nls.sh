initfs_feature_fs_nls() {
	initfs_feature_fs_nls_all
}


_initfs_feature_fs_nls_modules() {
	_initfs_feature_fs_nls_all_modules
}


initfs_feature_fs_nls_all() { return 0 ; }

_initfs_feature_fs_nls_all_modules() {
	cat <<-'EOF'
		kernel/fs/nls/
	EOF
}

