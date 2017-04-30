# Initifs feature cdrom.

initfs_feature_cdrom() { return 0 ; }

_initfs_feature_cdrom_modules() {
	cat <<-'EOF'
		cdrom
		isofs
	EOF
}
