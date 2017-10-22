#!/bin/sh

nxtool=/etc/nginx/nxapi/nxtool.py

if [ -f $nxtool ]; then
        cd $(dirname $nxtool)
        $nxtool $@
else
        printf "ERROR: missing => $nxtool\n"
fi
