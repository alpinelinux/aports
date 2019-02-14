#!/bin/sh

umask 022
/usr/bin/pango-querymodules > ${1}.cache
