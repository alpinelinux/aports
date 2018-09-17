profile_standard() {
	title="Standard"
	desc="Alpine as it was intended.
		Just enough to get you started.
		Network connection is required."
	profile_base
	profile_abbrev="std"
	image_ext="iso"
	arch="x86 x86_64 ppc64le s390x"
	output_format="iso"
	kernel_cmdline="nomodeset"
	kernel_addons="xtables-addons"
	case "$ARCH" in
	s390x)
		apks="$apks s390-tools"
		initfs_features="$initfs_features dasd_mod qeth"
		initfs_cmdline="modules=loop,squashfs,dasd_mod,qeth quiet"
		;;
	ppc64le)
		initfs_cmdline="modules=loop,squashfs,sd-mod,usb-storage,ibmvscsi quiet"
		;;
	esac
}

profile_extended() {
	profile_standard
	profile_abbrev="ext"
	title="Extended"
	desc="Most common used packages included.
		Suitable for routers and servers.
		Runs from RAM."
	arch="x86 x86_64"
	kernel_addons="dahdi-linux xtables-addons zfs spl"
	apks="$apks
		dahdi-linux dahdi-tools ethtool hwdata lftp links
		logrotate lua5.3 lsof lm_sensors lxc lxc-templates nano
		pax-utils paxmark pciutils screen strace sudo tmux
		usbutils v86d vim xtables-addons curl

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
		cciss_vol_status grub-efi lvm2 mdadm mkinitfs mtools nfs-utils
		parted rsync sfdisk syslinux unrar util-linux xfsprogs
		zfs
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

profile_virt() {
	profile_standard
	profile_abbrev="virt"
	title="Virtual"
	desc="Similar to standard.
		Slimmed down kernel.
		Optimized for virtual systems."
	arch="x86 x86_64"
	kernel_addons=
	kernel_flavors="virt"
	kernel_cmdline="console=tty0 console=ttyS0,115200"
	syslinux_serial="0 115200"
}
