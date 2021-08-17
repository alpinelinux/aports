#!/bin/sh

# Make sure we don't run osmscout-server twice
pkill -u "${USER}" -x osmscout-server # 1>/dev/null 2>&1

exec /usr/bin/osmscout-server --listen
