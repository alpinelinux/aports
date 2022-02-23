# Description
APKBUILD for dotnet6-bootstrap, acting as intermediary between stage0 and 
untainted dotnet build. This is a workaround for buildrepo, allowing automatic
build of dotnet after stage0, insuring that end-user doesn't have any binaries
"tainted" by Microsoft prebuilt binaries

# Generated packages
* dotnet6-bootstrap (used by dotnet6-build for first untainted dotnet build)
* dotnet6-bootstrap-artifacts (provides nupkgs for dotnet6-build)

# Special functions
To update, execute 'abuild _update_bootstrap
