export PATH="$PATH:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
export PAGER=less
umask 022

# set up fallback default PS1
: "${HOSTNAME:=$(hostname)}"
PS1='${HOSTNAME%%.*}:$PWD'
[ "$(id -u)" = "0" ] && PS1="${PS1}# "
[ "$(id -u)" = "0" ] || PS1="${PS1}\$ "

# use nicer PS1 for bash and busybox ash
[ -n "$BASH_VERSION" -o "$BB_ASH_VERSION" ] && PS1='\h:\w\$ '

# use nicer PS1 for zsh
[ -n "$ZSH_VERSION" ] && PS1='%m:%~%# '

# export PS1 as before
export PS1

for script in /etc/profile.d/*.sh ; do
	if [ -r $script ] ; then
		. $script
	fi
done
