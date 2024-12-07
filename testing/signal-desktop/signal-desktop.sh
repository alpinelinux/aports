#!/bin/sh

# app chooses config (including used endpoints) based on this
export NODE_ENV=production

exec electron /usr/lib/signal-desktop/app.asar
