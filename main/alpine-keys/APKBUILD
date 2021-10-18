# Maintainer: Natanael Copa <ncopa@alpinelinux.org>
pkgname=alpine-keys
pkgver=2.4
pkgrel=1
pkgdesc="Public keys for Alpine Linux packages"
url="https://alpinelinux.org"
# we install arch specific keys to /etc so we cannot do arch=noarch
arch="all"
license="MIT"
replaces="alpine-base"
options="!check" # No testsuite

_arch_keys="
	aarch64:alpine-devel@lists.alpinelinux.org-58199dcc.rsa.pub
	aarch64:alpine-devel@lists.alpinelinux.org-616ae350.rsa.pub
	armhf,armv7:alpine-devel@lists.alpinelinux.org-524d27bb.rsa.pub
	armv7:alpine-devel@lists.alpinelinux.org-616adfeb.rsa.pub
	armhf:alpine-devel@lists.alpinelinux.org-616a9724.rsa.pub

	x86:alpine-devel@lists.alpinelinux.org-5243ef4b.rsa.pub
	x86:alpine-devel@lists.alpinelinux.org-61666e3f.rsa.pub
	x86,x86_64:alpine-devel@lists.alpinelinux.org-4a6a0840.rsa.pub
	x86_64:alpine-devel@lists.alpinelinux.org-5261cecb.rsa.pub
	x86_64:alpine-devel@lists.alpinelinux.org-6165ee59.rsa.pub

	ppc64le:alpine-devel@lists.alpinelinux.org-58cbb476.rsa.pub
	ppc64le:alpine-devel@lists.alpinelinux.org-616abc23.rsa.pub

	s390x:alpine-devel@lists.alpinelinux.org-58e4f17d.rsa.pub
	s390x:alpine-devel@lists.alpinelinux.org-616ac3bc.rsa.pub

	mips64:alpine-devel@lists.alpinelinux.org-5e69ca50.rsa.pub

	riscv64:alpine-devel@lists.alpinelinux.org-60ac2099.rsa.pub
	riscv64:alpine-devel@lists.alpinelinux.org-616db30d.rsa.pub
	"

for _i in $_arch_keys; do
	source="$source ${_i#*:}"
done

_ins_key() {
	msg "- $2 ($1)"
	install -Dm644 "$srcdir"/$2 "$pkgdir"/etc/apk/keys/$2
}

_install_x86() {
	case "$1" in
	x86*) _ins_key $1 $2 ;;
	esac
}

_install_arm() {
	case "$1" in
	aarch64|arm*) _ins_key $1 $2 ;;
	esac
}

_install_ppc() {
	case "$1" in
	ppc*) _ins_key $1 $2 ;;
	esac
}

_install_s390x() {
	case "$1" in
	s390x) _ins_key $1 $2 ;;
	esac
}

_install_mips() {
	case "$1" in
	mips64) _ins_key $1 $2 ;;
	esac
}

_install_riscv() {
	case "$1" in
	riscv*) _ins_key $1 $2 ;;
	esac
}

package() {
	# copy keys for repos
	mkdir -p "$pkgdir"/etc/apk/keys
	for i in $_arch_keys; do
		_archs="${i%:*}"
		_key="${i#*:}"
		install -Dm644 "$srcdir"/$_key \
			"$pkgdir"/usr/share/apk/keys/$_key

		for _arch in ${_archs//,/ }; do
			mkdir -p "$pkgdir"/usr/share/apk/keys/$_arch
			ln -s ../$_key "$pkgdir"/usr/share/apk/keys/$_arch/

			case "$CARCH" in
			x86*) _install_x86 $_arch $_key ;;
			arm*|aarch64) _install_arm $_arch $_key ;;
			ppc*) _install_ppc $_arch $_key ;;
			s390x) _install_s390x $_arch $_key ;;
			mips*) _install_mips $_arch $_key ;;
			riscv*) _install_riscv $_arch $_key ;;
			esac
		done
	done
}

