# Initifs feature gfs2.

initfs_feature_gfs2() { return 0 ; }

_initfs_feature_gfs2_modules() {
	cat <<-'EOF'
		gfs2
	EOF
}

