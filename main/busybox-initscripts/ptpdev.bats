setup() {
	mdevscript=${BATS_TEST_FILENAME%.bats}
	root="$BATS_FILE_TMPDIR"
	mkdir -p "$root"/dev "$root"/sys "$root"/bin
	PATH="$root/bin:$PATH"
	export SYSFS="$root/sys"

	mkdir -p "$root"/sys/class/ptp/ptp0 \
		"$root"/sys/class/ptp/ptp1 \
		"$root"/sys/class/ptp/ptp2

	echo "KVM virtual PTP" > "$root"/sys/class/ptp/ptp0/clock_name
	echo "ptp vmw" > "$root"/sys/class/ptp/ptp1/clock_name
	echo hyperv > "$root"/sys/class/ptp/ptp2/clock_name

	cd "$root"/dev
}

teardown() {
	rm -r "$root"
}

@test "ptpdev kvm" {
	MDEV=ptp0 ACTION=add sh $mdevscript
	[ $(readlink ptp_kvm) = ptp0 ]

	MDEV=ptp0 ACTION=remove sh $mdevscript
	run readlink ptp_kvm
	[ "$status" -ne 0 ]
}

@test "ptpdev vmw" {
	MDEV=ptp1 ACTION=add sh $mdevscript
	[ $(readlink ptp_vmw) = ptp1 ]

	MDEV=ptp1 ACTION=remove sh $mdevscript
	run readlink ptp_vmw
	[ "$status" -ne 0 ]
}

@test "ptpdev hyperv" {
	MDEV=ptp2 ACTION=add sh $mdevscript
	run readlink ptp_hyperv
	[ $(readlink ptp_hyperv) = ptp2 ]

	MDEV=ptp2 ACTION=remove sh $mdevscript
	run readlink ptp_hyperv
	[ "$status" -ne 0 ]
}
