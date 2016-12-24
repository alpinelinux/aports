#!/bin/sh

find -L /usr/bin -type l -name "*php*" -delete

for file in $(find /usr/bin -type f -name "*php5*"); do
	[ ! -e ${file/5/} ] && ln -sf $file ${file/5/}
done

exit 0
