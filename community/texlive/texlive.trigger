#!/bin/sh
error=0

msg() {
    printf "%-50s" " --> $1"
}

print_result() {
    if [ "$1" -ne 0 ]; then
        echo "[FAILED]"
        error=1
    else
        echo "[OK]"
    fi
}

update_lang_config() {
    retval=0

    cat $(find /usr/share/texmf-dist/tex/generic/config/lang.d/ -name '*.def' | sort) \
        > /usr/share/texmf-dist/tex/generic/config/language.def || retval=1

    cat $(find /usr/share/texmf-dist/tex/generic/config/lang.d/ -name '*.dat' | sort) \
        > /usr/share/texmf-dist/tex/generic/config/language.dat || retval=1

    cat $(find /usr/share/texmf-dist/tex/generic/config/lang.d/ -name '*.dat.lua' | sort) \
        > /usr/share/texmf-dist/tex/generic/config/language.dat.lua || retval=1

    return "$retval"
}

if [ ! -f /usr/share/texmf-dist/tex/generic/config/lang.d/00-preamble.dat ]; then
    # This can only happen right after uninstalling texlive. In this case, no
    # language config will be present and update_lang_config would call cat
    # without arguments, and block waiting for input on stdin.
    echo "SKIPPED"
    exit 0
fi

msg "updating language config"
update_lang_config > /dev/null 2>&1
print_result "$?"

msg "mktexlsr"
mktexlsr > /dev/null 2>&1
print_result "$?"

if [ -f /usr/share/texmf-config/web2c/updmap.cfg ]; then
    mv -f /usr/share/texmf-config/web2c/updmap.cfg /usr/share/texmf-config/web2c/updmap.cfg.bak
    echo "updmap.cfg backed up as /usr/share/texmf-config/web2c/updmap.cfg.bak"
fi

msg "updmap-sys --syncwithtrees"
yes 2> /dev/null | updmap-sys --syncwithtrees > /dev/null 2>&1
print_result "$?"

msg "updmap-sys"
updmap-sys > /dev/null 2>&1
print_result "$?"

echo " --> fmtutil-sys --all"
echo "Note: Trying to generate all formats regardless of what is installed."
echo "      Hence, unless texlive-full is installed, some formats will fail"
echo "      to generate."
fmtutil-sys --all  2>&1 | tail -n 5

exit $error
