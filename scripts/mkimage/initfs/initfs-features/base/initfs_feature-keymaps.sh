initfs_feature_keymaps_all() { return 0 ; }

_initfs_feature_keymaps_all_pkgs() {
	cat <<-'EOF'
		bkeymaps
	EOF
}

_initfs_feature_keymaps_all_files() {
	cat <<-'EOF'
		/usr/share/bkeymaps/
	EOF
}

_initfs_feature_keymaps_all_hostcfg() {
	cat <<-'EOF'
		/etc/keymap/*
	EOF
}

