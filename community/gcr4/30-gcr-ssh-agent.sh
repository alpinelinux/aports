# Set SSH_AUTH_SOCK for gcr-ssh-agent.
if [ ! "$SSH_AUTH_SOCK" ]; then
	export SSH_AUTH_SOCK="/run/user/$(id -u)/gcr/ssh"
fi
