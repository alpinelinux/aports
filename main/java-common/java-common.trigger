#!/bin/sh

if [ -x /usr/lib/jvm/forced-jvm ]; then
	ln -sfn forced-jvm default-jvm
	exit 0
fi

cd /usr/lib/jvm
LATEST=`ls -d java-* | sort -r | head -1`
if [ "$LATEST" ]; then
	ln -sfn $LATEST default-jvm
fi

