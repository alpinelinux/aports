initfs_feature_fs_cifs() { return 0 ; }

_initfs_feature_fs_cifs_modules() {
	cat <<-'EOF'
		kernel/fs/cifs/
	EOF
}

