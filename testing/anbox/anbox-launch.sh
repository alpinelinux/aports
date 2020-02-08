#!/bin/sh

#if [ -z "$(ps aux | grep 'anbox session-manager' | grep -v grep)" ]; then
#	anbox session-manager &
#	sleep 5s
#fi

# We let Anbox autostart the session manager as this provides a splash-screen
anbox launch --package=org.anbox.appmgr --component=org.anbox.appmgr.AppViewActivity
