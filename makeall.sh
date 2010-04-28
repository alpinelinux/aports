#!/bin/sh

for p in 1 2 3
do
    echo "============>>> ERROR: Pass $p <<<============"
    make main 2>&1 | tee makelog-pass-$p-main.txt | grep ">>> ERROR:"
    make testing 2>&1 | tee makelog-pass-$p-testing.txt | grep ">>> ERROR:"
done
