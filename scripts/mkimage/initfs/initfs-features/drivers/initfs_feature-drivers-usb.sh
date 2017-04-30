initfs_feature_drivers_usb() { return 0 ; }

_initfs_feature_drivers_usb_modules() {
	cat <<-'EOF'
		kernel/drivers/usb/host
		kernel/drivers/usb/storage
		kernel/drivers/hid/usbhid
		kernel/drivers/hid/hid-generic.ko
		kernel/drivers/hid/hid-cherry.ko
	EOF
}

_initfs_feature_drivers_usb_files() { return 0 ; }
