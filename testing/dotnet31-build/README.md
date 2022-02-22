# Description
APKBUILD for dotnet31-build. This is sourced by dotnet31-runtime and dotnet31-sdk,
which handles the actual packaging due to this aport building products with
varying versions

# Generated packages
* dotnet31-build (aimed for internal use)
 
# How to build dotnet31 stack
* Build testing/dotnet31-stage0 (provides prebuilt bits for first bootstrap bld)
* Build testing/dotnet31-bootstrap (provides "tainted" SDK for first build)
* Build testing/dotnet31-build ("untainted" build of dotnet)
* Build testing/dotnet31-runtime (packages runtime bits from dotnet31-build)
* Build testing/dotnet31-sdk (packages sdk bits from dotnet31-build)

# Special functions
* abuild _update_bootstrap: Updates dotnet31-bootstrap to current version
