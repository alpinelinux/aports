#!/bin/sh
# Converts old <feature>.file and <feature>.module mkinitfs feature to new sh based format.
# Usage: initfs-convert-feature.sh <feature directory>/<feature basename>[.<(module|files)>]

for p ; do
f="${p##*/}" && f="${f%.*}"
d="${p%/*}"


cat <<EOF
# Initifs feature $f.

initfs_${f}() { return 0 ; }
EOF

[ -e "${d}/$f.modules" ] && cat << EOF

_initfs_${f}_modules() {
$(	printf "\tcat <<-'EOF'\n"
	cat "${d}/$f.modules" | sed -e 's|^|\t\t|g'
	printf "\tEOF\n"
)
}
EOF

[ -e "${d}/$f.files" ] && cat << EOF

_initfs_${f}_files() {
$(	printf "\tcat <<-'EOF'\n"
	cat "${d}/$f.files" | sed -e 's|^|\t\t|g'
	printf "\tEOF\n"
)
}
EOF

done
