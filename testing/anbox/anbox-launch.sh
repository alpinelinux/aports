#!/bin/sh

# https://gitlab.com/postmarketOS/pmaports/-/issues/479
# EGL_PLATFORM=wayland is broken on desktop as
# Anbox require PBuffer support but Wayland EGL
# doesn't provide it. Unsetting this value
# unbreaks Anbox on e.g. Plasma Mobile.
if [ "$EGL_PLATFORM" = wayland ]; then
	export EGL_PLATFORM
	unset EGL_PLATFORM
fi

# This breaks Anbox display if EGL_PLATFORM is not
# set to wayland. Since EGL_PLATFORM is never set
# to wayland, let's unset SDL_VIDEODRIVER if it is.
if [ "$SDL_VIDEODRIVER" = wayland ]; then
	export SDL_VIDEODRIVER
	unset SDL_VIDEODRIVER
fi

# We let Anbox autostart the session manager as this
# provides a splash-screen
anbox launch --package=org.anbox.appmgr --component=org.anbox.appmgr.AppViewActivity
