# Copyright 1999-2017 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

# Configuration for /etc/init.d/squeezelite

# IP address of Logitech Media Server; leave this blank to try to
# locate the server via auto-discovery.
# If you fill this in then include "-s" before the IP address, eg:
#  SL_SERVER_IP="-s 1.2.3.4"
SL_SERVERIP=""

# User that Squeezelite should run as. The dedicated 'squeezelite'
# user is preferred to avoid running with high privilege. This user
# should be a member of the 'audio' group to allow access to the audio
# hardware. Running as the 'root' user allows the sound output thread
# to run at a very high priority -- this can help avoid gaps in
# playback, but could be a potential security problem if there are
# exploitable vulnerabilities in Squeezelite.
SL_USER=squeezelite

# Any other switches to pass to Squeezelite. See 'squeezelite -h' for
# a description of all possible switches.

# Example setting:
#  1. the ALSA output device
#  2. the player name
#  3. turning on visualiser support (-v)
#
# SL_OPTS="-o sysdefault -n $HOSTNAME -v"
#
SL_OPTS=""
