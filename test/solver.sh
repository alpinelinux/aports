#!/bin/sh

APK_TEST=../src/apk_test

fail=0
for test in *.test; do
	bn=$(basename $test .test)
	$APK_TEST $(cat $test) &> $bn.got
	if ! cmp $bn.expect $bn.got 2> /dev/null; then
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

