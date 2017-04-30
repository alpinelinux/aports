# Initifs feature squashfs.

initfs_feature_squashfs() { return 0 ; }

_initfs_feature_squashfs_modules() {
	cat <<-'EOF'
		kernel/fs/squashfs/
	EOF
}

