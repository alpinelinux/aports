#!/bin/sh

# Reload .mount files
busybox killall -USR1 gvfsd >&/dev/null
exit 0
