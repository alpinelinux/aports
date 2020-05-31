#!/bin/sh
# Copyright 2020 Antoine Fontaine
# SPDX-License-Identifier: GPL-3.0-or-later

# tiny wrapper that starts a notification dæmon (dunst)
# for the testsuite to talk to

dunst >/dev/null 2>&1 &
dunst_pid=$!

python3 test_notify2.py
success=$?

kill $dunst_pid

exit $success
