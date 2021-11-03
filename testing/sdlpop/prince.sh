#!/bin/sh

cd /usr/share/sdlpop
if [ "$1" == '--help' ]; then
  cat doc/Readme.txt
else
  exec sdlpop-prince "$@"
fi
