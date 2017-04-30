# Initifs feature keymap.

initfs_feature_keymap() { return 0 ; }

_initfs_feature_keymap_hostcfg() {
	cat <<-'EOF'
		/etc/keymap/*
	EOF
}
