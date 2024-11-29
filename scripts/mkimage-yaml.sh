#!/bin/sh

progname=$(basename $0)

usage() {
	echo "usage: $progname --checksums <checksums> --arch <arch> FILE..."
}

checksums="sha256 sha512"
while [ $# -gt 0 ]; do
	opt="$1"
	shift
	case "$opt" in
	--checksums) checksums="$1"; shift ;;
	--arch) arch="$1"; shift ;;
	--branch) branch="$1"; shift;;
	--release) release="$1"; shift;;
	--flavor) flavor="$1"; shift;;
	--title) title="$1"; shift;;
	--desc) desc="$1"; shift;;
	--) break ;;
	-*) usage; exit 1;;
	esac
done

set -- $opt "$@"

if [ -z "$branch" ]; then
	case "$release" in
	*.*.*_alpha*|*.*.*_beta*) branch=edge;;
	*.*.*) branch=${release%.*}; branch="v${branch#v}";;
	*)
		git_branch="$(git rev-parse --abbrev-ref HEAD)"
		case "$git_branch" in
		*-stable) branch=${git_branch%-stable};;
		*) branch=edge;;
		esac
		;;
	esac
fi
releasedir="$branch/releases/$arch"

[ -n "$arch" ] || arch=$(apk --print-arch)

# size_and_sum [file] [prefix]
size_and_sum() {
	echo "  ${2}size: $(stat -c "%s" "$1")"
	for hash in ${checksums}; do
		# generate checksums if missing
		if ! [ -f "$1.$hash" ]; then
			${hash}sum "$1" | sed 's: .*/:  :' > "$1.$hash"
		fi
		echo "  $2$hash: $(cut -d' ' -f1 $1.$hash)"
	done
}

for image; do
	filepath="$releasedir/${image##*/}"
	datetime="$(stat -c "%y" $image)"
	size="$(stat -c "%s" $image)"
	date=${datetime%% *}
	time=${datetime#* }
	time=${time%.*}
	file=${filepath##*/}
	flavor=${file%-${release}-${arch}.*}
	desc=$(echo "$desc" | sed -E 's/^\s*/    /')

	cat <<-EOF
	-
	  title: "$title"
	  desc: |
	$desc
	  branch: $branch
	  arch: $arch
	  version: $release
	  flavor: $flavor
	  file: $file
	  iso: $file
	  date: $date
	  time: $time
EOF
	size_and_sum "$image"

	case "$file" in
	*.img.gz)
		extracted=${image%.gz}
		if ! [ -f "$extracted" ]; then
			zcat < "$image" > "$extracted"
		fi
		size_and_sum "$extracted" extracted_
		;;
	esac
done
