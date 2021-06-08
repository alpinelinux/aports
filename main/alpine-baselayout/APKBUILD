# Contributor: Sören Tempel <soeren+alpine@soeren-tempel.net>
# Maintainer: Natanael Copa <ncopa@alpinelinux.org>
pkgname=alpine-baselayout
pkgver=3.2.0
pkgrel=14
pkgdesc="Alpine base dir structure and init scripts"
url="https://git.alpinelinux.org/cgit/aports/tree/main/alpine-baselayout"
arch="all"
license="GPL-2.0-only"
pkggroups="shadow"
options="!fhs !check"
install="$pkgname.pre-install $pkgname.pre-upgrade $pkgname.post-upgrade
	$pkgname.post-install"
_nbver=6.2
source="mkmntdirs.c
	crontab
	color_prompt.sh.disabled
	locale.sh

	aliases.conf
	blacklist.conf
	i386.conf
	kms.conf

	group
	inittab
	passwd
	profile
	protocols-$_nbver::https://salsa.debian.org/md/netbase/-/raw/v$_nbver/etc/protocols
	services-$_nbver::https://salsa.debian.org/md/netbase/-/raw/v$_nbver/etc/services
	"
builddir="$srcdir/build"

prepare() {
	default_prepare
	mkdir -p "$builddir"
	mv "$srcdir"/protocols-$_nbver "$srcdir"/protocols
	mv "$srcdir"/services-$_nbver "$srcdir"/services
}

build() {
	${CC:-${CROSS_COMPILE}gcc} $CPPFLAGS $CFLAGS $LDFLAGS \
		"$srcdir"/mkmntdirs.c -o "$builddir"/mkmntdirs

	# generate shadow
	awk -F: '{
		pw = ":!:"
		if ($1 == "root") { pw = "::" }
		print($1 pw ":0:::::")
	}' "$srcdir"/passwd > shadow
}

package() {
	mkdir -p "$pkgdir"
	cd "$pkgdir"
	install -m 0755 -d \
		dev \
		dev/pts \
		dev/shm \
		etc \
		etc/apk \
		etc/conf.d \
		etc/crontabs \
		etc/init.d \
		etc/modprobe.d \
		etc/modules-load.d \
		etc/network/if-down.d \
		etc/network/if-post-down.d \
		etc/network/if-pre-up.d \
		etc/network/if-up.d \
		etc/opt \
		etc/periodic/15min \
		etc/periodic/daily \
		etc/periodic/hourly \
		etc/periodic/monthly \
		etc/periodic/weekly \
		etc/profile.d \
		etc/sysctl.d \
		home \
		lib/firmware \
		lib/mdev \
		lib/modules-load.d \
		lib/sysctl.d \
		media/cdrom \
		media/floppy \
		media/usb \
		mnt \
		proc \
		opt \
		run \
		sbin \
		srv \
		sys \
		usr/bin \
		usr/lib/modules-load.d \
		usr/local/bin \
		usr/local/lib \
		usr/local/share \
		usr/sbin \
		usr/share \
		usr/share/man \
		usr/share/misc \
		var/cache \
		var/cache/misc \
		var/lib \
		var/lib/misc \
		var/local \
		var/lock/subsys \
		var/log \
		var/opt \
		var/spool \
		var/spool/cron \
		var/mail

	ln -s /run var/run
	install -d -m 0555 var/empty
	install -d -m 0700 "$pkgdir"/root
	install -d -m 1777 "$pkgdir"/tmp "$pkgdir"/var/tmp
	install -m755 "$builddir"/mkmntdirs "$pkgdir"/sbin/mkmntdirs

	install -m600 "$srcdir"/crontab "$pkgdir"/etc/crontabs/root
	install -m644 \
		"$srcdir"/color_prompt.sh.disabled \
		"$srcdir"/locale.sh \
		"$pkgdir"/etc/profile.d/
	install -m644 \
		"$srcdir"/aliases.conf \
		"$srcdir"/blacklist.conf \
		"$srcdir"/i386.conf \
		"$srcdir"/kms.conf \
		"$pkgdir"/etc/modprobe.d/

	echo "localhost" > "$pkgdir"/etc/hostname
	cat > "$pkgdir"/etc/hosts <<-EOF
		127.0.0.1	localhost localhost.localdomain
		::1		localhost localhost.localdomain
	EOF
	cat > "$pkgdir"/etc/modules <<-EOF
		af_packet
		ipv6
	EOF
	cat > "$pkgdir"/etc/shells <<-EOF
		# valid login shells
		/bin/sh
		/bin/ash
	EOF
	cat > "$pkgdir"/etc/motd <<-EOF
		Welcome to Alpine!

		The Alpine Wiki contains a large amount of how-to guides and general
		information about administrating Alpine systems.
		See <http://wiki.alpinelinux.org/>.

		You can setup the system with the command: setup-alpine

		You may change this message by editing /etc/motd.

	EOF
	cat > "$pkgdir"/etc/sysctl.conf <<-EOF
		# content of this file will override /etc/sysctl.d/*
	EOF
	cat > "$pkgdir"/lib/sysctl.d/00-alpine.conf <<-EOF
		# Prevents SYN DOS attacks. Applies to ipv6 as well, despite name.
		net.ipv4.tcp_syncookies = 1

		# Prevents ip spoofing.
		net.ipv4.conf.default.rp_filter = 1
		net.ipv4.conf.all.rp_filter = 1

		# Only groups within this id range can use ping.
		net.ipv4.ping_group_range=999 59999

		# Redirects can potentially be used to maliciously alter hosts
		# routing tables.
		net.ipv4.conf.all.accept_redirects = 0
		net.ipv4.conf.all.secure_redirects = 1
		net.ipv6.conf.all.accept_redirects = 0

		# The source routing feature includes some known vulnerabilities.
		net.ipv4.conf.all.accept_source_route = 0
		net.ipv6.conf.all.accept_source_route = 0

		# See RFC 1337
		net.ipv4.tcp_rfc1337 = 1

		## Enable IPv6 Privacy Extensions (see RFC4941 and RFC3041)
		net.ipv6.conf.default.use_tempaddr = 2
		net.ipv6.conf.all.use_tempaddr = 2

		# Restarts computer after 120 seconds after kernel panic
		kernel.panic = 120

		# Users should not be able to create soft or hard links to files
		# which they do not own. This mitigates several privilege
		# escalation vulnerabilities.
		fs.protected_hardlinks = 1
		fs.protected_symlinks = 1

		# Disable unprivileged use of the bpf(2) syscall.
		# Allowing unprivileged use of the bpf(2) syscall may allow a
		# malicious user to compromise the machine.
		kernel.unprivileged_bpf_disabled = 1
	EOF
	cat > "$pkgdir"/etc/fstab <<-EOF
		/dev/cdrom	/media/cdrom	iso9660	noauto,ro 0 0
		/dev/usbdisk	/media/usb	vfat	noauto,ro 0 0
	EOF
	cat > "$pkgdir"/etc/profile.d/README <<-EOF
		This directory should contain shell scripts configuring system-wide
		environment on users' shells.

		Files with the .sh extension found in this directory are evaluated by
		Bourne-compatible shells (like ash, bash or zsh) when started as a
		login shell.
	EOF

	install -m644 \
		"$srcdir"/group \
		"$srcdir"/passwd \
		"$srcdir"/inittab \
		"$srcdir"/profile \
		"$srcdir"/protocols \
		"$srcdir"/services \
		"$pkgdir"/etc/

	install -m640 -g shadow "$builddir"/shadow \
		"$pkgdir"/etc/

	# symlinks
	ln -s /etc/crontabs "$pkgdir"/var/spool/cron/crontabs
	ln -s /proc/mounts "$pkgdir"/etc/mtab
	ln -s /var/mail "$pkgdir"/var/spool/mail
}

