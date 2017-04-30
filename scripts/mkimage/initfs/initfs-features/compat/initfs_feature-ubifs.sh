# Initifs feature ubifs.

initfs_feature_ubifs() { return 0 ; }

_initfs_feature_ubifs_modules() {
	cat <<-'EOF'
		kernel/fs/ubifs/
	EOF
}

