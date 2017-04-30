# Initifs feature ocfs2.

initfs_feature_ocfs2() { return 0 ; }

_initfs_feature_ocfs2_modules() {
	cat <<-'EOF'
		kernel/fs/ocfs2/
	EOF
}

