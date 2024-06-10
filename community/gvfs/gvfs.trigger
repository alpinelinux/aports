#!/bin/sh

# Reload .mount files
busybox killall -q USR1 gvfsd
exit 0
