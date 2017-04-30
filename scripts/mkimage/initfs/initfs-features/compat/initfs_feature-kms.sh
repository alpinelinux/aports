# Initifs feature kms.

initfs_feature_kms() { return 0 ; }

_initfs_feature_kms_modules() {
	cat <<-'EOF'
		kernel/drivers/char/agp/
		kernel/drivers/gpu/
		kernel/drivers/i2c/
		kernel/drivers/video/
		fbdev
	EOF
}

_initfs_feature_kms_pkgs() {
	cat <<-'EOF'
		alpine-baselayout
	EOF
}

_initfs_feature_kms_files() {
	cat <<-'EOF'
		etc/modprobe.d/kms.conf
	EOF
}

_initfs_feature_kms_hostcfg() {
	cat <<-'EOF'
		etc/modprobe.d/kms.conf
	EOF
}