sha512sums="
199a34716b1f029407b08679fed4fda58384a1ccefbbec9abe1c64f4a3f7ad2a89bc7c02fc19a7f791f7c6bb87f9f0c708cb3f18c027cb7f54f25976eba4b839  mkmntdirs.c
6e169c0975a1ad1ad871a863e8ee83f053de9ad0b58d94952efa4c28a8c221445d9e9732ad8b52832a50919c2f39aa965a929b3d5b3f9e62f169e2b2e0813d82  crontab
7fcb5df98b0f19e609cb9444b2e6ca5ee97f5f308eb407436acdd0115781623fd89768a9285e9816e36778e565b6f27055f2a586a58f19d6d880de5446d263c4  color_prompt.sh.disabled
b2fc9b72846a43a45ba9a8749e581cef34d1915836833b51b7919dfbf4e275b7d55fec4dea7b23df3796380910971a41331e53e8cf0d304834e3da02cc135e5a  locale.sh
bfe947bdd69e7d93b32c8cb4e2cabe5717cb6c1e1f49a74015ac2cfb13e96d1f12c4be23ae93a1d61aaa3760d33a032fa9bd99f227fb21223a76b5f5908acc65  aliases.conf
0b93db8ba1b5d16b2c23f9b6daea27a3a76c059a1f5ea0369af526ea3f4ff92a6040face89e95c45cf7daaa7a663f229df0f6c1ba24073ef4b2f7b74b298fdae  blacklist.conf
49109d434b577563849c43dd8141961ca798dada74d4d3f49003dac1911f522c43438b8241fa254e4faacdd90058f4d39a7d69b1f493f6d57422c1f706547c95  i386.conf
9dda8c9d1896baf1217aa05ae2936e909300a22a98da9f4c3ba29136852477bf4764321b6a1abb15e93ee58f4a6e77ddfc42cbb12cbbb53cf0f431ace444f72f  kms.conf
abb391a9b5c2b418ad9ea15dcc373a0a0946e5e438d371d00d4bd6c8c60fa81613429a3b8d4313970dcc7eae527793a874c31a9b5a62706f450ab9bb9e8db405  group
fdab6f8fec2a556ab817d90a73635a927ea04dbc4e0470ed59ee6a62c87393f9534c9b746b09a776d938c25b8af9c9fb1686578e24f8307d1d074921ade1bdc7  inittab
06d12a7b9ca14fe17e412d0f24814620b67d035ae859be7906cbf4782dd69e359a6a555dafb98060b7fb7e4714aaa676c88d9017cded36e6d8398e23369bb290  passwd
73cad571186f8c3202ca87af3d735d85b276a0e0c0877fc6f248de22a57f5995963173d6265dff42e0534c9dc96c2ad09bb21ded6add94dc01363382b95da10f  profile
eadc83e47fcc354ab83fd109bee452bda170886fb684e67faf615930c11480919505f4af60c685b124efc54af0ded9522663132f911eac6622144f8b4c8be695  protocols-6.2
adfae0d2f569c2a2f413b7e27683a007fc8ca689b8c3349672fe0dcb6208c192ede4402eff09c604b7e7b4fd9d8df93b875efa5bdaa6c14ff1d8022a7caad5cd  services-6.2
"
