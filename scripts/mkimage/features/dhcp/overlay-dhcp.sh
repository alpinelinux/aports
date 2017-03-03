overlay_dhcp_client() {
	_call="ovl_script_dhcp_client"
}

ovl_script_dhcp_client() {
	# TODO: Split into appropriate common overlays.
	ovl_runlevel_add sysinit devfs dmesg mdev hwdrivers modloop
	ovl_runlevel_add boot hwclock modules sysctl hostname bootmisc syslog
	ovl_runlevel_add mount-ro killprocs savecache

	ovl_create_file root:root 0644 /etc/hostname <<-EOF
	$HOSTNAME
	EOF

	ovl_create_file root:root 0644 /etc/network/interfaces <<-EOF
	auto lo
	iface lo inet loopback

	auto eth0
	iface eth0 inet dhcp
	EOF

	ovl_create_file root:root 0644 /etc/apk/world <<-EOF
	alpine-base
	EOF

}
