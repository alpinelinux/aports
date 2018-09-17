#!/bin/sh

Xvfb :4242 &
PID=$!
export DISPLAY=:4242
if ninja -C build meson-test; then
	kill $PID
	return 0
else
	kill $PID
	return 1
fi
