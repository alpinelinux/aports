# Initifs feature 9p.

initfs_feature_9p() { return 0 ; }

_initfs_feature_9p_modules() {
	cat <<-'EOF'
		kernel/net/9p/
		kernel/fs/9p/
	EOF
}

