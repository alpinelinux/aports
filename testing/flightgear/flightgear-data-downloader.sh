#!/bin/sh

FGDATA_VERSION=%FGDATA_VERSION%
RELEASE_BRANCH=${FGDATA_VERSION%.*}

if [ "$(id -u)" != '0' ]; then
  echo "This script requires root privileges to store flightgear-data into /var/lib/flightgear folder."
  exit 1
fi

echo "Downloading Flightgear data (version $FGDATA_VERSION) ..."
rm -rf /var/lib/flightgear
mkdir /var/lib/flightgear
curl -L https://sourceforge.net/projects/flightgear/files/release-$RELEASE_BRANCH/FlightGear-$FGDATA_VERSION-data.txz/download | tar -xJ --strip-components=1 -C /var/lib/flightgear -f -
echo 'Done.'
