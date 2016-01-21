#!/bin/sh

[ -L /usr/bin/Ted ]  && exit 0
[ -e /usr/bin/Ted ] || ln -s /usr/bin/Ted.motif /usr/bin/Ted
