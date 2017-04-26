# Initifs feature cryptsetup.

initfs_cryptsetup() {
	return 0
}

_initfs_cryptsetup_modules() {
	cat <<-'EOF'
		kernel/crypto/*
		kernel/arch/*/crypto/*
		kernel/drivers/md/dm-crypt.ko
	EOF
}

_initfs_cryptsetup_files() {
	cat <<-'EOF'
		/sbin/cryptsetup
	EOF
}
