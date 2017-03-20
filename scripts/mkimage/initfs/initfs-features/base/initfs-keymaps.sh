initfs_keymaps_all() {
	return 0
}

_initfs_keymaps_all_files() {
	cat <<-'EOF'
		/etc/keymap/*
	EOF
}

