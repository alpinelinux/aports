
initfs_drivers_floppy() {
	return 0
}

_initfs_drivers_floppy_modules() {
	cat <<-'EOF'
		kernel/drivers/block/floppy.ko
	EOF
}
