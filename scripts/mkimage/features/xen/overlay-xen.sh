overlay_xen_dom0() {
	_call="ovl_script_xen_dom0"
}

# Overlay for Xen dom0
ovl_script_xen_dom0() {

	ovl_runlevel_add sysinit devfs dmesg udev

	ovl_runlevel_add boothwclock modules sysctl hostname bootmisc syslog

	ovl_runlevel_add default udev-postmount
	ovl_runlevel_add default xenstored xenconsoled

	ovl_runlevel_add shutdown mount-ro killprocs savecache

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
