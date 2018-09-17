#!/bin/sh
set -e

mkdir test-py
cd test-py

cat > test.py <<'EOF'
import sys
import mgba.core
import mgba.image

core = mgba.core.loadPath(sys.argv[1])
screen = mgba.image.Image(*core.desiredVideoDimensions())
core.setVideoBuffer(screen)
core.reset()

for i in range(2000):
	core.runFrame()

with open("dump.png".format(i), "wb") as f:
	screen.savePNG(f)
EOF

echo "using LD_LIBRARY_PATH [$LD_LIBRARY_PATH]"
echo "using PYTHONPATH [$PYTHONPATH]"
echo "using ZIPFILE [$ZIPFILE]"

unzip "$ZIPFILE"
echo "running Z80 cpu test suite"
python3 test.py "cpu_instrs/cpu_instrs.gb"
ls -al dump.png
[ $(wc -c < dump.png) -gt 768 ]
