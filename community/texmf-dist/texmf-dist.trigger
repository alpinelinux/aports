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

msg "updating language config"
update_lang_config > /dev/null 2>&1
print_result "$?"

exit $error
