#!/bin/sh

infodir='/usr/share/info'

rm -f "$infodir"/dir
find "$infodir" \( -name "*.info" -o -name "*.info.gz" \) \
	-exec install-info {} "$infodir"/dir \;
