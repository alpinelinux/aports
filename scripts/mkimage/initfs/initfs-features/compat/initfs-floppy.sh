# Initifs feature floppy.

initfs_floppy() {
	return 0
}

_initfs_floppy_modules() {
	cat <<-'EOF'
		kernel/drivers/block/floppy.ko
	EOF
}

_initfs_floppy_files() {
	return 0
}
