#!/bin/sh

# Flare-game can be placed in two default locations: /usr/share/flare for system-wide installation
# or ~/.local/share/flare for user installation.
# It's better to place it inside user directory because each user could have their own game assets.

FG_VERSION=1.12
INST_DIRECTORY="$HOME/.local/share/flare"

echo "Downloading Flare Game data (version $FG_VERSION) ..."
mkdir -p ${INST_DIRECTORY}
curl -L https://github.com/flareteam/flare-game/releases/download/v${FG_VERSION}/flare-game-v${FG_VERSION}.tar.gz | tar -xz --strip-components=1 -C ${INST_DIRECTORY} -f -
echo 'Done.'
