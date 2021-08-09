#!/bin/sh

# We need to kill any existing pipewire instance to restore sound
pkill -u "${USER}" -x harbour-amazfishd 1>/dev/null 2>&1

exec /usr/bin/harbour-amazfishd
