# Initifs feature keymap.

initfs_keymap() {
	return 0
}

_initfs_keymap_modules() {
	return 0
}

_initfs_keymap_files() {
	cat <<-'EOF'
		/etc/keymap/*
	EOF
}
