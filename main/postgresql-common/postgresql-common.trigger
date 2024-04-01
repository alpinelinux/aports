#!/bin/sh

# Sort dirs in reverse order to prefer a higher version if the user installed
# multiple versions at once.
for dir in $(printf '%s\n' "$@" | sort -r); do
	pgver=${dir#*postgresql}
	expr "$pgver" : '[0-9]*$' >/dev/null || continue

	case "$dir" in
		# This is needed for creating man pages symlinks after a -doc
		# package is installed.
		/usr/share/postgresql[0-9]*)
				pg_versions fix
				continue
		;;
		/usr/libexec/postgresql[0-9]*)
			# 1. If the default version is already linked and provides postgres server.
			if [ -f /usr/libexec/postgresql/postgres ]; then
				pg_versions fix

			# 2. If the default version is already linked and both the default version
			#    and the affected version provides only client programs.
			elif [ -f /usr/libexec/postgresql/psql ] && ! [ -f "$dir"/postgres ]; then
				pg_versions fix

			# 3. If a new version has been installed and no default version has been
			#    set yet or the new version provides postgres server while the current
			#    default does not.
			elif [ -f "$dir"/psql ]; then
				echo "* Setting postgresql$pgver as the default version" >&2
				pg_versions set-default "$pgver"

			# 4. If the default version has not been set yet or it was uninstalled
			#    and there is some postgresql version installed.
			elif pg_versions list -q >/dev/null; then
				pgver=$(pg_versions list | head -n1)
				echo "* Setting postgresql$pgver as the default version" >&2

			# 5. There's no postgresql version installed.
			else
				pg_versions uninstall
			fi
		;;
		*)
			continue
		;;
	esac
done


# APK cannot offer the user an upgrade to a newer major version because it's
# provided by a different package. Thus we use this trigger and inform the user
# about a new major version if the current default version is not the latest.

# Find the latest available major version.
latest_ver=$(apk list -Pa postgresql \
             	| sed -En 's/.*\{postgresql([0-9]+)\}.*/\1/p' \
             	| sort | tail -n1) || exit 0

default_ver=$(pg_versions get-default -q) || exit 0

if [ "$latest_ver" ] && [ "$default_ver" -lt "$latest_ver" ]; then
	cat >&2 <<-EOF
	*
	* You are using 'postgresql$default_ver'. It's recommended to upgrade to the latest
	* major version provided by package 'postgresql$latest_ver'.
	* Use command 'pg_versions' to switch between versions.
	*
	EOF
fi

exit 0
