#!/bin/sh
set -eu

requested_name=$1
id=$2

# OPTIMIZE: Only call api once
name=$(go-passbolt-cli get resource --id $id | awk '/^Name:/ { print $2 }')
if [ "$name" != "$requested_name" ]; then
  echo "Name does not match (expected: $requested_name, returned: $name)" >&2
  exit 1
fi

pass=$(go-passbolt-cli get resource --id $id | awk '/^Password:/ { print $2 }')

echo "$pass"
