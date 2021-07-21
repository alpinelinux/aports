#!/bin/sh

PROGNAME='kernel-hooks'
HOOKS_DIR='/etc/kernel-hooks.d'

[ -d $HOOKS_DIR ] || exit 0

flavors_vers=
for path in "$@"; do
	name="${path##*/}"

	case "$name" in
		[0-9]*-[0-9]*-*) ;;  # go on
		*) echo "$PROGNAME: ERROR: invalid kernel version: $name!" >&2; exit 1;;
	esac

	flavor=${name#*-}; flavor=${flavor#*-}
	ver=${name%-$flavor}
	flavors_vers="$flavors_vers $flavor:$ver"
done

ret=0
for flavor in $(printf '%s\n' $flavors_vers | sort | cut -d: -f1 | uniq); do
	relfile=/usr/share/kernel/$flavor/kernel.release

	new_ver=
	old_ver=
	for fv in $flavors_vers; do
		[ "${fv%:*}" = "$flavor" ] || continue
		ver=${fv#*:}

		[ "$(cat "$relfile" 2>/dev/null)" = "$ver-$flavor" ] \
			&& new_ver=$ver \
			|| old_ver=$ver
	done

	for hook in $HOOKS_DIR/*; do
		[ -x "$hook" ] || continue
		name=${hook##*/}

		echo "$PROGNAME: executing hook $name ($flavor, $new_ver, $old_ver)" >&2

		$hook "$flavor" "$new_ver" "$old_ver" && continue
		
		echo "$PROGNAME: ERROR: hook $name failed, skipping hooks for linux-$flavor" >&2
		ret=1
		break
	done
done

exit $ret
