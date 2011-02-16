#!/bin/sh

dbus-send --system --type=method_call --dest=org.freedesktop.DBus / \
	org.freedesktop.DBUS.ReloadConfig >/dev/null 2>&1 || :
