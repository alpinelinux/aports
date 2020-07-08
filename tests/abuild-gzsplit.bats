setup() {
	startdir=$PWD
	export ABUILD_GZSPLIT="$PWD/.."/abuild-gzsplit
	outdir="$PWD/tmp"
	mkdir -p $outdir
	datadir="$PWD/testdata"
	cd "$outdir"
}

teardown() {
	rm -f "$outdir"/control.tar.gz \
		"$outdir"/data.tar.gz \
		"$outdir"/signatures.tar.gz
	rmdir "$outdir"
	cd "$startdir"
}

@test "abuild-gzsplit: 3.11 package" {

	run $ABUILD_GZSPLIT < "$datadir"/alpine-base-3.11.6-r0.apk
	[ "$status" -eq 0 ]
}

@test "abuild-gzsplit: 3.12 package" {
	run $ABUILD_GZSPLIT < "$datadir"/alpine-base-3.12.0-r0.apk
	[ "$status" -eq 0 ]
}
