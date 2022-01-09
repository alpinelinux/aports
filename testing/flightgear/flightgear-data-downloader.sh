#!/bin/sh

FGDATA_VERSION=2020.3.11
RELEASE_BRANCH=${FGDATA_VERSION%.*}

echo "Downloading Flightgear data (version $FGDATA_VERSION) ..."
mkdir /var/lib/flightgear
curl -L https://sourceforge.net/projects/flightgear/files/release-$RELEASE_BRANCH/FlightGear-$FGDATA_VERSION-data.txz/download | tar -xJ --strip-components=1 -C /var/lib/flightgear -f -
echo 'Done.'
