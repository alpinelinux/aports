# TODO: Clean up packages list using features where appropriate.
profile_extended() {
	feature_ntp "provider=sntpc"
	profile_standard
	feature_dahdi
	feature_xtables_addons
	add_apks "
		ethtool hwdata lftp links logrotate lua5.3 lsof
		lm_sensors lxc lxc-templates nano pax-utils paxctl
		pciutils screen strace sudo tmux usbutils v86d vim

		acct arpon arpwatch awall bridge-utils bwm-ng
		ca-certificates conntrack-tools cutter cyrus-sasl dhcp
		dhcpcd dhcrelay dnsmasq email fping fprobe haserl htop
		igmpproxy ip6tables iproute2 iproute2-qos ipsec-tools
		iptables iputils irssi ldns-tools links
		ncurses-terminfo net-snmp net-snmp-tools nrpe nsd
		opennhrp openvpn openvswitch pingu ppp quagga
		quagga-nhrp rpcbind socat ssmtp strongswan
		sysklogd tcpdump tcpproxy tinyproxy unbound
		wireless-tools wpa_supplicant zonenotify

		btrfs-progs cksfv dosfstools cryptsetup
		cciss_vol_status lvm2 mdadm mkinitfs mtools nfs-utils
		parted rsync sfdisk syslinux unrar util-linux xfsprogs
		"

	add_apks "linux-firmware"
	add_apks_flavored "linux"
}
