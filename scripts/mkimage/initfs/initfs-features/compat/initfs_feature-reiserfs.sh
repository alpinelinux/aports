# Initifs feature reiserfs.

initfs_feature_reiserfs() { return 0 ; }

_initfs_feature_reiserfs_modules() {
	cat <<-'EOF'
		kernel/fs/reiserfs
	EOF
}

