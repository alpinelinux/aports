#!/bin/sh

if [ -S /run/udev/control ]; then
	udevadm control --reload
fi

exit 0
