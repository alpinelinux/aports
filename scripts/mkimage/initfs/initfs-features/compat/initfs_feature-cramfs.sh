# Initifs feature cramfs.

initfs_feature_cramfs() { return 0 ;}

_initfs_feature_cramfs_modules() {
	cat <<-'EOF'
		cramfs
	EOF
}

