# Overlay for Xen dom0
overlay_xen_dom0() {

	ovl_rc_add devfs sysinit
	ovl_rc_add dmesg sysinit
	ovl_rc_add udev sysinit

	ovl_rc_add hwclock boot
	ovl_rc_add modules boot
	ovl_rc_add sysctl boot
	ovl_rc_add hostname boot
	ovl_rc_add bootmisc boot
	ovl_rc_add syslog boot

	ovl_rc_add udev-postmount default
	ovl_rc_add xenstored default
	ovl_rc_add xenconsoled default

	ovl_rc_add mount-ro shutdown
	ovl_rc_add killprocs shutdown
	ovl_rc_add savecache shutdown

	ovl_create_file root:root 0644 /etc/modules-load.d/xen-dom0.conf <<-EOF
		# Modules needed by Xen dom0
		xen_netback
		xen_blkback
		xenfs
		xen-platform-pci
		xen_wdt
		tun
	EOF

	ovl_create_file root:root 0644 /etc/network/interfaces <<-EOF
		# Network configuration for Xen dom0.
		auto lo
		iface lo inet loopback
	EOF
}
