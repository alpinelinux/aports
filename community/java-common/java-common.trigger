#!/bin/sh

# create symlink for the active jvm
cd /usr/lib/jvm

# the link "forced-jvm" is created by an user to explicitly use a specific jvm
if [ -x forced-jvm ]; then
	ln -sfn forced-jvm default-jvm
else
	# if no "forced-jvm" is present, then the latest installed version is taken
	LATEST=`ls -d java-*/bin/java | sort -Vr | head -1 | cut -d/ -f1`
	if [ "$LATEST" ]; then
		ln -sfn $LATEST default-jvm
	fi
fi

# clean up dangling symlinks
for link in $(find -L /usr/bin -type l -print); do
	case "$(readlink $link)" in ../lib/jvm/*)
		rm "$link"
		;;
	esac
done

# create program links for the currently active jre
cd /usr/bin
find ../lib/jvm/default-jvm/bin -mindepth 1 -type f -exec ln -sfn {} . \;

