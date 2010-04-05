#!/bin/sh

for p in 1 2 3 4 5 6
do
    echo "============>>> ERROR: Pass $p <<<============"
    make main 2>&1 | tee makelog-pass-$p.txt | grep ">>> ERROR:"
done
