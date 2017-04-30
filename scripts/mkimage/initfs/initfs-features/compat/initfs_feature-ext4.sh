# Initifs feature ext4.

initfs_feature_ext4() { return 0 ; }

_initfs_feature_ext4_modules() {
	cat <<-'EOF'
		ext4
	EOF
}