sha512sums="
e4f9e314f8e506fba2cb3e599c6412a036ec37ce3a54990fc7d80a821d8728f40ee3b4aa8a15218d50341fa785d9ddf7c7471f45018c6a2065ab13664a1aa9e9  alpine-devel@lists.alpinelinux.org-58199dcc.rsa.pub
51a5ec21283fe218809b2325202e1f8c9b2551705db48254b9d48a04f4ed0075de51e9886c4704647ffb309fd32d9850d14013848a53038039e85011251fe1cc  alpine-devel@lists.alpinelinux.org-616ae350.rsa.pub
698fda502f70365a852de3c10636eadfc4f70a7a00f096581119aef665e248b787004ceef63f4c8cb18c6f88d18b8b1bd6b3c5d260e79e6d73a3cc09537b196e  alpine-devel@lists.alpinelinux.org-524d27bb.rsa.pub
a98095a626f2dcbda73ffd8873ba2d609ee1d881f5da13b0eb3469ddd58b06440b4b0b2f791b037c88073e9a17c6dfc62dc1a4c8491bed871524d772ef04ad24  alpine-devel@lists.alpinelinux.org-616adfeb.rsa.pub
7aa5526a88519ae91f997bf914a9bd3d230b21c011587f155ce22c4bb94b70181b28590027eb555d96d1122dffb8242c1fb044228e99b4e9b7650fcf6f5121c7  alpine-devel@lists.alpinelinux.org-616a9724.rsa.pub
e18e65ee911eb1f8ea869f758e8f2c94cf2ac254ee7ab90a3de1d47b94a547c2066214abf710da21910ebedc0153d05fd4fe579cc5ce24f46e0cfd29a02b1a68  alpine-devel@lists.alpinelinux.org-5243ef4b.rsa.pub
b89d825e6af73687339848817791b294e2404162e2e069d9212d76d4ee53d6216eb75421a07b02f9778ef57dbb27962b2436247264eea1a1d882967ca0c18724  alpine-devel@lists.alpinelinux.org-61666e3f.rsa.pub
2d4064cbe09ff958493ec86bcb925af9b7517825d1d9d8d00f2986201ad5952f986fea83d1e2c177e92130700bafa8c0bff61411b3cdb59a41e460ed719580a6  alpine-devel@lists.alpinelinux.org-4a6a0840.rsa.pub
721134f289ab1e7dde9158359906017daee40983199fe55f28206c8cdc46b8fcf177a36f270ce374b0eba5dbe01f68cbb3e385ae78a54bb0a2ed1e83a4d820a5  alpine-devel@lists.alpinelinux.org-5261cecb.rsa.pub
8b9c2208c904c9f34d9d01d3d68b224208530e684265df214deb8c9e6b4b19633aa48a405e673249c9e93a8ee194a336e951cd82a4e27e5e66e85fdc5e0d495e  alpine-devel@lists.alpinelinux.org-6165ee59.rsa.pub
bb5a3df8fac14a62d5936fb3722873fa6a121219b703cba955eb77de38c4384aeaf378fb9321a655e255f0be761e894e309b3789867279c1524dab6300cd8ef1  alpine-devel@lists.alpinelinux.org-58cbb476.rsa.pub
bad4da65221150a5d4cc6f63981e4dd203d40844d32e82c17f346eee5350e460e32d28f0e231a2b78d326ec32b898eec597d3787dae47dcacc9a9776d19fb4a1  alpine-devel@lists.alpinelinux.org-616abc23.rsa.pub
0666389ca53121453578cd4bef5fd06e159e291164b3e3233e7d6521604f8bebd30caeef1663adcd5309e07278833402c8a92c33294ec0c5cada24dc47c8cc98  alpine-devel@lists.alpinelinux.org-58e4f17d.rsa.pub
83fc29066f6073418ecf01176ce24c1c0e788508f3083a97691706e2c78323e53448060fb0d2abb8118a759570f1f0db9d39953c63fe26fe06da2be05dff393c  alpine-devel@lists.alpinelinux.org-616ac3bc.rsa.pub
66ce9677e9c2a7961d5d7bc5b162ed3114a7aef6d01181073c1f42a9934966eecded2ec09deb210f5a389d434d1641ba35fe3abdd5246b2e97d5a5b26a945c5c  alpine-devel@lists.alpinelinux.org-5e69ca50.rsa.pub
34514100e502f449dcabe0aa550232c3330ed2f0b789b977eb228d4ac86afc93479474ac005914992a3b47c18ee3eb32ca27ccd0d392700a8f11f47d64a78969  alpine-devel@lists.alpinelinux.org-60ac2099.rsa.pub
7cea57204a50d72bddff201c509ccbf06773d87062a3ead0a206cc6e4a00e0960f52d21f7cee7aaec6a4abba7a697e2e2e7f630fa1ccef7ee2c33908fca18998  alpine-devel@lists.alpinelinux.org-616db30d.rsa.pub
"
