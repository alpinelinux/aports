# Initifs feature kms.

initfs_kms() {
	return 0
}

_initfs_kms_modules() {
	cat <<-'EOF'
		kernel/drivers/char/agp
		kernel/drivers/gpu
		kernel/drivers/i2c
		kernel/drivers/video
		kernel/arch/x86/video/fbdev.ko
	EOF
}

_initfs_kms_files() {
	return 0
}
