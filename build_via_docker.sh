#!/bin/sh

if [ "$#" -ne 2 ]; then
  echo "USAGE:   $0 ARCH REPO/PACKAGE"
  echo "EXAMPLE  $0 armhf main/busybox"
  exit 1
fi


BASE=$( cd $( dirname $0 ); pwd)
ARCH=$1
PACKAGE=$2

mkdir -p ${BASE}/packages
chown 1000 ${BASE}/packages

docker run -it --rm \
  -v ${BASE}:/aports \
  -v ${BASE}/packages:/home/build/packages \
  --user build --workdir /aports \
  micwy/aport-builder-${ARCH}:latest \
  /build.sh $PACKAGE
