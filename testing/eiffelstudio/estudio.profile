#! /usr/bin/sh

export ISE_EIFFEL=/usr/share/eiffelstudio-@PKGVER@
export ISE_PLATFORM=@ARCH@
export PATH="$PATH:${ISE_EIFFEL}/studio/spec/${ISE_PLATFORM}/bin"
