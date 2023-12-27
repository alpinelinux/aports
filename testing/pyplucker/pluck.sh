#!/usr/bin/env bash

set -e

## Dependencies
#
# sudo apk add nodejs npm libnotify
# sudo npm install -g percollate
#
## Usage
#
# Copy the link to save, or a blob of text, then run this script
# It will produce a PDB doc for Plucker in ~/.plucker/

CODEPAGE="cp1251"

timestamp="$(date +%s)"
workdir="/tmp/pluckerbook$timestamp"

mkdir "$workdir"
cd "$workdir"

clip="$(xclip -o -selection clip)"

if [[ "$clip" = http* ]]; then
  percollate html "$clip"

  html_orig="$(ls *.html | xargs -I% basename % | head -1)"
  iconv -c -f utf-8 -t "$CODEPAGE" "$html_orig" > html_conv.html

  output_name="$(echo $html_orig | sed -e 's/\.html//' | cut -c 1-16)"
  plucker-build --doc-file="$output_name" --bpp=4 --maxdepth=1 -H "$PWD"/html_conv.html

  echo "$clip" >> $HOME/.plucker/history.txt

else
  echo $clip > note.txt
  iconv -c -f utf-8 -t "$CODEPAGE" note.txt > note_conv.txt

  output_name="Clip$timestamp"
  plucker-build --doc-file="$output_name" -H "$PWD"/note_conv.txt
fi

rm -rf "$workdir"

notify-send "Built $output_name"

