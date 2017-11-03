#!/bin/sh

Xvfb :4242 &
PID=$!
export DISPLAY=:4242
if make test COLOR=0; then
	kill $PID
	return 0
else
	kill $PID
	return 1
fi
