#!/bin/sh
# Use this instead of packaging makedev.

# Exit on failure.
set -e

# Create devices in $PWD.
ln -s /proc/self/fd fd
ln -s fd/0 stdin
ln -s fd/1 stdout
ln -s fd/2 stderr
mknod full c 1 7
mknod null c 1 3
mknod ptmx c 5 2
mkdir pts shm
mknod random c 1 8
mknod tty c 5 0
mknod urandom c 1 9
mknod zero c 1 5
