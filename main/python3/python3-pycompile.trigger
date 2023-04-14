#!/bin/sh

# just two jobs to be a bit faster but not overuse resources
python3 -m compileall -j2 -q "$@"
