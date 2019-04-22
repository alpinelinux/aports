# Profile for Nix package manager
# This script is based on https://github.com/NixOS/nix/blob/master/scripts/nix-profile.sh.in.

# Sanity check
[ "$HOME" ] && [ "$USER" ] || return 0

_nix_setup_user() {
	local nix_profile="$HOME/.nix-profile"
	local nix_defexpr="$HOME/.nix-defexpr"
	local profiles_dir="/nix/var/nix/profiles"
	local user_profile_dir="$profiles_dir/per-user/$USER"
	local user_gcroots_dir="/nix/var/nix/gcroots/per-user/$USER"

	mkdir -m 0755 -p "$user_profile_dir"
	[ -O "$user_profile_dir" ] \
		|| echo "Nix: WARNING: bad ownership on $user_profile_dir, should be $(id -u)" >&2

	[ -w "$HOME" ] || return 0

	# Create ~/.nix-profile if needed.
	if ! [ -L "$nix_profile" ]; then
		echo "Nix: creating $nix_profile" >&2

		if [ "$USER" = root ]; then
			# Root installs in the system-wide profile by default.
			ln -s "$profiles_dir/default" "$nix_profile" \
				|| echo "Nix: WARNING: could not create $nix_profile -> $profiles_dir/default" >&2
		else
			ln -s "$user_profile_dir/profile" "$nix_profile" \
				|| echo "Nix: WARNING: could not create $nix_profile -> $user_profile_dir/profile" >&2
		fi
	fi

	# Subscribe the user to the unstable Nixpkgs channel by default.
	if ! [ -e "$HOME/.nix-channels" ]; then
		echo 'https://nixos.org/channels/nixpkgs-unstable nixpkgs' > "$HOME/.nix-channels"
	fi

	# Create the per-user garbage collector roots directory.
	mkdir -m 0755 -p "$user_gcroots_dir"
	[ -O "$user_gcroots_dir" ] \
		|| echo "Nix: WARNING: bad ownership on $user_gcroots_dir, should be $(id -u)" >&2

	# Set up a default Nix expression from which to install stuff.
	if [ ! -e "$nix_defexpr" -o -L "$nix_defexpr" ]; then
		rm -f "$nix_defexpr"
		mkdir -p "$nix_defexpr"

		if [ "$USER" != root ]; then
			ln -s "$profiles_dir"/per-user/root/channels "$nix_defexpr"/channels_root
		fi
	fi

	export NIX_PROFILES="$NIX_PROFILES $nix_profile"

	# Append ~/.nix-defexpr/channels/nixpkgs to $NIX_PATH so that <nixpkgs>
	# paths work when the user has fetched the Nixpkgs channel.
	export NIX_PATH="${NIX_PATH:+$NIX_PATH:}nixpkgs=$nix_defexpr/channels/nixpkgs"

	# Set up secure multi-user builds; non-root users build through the Nix daemon.
	[ "$USER" = root ] || export NIX_REMOTE='daemon'
}


# Set $NIX_SSL_CERT_FILE so that Nixpkgs applications like curl work.
export NIX_SSL_CERT_FILE='/etc/ssl/certs/ca-certificates.crt'

# The default profile for all users.
export NIX_PROFILES='/nix/var/nix/profiles/default'

# Set up environment for users that are allowed to build and install Nix
# packages: root and members of nix or wheel group.
if [ "$USER" = root ] || id -nG | grep -Eq '\b(nix|wheel)\b'; then
	_nix_setup_user
fi

# Set up PATH and MANPATH.
for _i in $NIX_PROFILES; do
	export PATH="$_i/bin:$PATH"
	[ "$MANPATH" ] && export MANPATH="$_i/share/man:$MANPATH"
done

unset _i
unset -f _nix_setup_user
