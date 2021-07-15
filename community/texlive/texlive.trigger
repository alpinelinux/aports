#!/bin/sh
echo " --> updmap-sys --syncwithtrees"
yes 2> /dev/null | updmap-sys --syncwithtrees > /dev/null 2>&1 > /dev/null
echo " --> mktexlsr"
mktexlsr > /dev/null 2>&1 > /dev/null
echo " --> texhash"
texhash > /dev/null 2>&1 > /dev/null
echo " --> fmtutil-sys --all"
fmtutil-sys --all > /dev/null 2>&1 > /dev/null
echo " --> updmap-sys --force"
updmap-sys --force > /dev/null 2>&1 > /dev/null
exit 0
