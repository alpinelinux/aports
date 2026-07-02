#!/bin/sh -eu

export NODE_ENV=production
exec electron /usr/lib/drawio-desktop/app.asar "$@"
