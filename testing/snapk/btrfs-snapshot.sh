#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later

[ -f /etc/snapk ] && . /etc/snapk

SNAP_VOL="${SNAP_VOL:-/}"
SNAP_DIR="${SNAP_DIR:-/.snapshots}"
SNAP_AGE="${SNAP_AGE:-30}"
SNAP_PREFIX="${SNAP_PREFIX:-snapk-}"
SNAP_LIMIT="${SNAP_LIMIT:-100}"

snap_date="$(date +%Y-%m-%d-%H%M)"

# Snapshots are formatted ${prefix}${date}${index}
# Prefix is to ensure only snapshots created with this tool are deleted when pruning
# Date is used to clarify when snapshots are from
# Index is used if multiple snapshots are created per minute

snapshot() {
	count=1
	while [ -d "$SNAP_DIR"/"${SNAP_PREFIX}${snap_date}${count}" ]; do
		count=$((count+1))
	done
	btrfs subvolume snapshot "$SNAP_VOL" "$SNAP_DIR"/"${SNAP_PREFIX}${snap_date}${count}"
}

prune_by_age() {
	find "$SNAP_DIR"/ -type d -name "${SNAP_PREFIX}*" -mindepth 1 -maxdepth 1 -mtime +"$SNAP_AGE" -exec btrfs subvolume delete {} \;

}
prune_by_limit() {
	x=0
	snaps=$(find "$SNAP_DIR"/ -type d -name "${SNAP_PREFIX}*" -mindepth 1 -maxdepth 1 | sort -r)
	for snap in $snaps;do
		x=$((x+1))
		if [ $x -gt "$SNAP_LIMIT" ];then
			btrfs subvolume delete "$snap"
		fi
	done
}

case "$1" in
	pre-commit)
		snapshot
		;;
	post-commit)
		snapshot
		if [ "$SNAP_AGE" -ne 0 ];then prune_by_age; fi
		if [ "$SNAP_LIMIT" -ne 0 ];then prune_by_limit; fi
		;;
esac
