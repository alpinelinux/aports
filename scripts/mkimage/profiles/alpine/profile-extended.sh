profile_extended() {
	profile_standard
	kernel_addons="dahdi-linux xtables-addons"
	apks="$apks
		dahdi-linux dahdi-tools ethtool hwdata lftp links
		logrotate lua5.3 lsof lm_sensors lxc lxc-templates nano
		pax-utils paxctl pciutils screen strace sudo tmux
		usbutils v86d vim xtables-addons

		acct arpon arpwatch awall bridge-utils bwm-ng
		ca-certificates conntrack-tools cutter cyrus-sasl dhcp
		dhcpcd dhcrelay dnsmasq email fping fprobe haserl htop
		igmpproxy ip6tables iproute2 iproute2-qos ipsec-tools
		iptables iputils irssi ldns-tools links
		ncurses-terminfo net-snmp net-snmp-tools nrpe nsd
		opennhrp openvpn openvswitch pingu ppp quagga
		quagga-nhrp rpcbind sntpc socat ssmtp strongswan
		sysklogd tcpdump tcpproxy tinyproxy unbound
		wireless-tools wpa_supplicant zonenotify

		btrfs-progs cksfv dosfstools cryptsetup
		cciss_vol_status lvm2 mdadm mkinitfs mtools nfs-utils
		parted rsync sfdisk syslinux unrar util-linux xfsprogs
		"

	local _k _a
	for _k in $kernel_flavors; do
		apks="$apks linux-$_k"
		for _a in $kernel_addons; do
			apks="$apks $_a-$_k"
		done
	done
	apks="$apks linux-firmware"
}
