initfs_feature_fs_gfs2() { return 0 ; }

_initfs_feature_fs_gfs2_modules() {
	cat <<-'EOF'
		kernel/fs/gfs2/
	EOF
}

