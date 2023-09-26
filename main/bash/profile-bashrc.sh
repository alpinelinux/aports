if [ -n "$BASH_VERSION" ] && [ "$BASH" != "/bin/sh" ]; then
	# if we're bash (and not /bin/sh bash), also source the bashrc
	# by default, bash sources the bashrc for non-login,
	# and only /etc/profile on login (-l). so, make it do both on login.
	# this ensures that login-shell bash (e.g. -bash or bash -l) still sources the
	# system bashrc, which e.g. loads bash-completion
	. /etc/bash/bashrc
fi
