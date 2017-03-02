overlay_dhcp() {

	ovl_rc_add devfs sysinit
	ovl_rc_add dmesg sysinit
	ovl_rc_add mdev sysinit
	ovl_rc_add hwdrivers sysinit
	ovl_rc_add modloop sysinit

	ovl_rc_add hwclock boot
	ovl_rc_add modules boot
	ovl_rc_add sysctl boot
	ovl_rc_add hostname boot
	ovl_rc_add bootmisc boot
	ovl_rc_add syslog boot

	ovl_rc_add mount-ro shutdown
	ovl_rc_add killprocs shutdown
	ovl_rc_add savecache shutdown

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
