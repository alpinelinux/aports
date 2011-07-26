#!/bin/sh

fail=0
cat version.data | while read a result b rest ; do
	output="$(../src/apk version -t "$a" "$b")"
	if [ "$output" != "$result" ] ; then
		echo "$a $result $b, but got $output"
		fail=$(($fail+1))
	fi
done

if [ "$fail" == "0" ]; then
	echo "OK: version checking works"
fi

exit $fail

