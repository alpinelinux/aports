# Description
APKBUILD for dotnet6-stage0, prebuilt bits sourced from Microsoft

# Generated packages
* dotnet6-stage0-bootstrap
* dotnet6-stage0-artifacts 

# How to build dotnet6 stack
* Build testing/dotnet6-stage0 (provides prebuilt bits for first bootstrap bld)
* Build testing/dotnet6-bootstrap (provides "tainted" SDK for first build)
* Build testing/dotnet6-build ("untainted" build of dotnet)
* Build testing/dotnet6-runtime (packages runtime bits from dotnet6-build)
* Build testing/dotnet6-sdk (packages sdk bits from dotnet6-build)

# Special functions
* abuild _update_source: Stage0 adapts prebuilt artifacts packages by replacing
linux-x64 nupkgs with linux-musl-x64 versions. Any updates to artifacts will
require executing this function to update source with up to date nuget packages
