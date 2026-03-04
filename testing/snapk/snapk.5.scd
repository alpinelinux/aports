snapk(5)

# NAME

snapk - configuration file

# DESCRIPTION

/etc/snapk.conf should supply SNAP_VOL and SNAP_DIR.

# CONFIGURATION OPTIONS

*SNAP_VOL*
	Defines the volume to snapshot.

*SNAP_DIR*
	Defines the directory in which to place snapshots of the SNAP_VOL.

*SNAP_AGE*
	Defines snapshot retention duration - older snapshots are removed.
	To disable set to 0.

*SNAP_LIMIT*
	Defines the number of snapshots to keep. Oldest snapshots are removed.
	To disable set to 0.

*SNAP_PREFIX*
	Defines prefix to snapshots. Only snapshots with this prefix are
	removed automatically. Snapshots are only removed by age or by limit, so
	if both SNAP_AGE and SNAP_LIMIT are 0, none will be removed.

