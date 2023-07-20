#!/bin/sh

#shellcheck disable=SC3040
#shellcheck disable=SC3043
set -eu -o pipefail

version=${1?Please provide a version}

req() {
	local request="$1"
	curl \
		-H 'Accept: application/vnd.github+json' \
		-Ssf https://api.github.com/repos/"$request"
}

submodule_commit() {
	local path="$1"

	jq -r --arg path "$path" 'select(.path == $path) | .sha'
}

netdata_submodules="$(req netdata/netdata/git/trees/"${version}"\?recursive=true | jq '.tree[] | select(.type == "commit")')"

websockets_commit="$(echo "$netdata_submodules" | submodule_commit mqtt_websockets)"
websockets_submodules="$(req underhood/mqtt_websockets/git/trees/"${websockets_commit}"\?recursive=true | jq '.tree[] | select(.type == "commit")')"

printf "_aclk_schemas_commit=%s\n" "$(echo "$netdata_submodules" | submodule_commit aclk/aclk-schemas)"
printf "_ml_dlib_commit=%s\n" "$(echo "$netdata_submodules" | submodule_commit ml/dlib)"
printf "_mqtt_websockets_commit=%s\n" "$(echo "$netdata_submodules" | submodule_commit mqtt_websockets)"
printf "_h2o_commit=%s\n" "$(echo "$netdata_submodules" | submodule_commit web/server/h2o/libh2o)"
printf "_c_rbuf_commit=%s\n" "$(echo "$websockets_submodules" | submodule_commit c-rbuf)"
printf "_c_rhash_commit=%s\n" "$(echo "$websockets_submodules" | submodule_commit c_rhash)"
