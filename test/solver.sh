#!/bin/sh

APK_TEST=../src/apk_test

fail=0
for test in *.test; do
	bn=$(basename $test .test)
	(
		read options
		read world
		$APK_TEST $options "$world" &> $bn.got
	) < $bn.test
	if ! cmp $bn.expect $bn.got &> /dev/null; then
		fail=$((fail+1))
		echo "FAIL: $test"
		diff -ru $bn.expect $bn.got
	else
		echo "OK: $test"
	fi
done

if [ "$fail" != "0" ]; then
	echo "FAIL: $fail failed test cases"
fi

exit $fail

