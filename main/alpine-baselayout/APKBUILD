# Contributor: Sören Tempel <soeren+alpine@soeren-tempel.net>
# Maintainer: Natanael Copa <ncopa@alpinelinux.org>
pkgname=alpine-baselayout
pkgver=3.0.3
pkgrel=2
pkgdesc="Alpine base dir structure and init scripts"
url="http://git.alpinelinux.org/cgit/aports/tree/main/alpine-baselayout"
arch="all"
license="GPL2"
depends=""
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

_builddir="$srcdir"/build
prepare() {
	mkdir -p "$_builddir"
}

build() {
	cd "$_builddir"
	${CC:-${CROSS_COMPILE}gcc} $CPPFLAGS $CFLAGS $LDFLAGS \
		"$srcdir"/mkmntdirs.c -o "$_builddir"/mkmntdirs

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
		var/empty \
		var/lib \
		var/lib/misc \
		var/local \
		var/lock/subsys \
		var/log \
		var/opt \
		var/run \
		var/spool \
		var/spool/cron \
		|| return 1

	install -d -m 0700 "$pkgdir"/root || return 1
	install -d -m 1777 "$pkgdir"/tmp "$pkgdir"/var/tmp || return 1
	install -m755 "$_builddir"/mkmntdirs "$pkgdir"/sbin/mkmntdirs \
		|| return 1

	install -m600 "$srcdir"/crontab "$pkgdir"/etc/crontabs/root || return 1
	install -m644 "$srcdir"/color_prompt "$pkgdir"/etc/profile.d/ \
		|| return 1
	install -m644 \
		"$srcdir"/aliases.conf \
		"$srcdir"/blacklist.conf \
		"$srcdir"/i386.conf \
		"$srcdir"/kms.conf \
		"$pkgdir"/etc/modprobe.d/ || return 1

	echo "UTC" > "$pkgdir"/etc/TZ
	echo "localhost" > "$pkgdir"/etc/hostname
	echo "127.0.0.1	localhost localhost.localdomain" > "$pkgdir"/etc/hosts
	echo "af_packet" >"$pkgdir"/etc/modules

	cat > "$pkgdir"/etc/shells <<EOF
# valid login shells
/bin/sh
/bin/ash
EOF

	cat > "$pkgdir"/etc/motd <<EOF
Welcome to Alpine!

The Alpine Wiki contains a large amount of how-to guides and general
information about administrating Alpine systems.
See <http://wiki.alpinelinux.org>.

You can setup the system with the command: setup-alpine

You may change this message by editing /etc/motd.

EOF
	cat > "$pkgdir"/etc/sysctl.conf <<EOF
# content of this file will override /etc/sysctl.d/*
EOF
	cat > "$pkgdir"/etc/sysctl.d/00-alpine.conf <<EOF
net.ipv4.tcp_syncookies = 1
net.ipv4.conf.default.rp_filter = 1
net.ipv4.conf.all.rp_filter = 1
net.ipv4.ping_group_range=999 59999
kernel.panic = 120
EOF
	cat > "$pkgdir"/etc/fstab <<EOF
/dev/cdrom	/media/cdrom	iso9660	noauto,ro 0 0
/dev/usbdisk	/media/usb	vfat	noauto,ro 0 0
EOF

	install -m644 \
		"$srcdir"/group \
		"$srcdir"/passwd \
		"$srcdir"/inittab \
		"$srcdir"/profile \
		"$srcdir"/protocols \
		"$srcdir"/services \
		"$pkgdir"/etc/ || return 1

	install -m640 -g shadow "$_builddir"/shadow \
		"$pkgdir"/etc/ || return 1

	# symlinks
	ln -s /etc/crontabs "$pkgdir"/var/spool/cron/crontabs
	ln -s /proc/mounts "$pkgdir"/etc/mtab

}
md5sums="db115e3302157a836a746bfd1d3f39be  mkmntdirs.c
6e67923e98287d5133b565acd44cb506  crontab
d7da26e76752b2e31c66b63f40a73397  color_prompt
6945faabb2a18cee73c0a6d78b499084  aliases.conf
0ea6be55b18804f166e6939e9d5f4099  blacklist.conf
f9e3eac60200d41dd5569eeabb4eddff  i386.conf
17ba4f49c51dd2ba49ccceb0ecadaca4  kms.conf
e76b843364cfde89db77f838e4af9ee3  group
e16a056e48e7003b7aa08cb4034d3228  inittab
5d620e5b17afaa6d66dc1044780a0a07  passwd
2939a40b911cac04c85263072c663973  profile
394f8cd9fbf549f1d1b9a5bba680fc92  protocols
5278bea4f45f4e289f72897b84dcb909  services"
sha256sums="44ca046e8188a4c9d32577470c1cbb864fe20889a3c4e6309d1d665c4c13a852  mkmntdirs.c
575d810a9fae5f2f0671c9b2c0ce973e46c7207fbe5cb8d1b0d1836a6a0470e3  crontab
a00b56dbd437d3f2c32ced50974daa3cfc84a8dd1cbaf75cf307be20b398fc75  color_prompt
3e2daa0326fc4d73921cd693dff47b211d0ba3d92ad62fca072c259600695693  aliases.conf
70becab743ff2071247bd144499eadbb1b42a5436dcc63991d69aa63ee2fe755  blacklist.conf
6c46c4cbfb8b7594f19eb94801a350fa2221ae9ac5239a8819d15555caa76ae8  i386.conf
079f74b93a4df310f55f60fea5e05996d3267c50076ae16402fa9e497ea5fdb2  kms.conf
132d13f9260edf201efd1400b830ec39d25769c1a5b5a5dc74acdfed126f9d0a  group
54a5f36970125bf70cdf7b215c9e12a287d92ad76a693bd72aec4cbc5645df87  inittab
3dcaeb5a9ee2cf8808565a7108aafe4f97ffdc47dbaac5c3eac4e68ff31b6176  passwd
8ef550a4efc4bed1630b47c48aefbf1c63ae6e8d9c1246be92135465965712d2  profile
a6695dbd53b87c7b41dfdafd40b1c8ba34fed2f0fa8eaaa296ad17c0b154603e  protocols
e96af627f7774e8c87b0de843556a355fea6150c4d64fa4e2ff2c5fd610e7a79  services"
sha512sums="94a75cbd309e7b7d3b1bbbddb6f361335336d804c2a331b9963de7cee38633217bcd533b4b29a4333ba477c6a13fe9a160c822108027a03a84b334bc76bf8ebd  mkmntdirs.c
6e169c0975a1ad1ad871a863e8ee83f053de9ad0b58d94952efa4c28a8c221445d9e9732ad8b52832a50919c2f39aa965a929b3d5b3f9e62f169e2b2e0813d82  crontab
7fcb5df98b0f19e609cb9444b2e6ca5ee97f5f308eb407436acdd0115781623fd89768a9285e9816e36778e565b6f27055f2a586a58f19d6d880de5446d263c4  color_prompt
8d55acb0457eec4008ba09c3de92fed6893b72d062edd5dcd74016e9c6c4dfeba7e86620e59d21a7adce11377ec793812c90e9d9771a804097fa64cff646666c  aliases.conf
2b8e55339955c9670b5b9832bf57e711aca70cd2ebf815a9623fbb7fcd440cca4dd6a4862750885f779080d5c5416de197ff9a250cf116b1c8cf130fafbdaae8  blacklist.conf
49109d434b577563849c43dd8141961ca798dada74d4d3f49003dac1911f522c43438b8241fa254e4faacdd90058f4d39a7d69b1f493f6d57422c1f706547c95  i386.conf
b407351a5a64b00100753a13a91f4b1cb51017ae918a91fd37f3a6e76e3b6f562be643e74f969a888bdd54b0ad2d09e3b283d44ae4b5efccca7d7e9f735c5afb  kms.conf
e6775b9e1c6421338aaceee375b3b74aa100fd444e369b280ce45c9167119b76bebc11737d7f929e50e20a553a35e0e25f7d0f71deb0483d3bccc08e319dcf98  group
fdab6f8fec2a556ab817d90a73635a927ea04dbc4e0470ed59ee6a62c87393f9534c9b746b09a776d938c25b8af9c9fb1686578e24f8307d1d074921ade1bdc7  inittab
17c40af54f29daf542d36da1947913ea9e14af94656efc3e30b9ec2d22d12e3cf3b456466904cf56bc99412e0f650c7a0187c706820da68ce0c99253e53f6338  passwd
c4088a7148c0f161809852d248d2c2272d9c72be3f968c2e2ba40806f508238496eda0f8f2a42aa092773a56800b1dae9f843a42d93f1bb16ba5f58c111d531b  profile
f1548a2b5a107479446f15905f0f2fbf8762815b2215188d49d905c803786d35de6d98005dc0828fb2486b04aaa356f1216a964befddf1e72cb169656e23b6ac  protocols
cecfc06b1f455d65b0c54a5651e601298b455771333e39d0109eeffd7ebd8d81b7738738eb647e6d3076230b6f3707782b83662ea3764ec33dc5e0b3453d3965  services"
