# Description
APKBUILD for dotnet6-build. This is sourced by dotnet6-runtime and dotnet6-sdk,
which handles the actual packaging due to this aport building products with
varying versions

# Generated packages
* dotnet6-build (aimed for internal use)
 
# How to build dotnet6 stack
* Build testing/dotnet6-stage0 (provides prebuilt bits for first bootstrap bld)
* Build testing/dotnet6-bootstrap (provides "tainted" SDK for first build)
* Build testing/dotnet6-build ("untainted" build of dotnet)
* Build testing/dotnet6-runtime (packages runtime bits from dotnet6-build)
* Build testing/dotnet6-sdk (packages sdk bits from dotnet6-build)

# Special functions
* abuild _update_bootstrap: Updates dotnet6-bootstrap to current version
