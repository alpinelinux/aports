# Maintainer: Natanael Copa <ncopa@alpinelinux.org>
pkgname=alpine-baselayout
pkgver=2.3.1
pkgrel=0
pkgdesc="Alpine base dir structure and init scripts"
url="http://git.alpinelinux.org/cgit/aports/tree/main/alpine-baselayout"
depends=
options="!fhs"
install="$pkgname.pre-upgrade $pkgname.post-upgrade $pkgname.post-install"
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
arch="all"
license=GPL-2

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
		etc/profile.d \
		etc/network/if-down.d \
		etc/network/if-post-down.d \
		etc/network/if-pre-up.d \
		etc/network/if-up.d \
		etc/periodic/15min \
		etc/periodic/daily \
		etc/periodic/hourly \
		etc/periodic/monthly \
		etc/periodic/weekly \
		home \
		lib/firmware \
		lib/mdev \
		media/cdrom \
		media/floppy \
		media/usb \
		mnt \
		proc \
		sbin \
		sys \
		usr/bin \
		usr/sbin \
		usr/local/bin \
		usr/local/lib \
		usr/local/share \
		usr/share \
		var/cache/misc \
		var/lib/misc \
		var/lock/subsys \
		var/log \
		var/run \
		var/spool/cron \
		run \
		|| return 1

	install -d -m 0700 "$pkgdir"/root || return 1
	install -d -m 1777 "$pkgdir"/tmp "$pkgdir"/var/tmp || return 1
	install -m755 "$_builddir"/mkmntdirs "$pkgdir"/sbin/mkmntdirs \
		|| return 1

	install -m644 "$srcdir"/crontab "$pkgdir"/etc/crontabs/root || return 1
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

You may change this message by editing /etc/motd.

EOF
	cat > "$pkgdir"/etc/sysctl.conf <<EOF
net.ipv4.ip_forward = 0
net.ipv4.tcp_syncookies = 1
net.ipv4.conf.default.rp_filter = 1
net.ipv4.conf.all.rp_filter = 1
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

	install -m600 "$_builddir"/shadow \
		"$pkgdir"/etc/ || return 1

	# symlinks
	ln -s /etc/crontabs "$pkgdir"/var/spool/cron/crontabs
	ln -s /proc/mounts "$pkgdir"/etc/mtab

}
md5sums="686863b10c1cfdf07322ea0ecc977da5  mkmntdirs.c
6e67923e98287d5133b565acd44cb506  crontab
a658b219ac3dd3199ef4092f6be3c9da  color_prompt
6945faabb2a18cee73c0a6d78b499084  aliases.conf
0ea6be55b18804f166e6939e9d5f4099  blacklist.conf
f9e3eac60200d41dd5569eeabb4eddff  i386.conf
17ba4f49c51dd2ba49ccceb0ecadaca4  kms.conf
286a137ba3b0a79dac2ff1b5da0d8110  group
d93ca19f6867a1a3918fa782d9b7c512  inittab
d05579984e737021401d8c40153c5b5d  passwd
dcb55fbc491316cdb3874f3ddae8f566  profile
394f8cd9fbf549f1d1b9a5bba680fc92  protocols
5278bea4f45f4e289f72897b84dcb909  services"
sha256sums="9c5f25b07bfc28fff4232b5fd64fce45dd10ddd0f4a59741f2ca74ff99e4db3e  mkmntdirs.c
575d810a9fae5f2f0671c9b2c0ce973e46c7207fbe5cb8d1b0d1836a6a0470e3  crontab
79698d2a91fa608954122b7e39eca67bf1e8aaba93ad216f70ef88c903e417b9  color_prompt
3e2daa0326fc4d73921cd693dff47b211d0ba3d92ad62fca072c259600695693  aliases.conf
70becab743ff2071247bd144499eadbb1b42a5436dcc63991d69aa63ee2fe755  blacklist.conf
6c46c4cbfb8b7594f19eb94801a350fa2221ae9ac5239a8819d15555caa76ae8  i386.conf
079f74b93a4df310f55f60fea5e05996d3267c50076ae16402fa9e497ea5fdb2  kms.conf
b62991a782171451461b42c182722f1a165cb4f1638005f233547cecd394a4da  group
8a54b20451ba9469b5260f76faa90fbde96915bf33e7f03f88bdbaf813400485  inittab
348e451c8d4d2d9807d78114d100942f1c5481fd52550c821922b84a16f02d42  passwd
8165abfa26a1754c317228e78142f4a89dd72a669951d269f0b68d7a92faf1e8  profile
a6695dbd53b87c7b41dfdafd40b1c8ba34fed2f0fa8eaaa296ad17c0b154603e  protocols
e96af627f7774e8c87b0de843556a355fea6150c4d64fa4e2ff2c5fd610e7a79  services"
sha512sums="670fce7a9d5c492289b7dca06077a5109672fbec2accf21bfb3531651f0434939ff07e71c61949df2c0bfe3c2f178a503f0086867b84e3874abd5c90233149b7  mkmntdirs.c
6e169c0975a1ad1ad871a863e8ee83f053de9ad0b58d94952efa4c28a8c221445d9e9732ad8b52832a50919c2f39aa965a929b3d5b3f9e62f169e2b2e0813d82  crontab
9ca0da660eec586efba538410de04d3d76e99c069fadf0811bfb0a5da0bb8432b8ea5fbb8e530b7f71eb97b68dbe84dbbf67763252069433ce8c56a738d434a6  color_prompt
8d55acb0457eec4008ba09c3de92fed6893b72d062edd5dcd74016e9c6c4dfeba7e86620e59d21a7adce11377ec793812c90e9d9771a804097fa64cff646666c  aliases.conf
2b8e55339955c9670b5b9832bf57e711aca70cd2ebf815a9623fbb7fcd440cca4dd6a4862750885f779080d5c5416de197ff9a250cf116b1c8cf130fafbdaae8  blacklist.conf
49109d434b577563849c43dd8141961ca798dada74d4d3f49003dac1911f522c43438b8241fa254e4faacdd90058f4d39a7d69b1f493f6d57422c1f706547c95  i386.conf
b407351a5a64b00100753a13a91f4b1cb51017ae918a91fd37f3a6e76e3b6f562be643e74f969a888bdd54b0ad2d09e3b283d44ae4b5efccca7d7e9f735c5afb  kms.conf
e732d715968a4dc3bf6492964080fb71df900f3fbc6ca0ed11f114ca66ec01c18501e4a9978c85eee133c6249a9c2ecb327c473397d05e2f1ad2b5a950cfa632  group
bdb6fe1f3e07466d9badf9a18b1f3fc9585f0a4cb69c00f1fa6d8b1e9903a3c497edefbfd345e324961013999e5692d2ed68ae7defaf1c6e57cc7916f206fd75  inittab
3b9be283e171516dd45ee29775a1149ba20d5672bef422a698ed944ce66697e5a80dec21a9a20c079cca895890dbfa73ccbb8000a55504d95ced4e7436d8e16b  passwd
9e197ec99c97ec2b03b97546d632a865d6e7abe5742ac48bcb458a505ce38abd1010ef93ea8a752251578ea993708324e1d36ccf77ee3e7cde7e8db98cc42d67  profile
f1548a2b5a107479446f15905f0f2fbf8762815b2215188d49d905c803786d35de6d98005dc0828fb2486b04aaa356f1216a964befddf1e72cb169656e23b6ac  protocols
cecfc06b1f455d65b0c54a5651e601298b455771333e39d0109eeffd7ebd8d81b7738738eb647e6d3076230b6f3707782b83662ea3764ec33dc5e0b3453d3965  services"
