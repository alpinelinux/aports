# Description
APKBUILD for dotnet5-build. This is sourced by dotnet5-runtime and dotnet5-sdk,
which handles the actual packaging due to this aport building products with
varying versions

# Generated packages
* dotnet5-build (aimed for internal use)
 
# How to build dotnet5 stack
* Build testing/dotnet5-stage0 (provides prebuilt bits for first bootstrap bld)
* Build testing/dotnet5-bootstrap (provides "tainted" SDK for first build)
* Build testing/dotnet5-build ("untainted" build of dotnet)
* Build testing/dotnet5-runtime (packages runtime bits from dotnet5-build)
* Build testing/dotnet5-sdk (packages sdk bits from dotnet5-build)

# Special functions
* abuild _update_bootstrap: Updates dotnet5-bootstrap to current version
