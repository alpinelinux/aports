# The Alpine nix package uses the multi-user setup.
# For this purpose, nix-daemon needs be started via OpenRC.
# This file tells nix to use nix-daemon for unprivileged users.
#
# See https://github.com/NixOS/nix/blob/1d0a7b/doc/manual/src/installation/multi-user.md#running-the-daemon
export NIX_REMOTE=daemon
