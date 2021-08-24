#!/bin/sh

# We need to kill any existing pipewire instance to restore sound
pkill -u "${USER}" -x pipewire 1>/dev/null 2>&1
pkill -u "${USER}" -fx "pipewire -c pipewire-pulse.conf" 1>/dev/null 2>&1
pkill -u "${USER}" -fx /usr/bin/pipewire-media-session 1>/dev/null 2>&1

exec /usr/bin/pipewire &

# wait for pipewire to start before attempting to start related daemons
while [ "$(pgrep -f /usr/bin/pipewire)" = "" ]; do
        sleep 1
done

[ -x /usr/bin/pipewire-media-session ]  && exec /usr/bin/pipewire-media-session &

[ -f "/usr/share/pipewire/pipewire-pulse.conf" ] && exec pipewire -c pipewire-pulse.conf &
