#!/bin/sh

# Bake checksums and apk keys into init stub.

# Ugly formatting is due to sed.

# Replace '###SUMS###' token with checksums for busybox and apk.
sed -e '/###SUMS###/c\
'"$(cd ${outroot:?} && sha512sum bin/busybox.static sbin/apk.static | sed -e '$q s/$/\\/g')"'' -i "${outfile:?}"

# Insert keys after '###KEYS###' token.
for file in "${keysdir:?}"/* ; do
	[ -e "$file" ] && [ -f "$file" ] || continue
	sed -e '/###KEYS###/a\
$BB cat > \/.init.d\/etc\/apk\/keys\/'"${file##*/}"' <<"EOF"\
'"$(cat "$file" | sed -e 's/$/\\/')"'
EOF\
' -i "${outfile:?}"
done

# Replace '###KEYS### token ' with comment.
sed -e 's/###KEYS###/# Keys:/g' -i "${outfile:?}"
