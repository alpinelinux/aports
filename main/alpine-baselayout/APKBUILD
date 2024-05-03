# Contributor: Sören Tempel <soeren+alpine@soeren-tempel.net>
# Maintainer: Natanael Copa <ncopa@alpinelinux.org>
pkgname=alpine-baselayout
pkgver=3.6.5
pkgrel=0
pkgdesc="Alpine base dir structure and init scripts"
url="https://git.alpinelinux.org/cgit/aports/tree/main/alpine-baselayout"
arch="noarch"
license="GPL-2.0-only"
pkggroups="shadow"
replaces_priority=1000
options="!fhs !check keepdirs"
depends="$pkgname-data=$pkgver-r$pkgrel"
subpackages="$pkgname-data"
install="$pkgname.pre-install $pkgname.pre-upgrade $pkgname.post-upgrade
	$pkgname.post-install"
_nbver=6.4
source="crontab
	color_prompt.sh.disabled
	20locale.sh

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
	# generate shadow
	awk -F: '{
		pw = ":!:"
		if ($1 == "root") { pw = "::" }
		print($1 pw ":0:::::")
	}' "$srcdir"/passwd > shadow
}

data() {
	replaces="alpine-baselayout"
	depends=

	amove etc/fstab
	amove etc/group
	amove etc/hostname
	amove etc/hosts
	amove etc/inittab
	amove etc/nsswitch.conf
	amove etc/modules
	amove etc/mtab
	amove etc/passwd
	amove etc/profile
	amove etc/protocols
	amove etc/services
	amove etc/shadow
	amove etc/shells
	amove etc/sysctl.conf
}

package() {
	mkdir -p "$pkgdir"
	cd "$pkgdir"
	install -m 0755 -d \
		dev \
		dev/pts \
		dev/shm \
		etc \
		etc/crontabs \
		etc/modprobe.d \
		etc/modules-load.d \
		etc/network \
		etc/network/if-down.d \
		etc/network/if-post-down.d \
		etc/network/if-pre-up.d \
		etc/network/if-up.d \
		etc/opt \
		etc/periodic \
		etc/periodic/15min \
		etc/periodic/daily \
		etc/periodic/hourly \
		etc/periodic/monthly \
		etc/periodic/weekly \
		etc/profile.d \
		etc/sysctl.d \
		home \
		lib \
		lib/firmware \
		lib/modules-load.d \
		lib/sysctl.d \
		media \
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
		usr \
		usr/bin \
		usr/lib \
		usr/lib/modules-load.d \
		usr/local \
		usr/local/bin \
		usr/local/lib \
		usr/local/share \
		usr/sbin \
		usr/share \
		usr/share/man \
		usr/share/misc \
		var \
		var/cache \
		var/cache/misc \
		var/lib \
		var/lib/misc \
		var/local \
		var/lock \
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

	install -m600 "$srcdir"/crontab "$pkgdir"/etc/crontabs/root
	install -m644 \
		"$srcdir"/color_prompt.sh.disabled \
		"$srcdir"/20locale.sh \
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
		See <https://wiki.alpinelinux.org/>.

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
	cat > "$pkgdir"/etc/nsswitch.conf <<-EOF
		# musl itself does not support NSS, however some third-party DNS
		# implementations use the nsswitch.conf file to determine what
		# policy to follow.
		# Editing this file is not recommended.
		hosts: files dns
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
6e169c0975a1ad1ad871a863e8ee83f053de9ad0b58d94952efa4c28a8c221445d9e9732ad8b52832a50919c2f39aa965a929b3d5b3f9e62f169e2b2e0813d82  crontab
558071efdce2fe92afe4277006235b1a6368b070337c7567e5632a1a3fe531f87ca692eb36f3dda498d4d29d1f834fc8f7139f2985669ae3400b6d103d6f4c5e  color_prompt.sh.disabled
03361d912cf29c127608697ee14bfa5972f82a5c475e653378ca5f7670cbd8183efc7c8c339ff046ff6537944fe00c4a732bb6b552aecaecd1214ed3e11bdf90  20locale.sh
bfe947bdd69e7d93b32c8cb4e2cabe5717cb6c1e1f49a74015ac2cfb13e96d1f12c4be23ae93a1d61aaa3760d33a032fa9bd99f227fb21223a76b5f5908acc65  aliases.conf
0a1e1afa580751e80bf26057b65fadffe269c0552e7a1903de498f94973ba3da8453b51f25e649968ca5f4841266f5ccf951700fa28465a8614b83d07344de60  blacklist.conf
49109d434b577563849c43dd8141961ca798dada74d4d3f49003dac1911f522c43438b8241fa254e4faacdd90058f4d39a7d69b1f493f6d57422c1f706547c95  i386.conf
9dda8c9d1896baf1217aa05ae2936e909300a22a98da9f4c3ba29136852477bf4764321b6a1abb15e93ee58f4a6e77ddfc42cbb12cbbb53cf0f431ace444f72f  kms.conf
b5eb01165c714861e860f17c0156911ff882a9010306b7fc4cdb22251acf8b1c91a3fa1d44cc41cb3d9b50892e2f98f43da57b002c5c33200c1bf49c3d2d587d  group
37d7b8348e604b12c055d9d7e79afb568ededea7153ff552c9f383cffd537d9c78cfd9facd612d2a6753fc626ff608a6d22d62637585a33166aa28f59fabed22  inittab
f0d12f365839e7e262ec91e151119de7f2f253e9d0443157de4d52e183f421fbbb9eb0a83b9267a9ee850bebe41bff3c3cef553f9bda6e70d59a754a955be57d  passwd
4eb857ed59c2edb257636d2bf196989e514a273e9701e9f076c9ae8c1589b4898269180569960acf072c0981ec7ea54014fd230f014401d6bb92314285d1e6aa  profile
3a00083bcdf5a9e884c9d07877d52311e3d99e79cbee656e236ba06e08ba0dddb7ba76494fdc9dd1a826c48e197a790a69e6bb458e9df64832d6b5e904e9fd15  protocols-6.4
47b0f3ee73af2d259bd206a026204be0ea25531a895a0b035a904b38fe5407bc3dd2beab7f8fcb3d760587e6159702ebdb9cbc4f508942befdf7f10c10c87888  services-6.4
"
