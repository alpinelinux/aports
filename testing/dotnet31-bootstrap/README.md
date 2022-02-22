# Description
APKBUILD for dotnet31-bootstrap, acting as intermediary between stage0 and 
untainted dotnet build. This is a workaround for buildrepo, allowing automatic
build of dotnet after stage0, insuring that end-user doesn't have any binaries
"tainted" by Microsoft prebuilt binaries

# Generated packages
* dotnet31-bootstrap (used by dotnet31-build for first untainted dotnet build)
* dotnet31-bootstrap-artifacts (provides nupkgs for dotnet31-build)

# Special functions
To update, execute 'abuild _update_bootstrap
