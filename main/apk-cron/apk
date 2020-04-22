#!/bin/sh

# SPDX-License-Identifier: GPL-2.0-only
#
# cron job for automatic software updates
# Copyright (c) 2014 Kaarle Ritvanen

set -eu

sleep $(expr $RANDOM % 7200)
exec apk -U upgrade
