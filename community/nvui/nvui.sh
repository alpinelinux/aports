#!/bin/sh
# XXX: Set LANG=C to avoid error locale::facet::_S_create_c_locale name not valid.
LANG=C exec /usr/libexec/nvui --detached -- "$@"
