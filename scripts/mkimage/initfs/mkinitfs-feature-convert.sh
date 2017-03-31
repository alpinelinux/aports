#!/bin/sh
# Converts old <feature>.file and <feature>.module mkinitfs feature to new sh based format.
# Usage: initfs-convert-feature.sh <feature directory>/<feature basename>[.<(module|files)>]

for p ; do
f="${p##*/}" && f="${f%.*}"
d="${p%/*}"


cat <<EOF
# Initifs feature $f.

initfs_${f}() {
	return 0
}
EOF

cat << EOF

_initfs_${f}_modules() {
$( if [ -e "${d}/$f.modules" ] ; then
	printf "\tcat <<-'EOF'\n"
	cat "${d}/$f.modules" | sed -e 's|^|\t\t|g'
	printf "\tEOF\n"
   else printf '\treturn 0\n' ; fi
)
}
EOF

cat << EOF

_initfs_${f}_files() {
$( if [ -e "${d}/$f.files" ] ; then
	printf "\tcat <<-'EOF'\n"
	cat "${d}/$f.files" | sed -e 's|^|\t\t|g'
	printf "\tEOF\n"
   else printf '\treturn 0\n' ; fi
)
}
EOF

done
