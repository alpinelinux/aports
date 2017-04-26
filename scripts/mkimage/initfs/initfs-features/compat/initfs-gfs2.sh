# Initifs feature gfs2.

initfs_gfs2() {
	return 0
}

_initfs_gfs2_modules() {
	cat <<-'EOF'
		kernel/fs/gfs2
	EOF
}

_initfs_gfs2_files() {
	return 0
}
