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


for image; do
	filepath="$releasedir/${image##*/}"
	datetime="$(stat -c "%y" $image)"
	size="$(stat -c "%s" $image)"
	date=${datetime%% *}
	time=${datetime#* }
	time=${time%.*}
	file=${filepath##*/}
	flavor=${file%-${release}-${arch}.*}

	cat <<-EOF
	-
	  branch: $branch
	  arch: $arch
	  version: $release
	  flavor: $flavor
	  file: $file
	  iso: $file
	  date: $date
	  time: $time
	  size: $size
EOF
	# generate checksums if missing
	for hash in ${checksums}; do
		if ! [ -f "$image.$hash" ]; then
			${hash}sum $image | sed 's: .*/:  :' > $image.$hash
		fi
		echo "  $hash: $(cut -d' ' -f1 $image.$hash)"
	done


done
