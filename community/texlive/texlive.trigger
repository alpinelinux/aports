#!/bin/sh
echo " --> generate language"
/usr/share/texmf-dist/scripts/texlive/tlmgr.pl generate language > /dev/null 2>&1
echo " --> mktexlsr"
mktexlsr > /dev/null 2>&1
if [ -f /usr/share/texmf-config/web2c/updmap.cfg ]; then
    mv -f /usr/share/texmf-config/web2c/updmap.cfg /usr/share/texmf-config/web2c/updmap.cfg.bak
    echo "updmap.cfg backed up as /usr/share/texmf-config/web2c/updmap.cfg.bak"
fi
echo " --> updmap-sys --syncwithtrees"
yes 2> /dev/null | updmap-sys --syncwithtrees > /dev/null 2>&1
echo " --> updmap-sys"
updmap-sys > /dev/null 2>&1
echo " --> fmtutil-sys --all"
fmtutil-sys --all > /dev/null 2>&1
exit 0
