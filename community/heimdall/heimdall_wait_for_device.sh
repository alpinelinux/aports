#!/bin/sh
# Simple wrapper, that makes heimdall behave more like fastboot

echo "< wait for any device >"
while ! heimdall detect > /dev/null 2>&1; do
	sleep 1
done
