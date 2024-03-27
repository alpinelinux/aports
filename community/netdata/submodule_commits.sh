#!/bin/sh

#shellcheck disable=SC3040
#shellcheck disable=SC3043
set -eu -o pipefail

version=${1?Please provide a version}

case $version in
	v*) ;;
	*) echo "Version should start with 'v'"; exit 1;;
esac

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

printf "_aclk_schemas_commit=%s\n" "$(echo "$netdata_submodules" | submodule_commit src/aclk/aclk-schemas)"
printf "_ml_dlib_commit=%s\n" "$(echo "$netdata_submodules" | submodule_commit src/ml/dlib)"
printf "_h2o_commit=%s\n" "$(echo "$netdata_submodules" | submodule_commit src/web/server/h2o/libh2o)"
printf "_fluentbit_commit=%s\n" "$(echo "$netdata_submodules" | submodule_commit src/fluent-bit)"
