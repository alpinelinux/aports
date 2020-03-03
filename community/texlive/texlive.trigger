#!/bin/sh
texhash > /dev/null 2>&1 > /dev/null
fmtutil-sys --all > /dev/null
exit 0
