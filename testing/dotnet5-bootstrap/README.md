# Description
APKBUILD for dotnet5-bootstrap, acting as intermediary between stage0 and 
untainted dotnet build. This is a workaround for buildrepo, allowing automatic
build of dotnet after stage0, insuring that end-user doesn't have any binaries
"tainted" by Microsoft prebuilt binaries

# Generated packages
* dotnet5-bootstrap (used by dotnet5-build for first untainted dotnet build)
* dotnet5-bootstrap-artifacts (provides nupkgs for dotnet5-build)

# Special functions
To update, execute 'abuild _update_bootstrap
