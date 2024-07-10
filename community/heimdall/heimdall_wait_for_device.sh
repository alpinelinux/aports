#!/bin/sh
# Simple wrapper, that makes heimdall behave more like fastboot

echo "DEPRECATION WARNING: heimdall_wait_for_device.sh is deprecated and will"
echo "be removed in a future version. Please use heimdall <action> --wait"
echo "instead."
echo "< wait for any device >"
while ! heimdall detect > /dev/null 2>&1; do
	sleep 1
done
