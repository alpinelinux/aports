#define CONFIG_BOOTCOMMAND \
	"if test -z \"${bootargs}\"; then setenv bootargs console=${console},${baudrate} alpine_dev=mmcblk${devnum}p${devpart}; fi; " \
	"load ${devtype} ${devnum}:${devpart} ${fdt_addr_r} /boot/dtbs/${fdt_file} ; " \
	"load ${devtype} ${devnum}:${devpart} ${kernel_addr_r} /boot/vmlinuz-grsec ; " \
	"load ${devtype} ${devnum}:${devpart} ${initrd_addr_r} /boot/initramfs-grsec ; " \
	"bootz ${kernel_addr_r} ${initrd_addr_r}:${filesize} ${fdt_addr_r}"
