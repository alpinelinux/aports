#!/bin/sh

APK_TEST=../src/apk-test

fail=0
pass=0
for test in *.test; do
	awk '/^@ARGS/{p=1;next} /^@/{p=0} p{print}' < $test | xargs $APK_TEST &> .$test.got

	if ! awk '/^@EXPECT/{p=1;next} /^@/{p=0} p{print}' < $test | cmp .$test.got &> /dev/null; then
		fail=$((fail+1))
		echo "FAIL: $test"
		awk '/^@EXPECT/{p=1;next} /^@/{p=0} p{print}' < $test | diff -ru - .$test.got
	else
		pass=$((pass+1))
	fi
done

total=$((fail+pass))
if [ "$fail" != "0" ]; then
	echo "FAIL: $fail of $total test cases failed"
else
	echo "OK: all $total solver test cases passed"
fi

exit $fail

