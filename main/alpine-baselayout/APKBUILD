# Contributor: Sören Tempel <soeren+alpine@soeren-tempel.net>
# Maintainer: Natanael Copa <ncopa@alpinelinux.org>
pkgname=alpine-baselayout
pkgver=3.1.0
pkgrel=2
pkgdesc="Alpine base dir structure and init scripts"
url="https://git.alpinelinux.org/cgit/aports/tree/main/alpine-baselayout"
arch="all"
license="GPL-2.0"
pkggroups="shadow"
options="!fhs"
install="$pkgname.pre-install $pkgname.pre-upgrade $pkgname.post-upgrade
	$pkgname.post-install"
source="mkmntdirs.c
	crontab
	color_prompt

	aliases.conf
	blacklist.conf
	i386.conf
	kms.conf

	group
	inittab
	passwd
	profile
	protocols
	services
	"
builddir="$srcdir/build"

prepare() {
	mkdir -p "$builddir"
}

build() {
	cd "$builddir"
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
		var/spool/cron

	ln -s /run var/run
	install -d -m 0555 var/empty
	install -d -m 0700 "$pkgdir"/root
	install -d -m 1777 "$pkgdir"/tmp "$pkgdir"/var/tmp
	install -m755 "$builddir"/mkmntdirs "$pkgdir"/sbin/mkmntdirs

	install -m600 "$srcdir"/crontab "$pkgdir"/etc/crontabs/root
	install -m644 "$srcdir"/color_prompt "$pkgdir"/etc/profile.d/
	install -m644 \
		"$srcdir"/aliases.conf \
		"$srcdir"/blacklist.conf \
		"$srcdir"/i386.conf \
		"$srcdir"/kms.conf \
		"$pkgdir"/etc/modprobe.d/

	echo "UTC" > "$pkgdir"/etc/TZ
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
	cat > "$pkgdir"/etc/sysctl.d/00-alpine.conf <<-EOF
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
	EOF
	cat > "$pkgdir"/etc/fstab <<-EOF
		/dev/cdrom	/media/cdrom	iso9660	noauto,ro 0 0
		/dev/usbdisk	/media/usb	vfat	noauto,ro 0 0
	EOF

	if [ "$CARCH" = "s390x" ]; then
		local i; for i in $(seq 1 6); do
			sed -i "s/tty$i::/\#tty$i::/g" "$srcdir"/inittab
		done
		echo "console::respawn:/sbin/getty 38400 /dev/console" >> "$srcdir"/inittab
	fi

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
}

sha512sums="199a34716b1f029407b08679fed4fda58384a1ccefbbec9abe1c64f4a3f7ad2a89bc7c02fc19a7f791f7c6bb87f9f0c708cb3f18c027cb7f54f25976eba4b839  mkmntdirs.c
6e169c0975a1ad1ad871a863e8ee83f053de9ad0b58d94952efa4c28a8c221445d9e9732ad8b52832a50919c2f39aa965a929b3d5b3f9e62f169e2b2e0813d82  crontab
7fcb5df98b0f19e609cb9444b2e6ca5ee97f5f308eb407436acdd0115781623fd89768a9285e9816e36778e565b6f27055f2a586a58f19d6d880de5446d263c4  color_prompt
bfe947bdd69e7d93b32c8cb4e2cabe5717cb6c1e1f49a74015ac2cfb13e96d1f12c4be23ae93a1d61aaa3760d33a032fa9bd99f227fb21223a76b5f5908acc65  aliases.conf
2b8e55339955c9670b5b9832bf57e711aca70cd2ebf815a9623fbb7fcd440cca4dd6a4862750885f779080d5c5416de197ff9a250cf116b1c8cf130fafbdaae8  blacklist.conf
49109d434b577563849c43dd8141961ca798dada74d4d3f49003dac1911f522c43438b8241fa254e4faacdd90058f4d39a7d69b1f493f6d57422c1f706547c95  i386.conf
9dda8c9d1896baf1217aa05ae2936e909300a22a98da9f4c3ba29136852477bf4764321b6a1abb15e93ee58f4a6e77ddfc42cbb12cbbb53cf0f431ace444f72f  kms.conf
e6775b9e1c6421338aaceee375b3b74aa100fd444e369b280ce45c9167119b76bebc11737d7f929e50e20a553a35e0e25f7d0f71deb0483d3bccc08e319dcf98  group
fdab6f8fec2a556ab817d90a73635a927ea04dbc4e0470ed59ee6a62c87393f9534c9b746b09a776d938c25b8af9c9fb1686578e24f8307d1d074921ade1bdc7  inittab
17c40af54f29daf542d36da1947913ea9e14af94656efc3e30b9ec2d22d12e3cf3b456466904cf56bc99412e0f650c7a0187c706820da68ce0c99253e53f6338  passwd
c4088a7148c0f161809852d248d2c2272d9c72be3f968c2e2ba40806f508238496eda0f8f2a42aa092773a56800b1dae9f843a42d93f1bb16ba5f58c111d531b  profile
f1548a2b5a107479446f15905f0f2fbf8762815b2215188d49d905c803786d35de6d98005dc0828fb2486b04aaa356f1216a964befddf1e72cb169656e23b6ac  protocols
cecfc06b1f455d65b0c54a5651e601298b455771333e39d0109eeffd7ebd8d81b7738738eb647e6d3076230b6f3707782b83662ea3764ec33dc5e0b3453d3965  services"
