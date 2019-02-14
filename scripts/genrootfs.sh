#!/bin/sh -e

cleanup() {
	rm -rf "$tmp"
}

tmp="$(mktemp -d)"
trap cleanup EXIT

arch="$(apk --print-arch)"
repositories_file=/etc/apk/repositories
keys_dir=/etc/apk/keys

while getopts "a:r:k:o:" opt; do
	case $opt in
	a) arch="$OPTARG";;
	r) repositories_file="$OPTARG";;
	k) keys_dir="$OPTARG";;
	o) outfile="$OPTARG";;
	esac
done
shift $(( $OPTIND - 1))

cat "$repositories_file"

if [ -z "$outfile" ]; then
	outfile=$name-$arch.tar.gz
fi

${APK:-apk} add --keys-dir "$keys_dir" --no-cache \
	--repositories-file "$repositories_file" \
	--no-script --root "$tmp" --initdb \
	"$@"
for link in $("$tmp"/bin/busybox --list-full); do
	[ -e "$tmp"/$link ] || ln -s /bin/busybox "$tmp"/$link
done

${APK:-apk} fetch --keys-dir "$keys_dir" --no-cache \
	--repositories-file "$repositories_file" \
	--stdout --quiet alpine-base | tar -zx -C "$tmp" etc/

branch=edge
VERSION_ID=$(awk -F= '$1=="VERSION_ID" {print $2}'  "$tmp"/etc/os-release)
case $VERSION_ID in
*_alpha*|*_beta*) branch=edge;;
*.*.*) branch=v${VERSION_ID%.*};;
esac

cat > "$tmp"/etc/apk/repositories <<EOF
http://dl-cdn.alpinelinux.org/alpine/$branch/main
http://dl-cdn.alpinelinux.org/alpine/$branch/community
EOF

#rm -rf "$tmp"/var/cache/apk/*

tar --numeric-owner -c -C "$tmp" . | gzip -9n > "$outfile"
