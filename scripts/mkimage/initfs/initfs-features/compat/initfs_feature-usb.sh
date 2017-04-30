# Initifs feature usb.

initfs_feature_usb() { return 0 ; }

_initfs_feature_usb_modules() {
	cat <<-'EOF'
		kernel/drivers/usb/host/
		kernel/drivers/usb/storage/
		kernel/drivers/hid/usbhid/
		kernel/drivers/hid/hid-generic.ko
		kernel/drivers/hid/hid-cherry.ko
		kernel/fs/fat/
		kernel/fs/nls/
	EOF
}

