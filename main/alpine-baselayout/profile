# Append "$1" to $PATH when not already in.
# Copied from Arch Linux, see #12803 for details.
append_path () {
	case ":$PATH:" in
		*:"$1":*)
			;;
		*)
			PATH="${PATH:+$PATH:}$1"
			;;
	esac
}

append_path "/usr/local/sbin"
append_path "/usr/local/bin"
append_path "/usr/sbin"
append_path "/usr/bin"
append_path "/sbin"
append_path "/bin"
unset -f append_path

export PATH
export PAGER=less
umask 022

# use nicer PS1 for bash and busybox ash
if [ -n "$BASH_VERSION" -o "$BB_ASH_VERSION" ]; then
	PS1='\h:\w\$ '
# use nicer PS1 for zsh
elif [ -n "$ZSH_VERSION" ]; then
	PS1='%m:%~%# '
# set up fallback default PS1
else
	: "${HOSTNAME:=$(hostname)}"
	PS1='${HOSTNAME%%.*}:$PWD'
	[ "$(id -u)" -eq 0 ] && PS1="${PS1}# " || PS1="${PS1}\$ "
fi

if [ -n "$BASH_VERSION" ] && [ "$BASH" != "/bin/sh" ]; then
	# if we're bash (and not /bin/sh bash), also source the bashrc
	# by default, bash sources the bashrc for non-login,
	# and only /etc/profile on login (-l). so, make it do both on login.
	# this ensures that login-shell bash (e.g. -bash or bash -l) still sources the
	# system bashrc, which e.g. loads bash-completion
	. /etc/bash/bashrc
fi

for script in /etc/profile.d/*.sh ; do
	if [ -r "$script" ] ; then
		. "$script"
	fi
done
unset script
