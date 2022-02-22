# Description
APKBUILD for dotnet31-stage0, prebuilt bits sourced from Microsoft

# Generated packages
* dotnet31-stage0-bootstrap
* dotnet31-stage0-artifacts 

# How to build dotnet31 stack
* Build testing/dotnet31-stage0 (provides prebuilt bits for first bootstrap bld)
* Build testing/dotnet31-bootstrap (provides "tainted" SDK for first build)
* Build testing/dotnet31-build ("untainted" build of dotnet)
* Build testing/dotnet31-runtime (packages runtime bits from dotnet31-build)
* Build testing/dotnet31-sdk (packages sdk bits from dotnet31-build)

# Special functions
* abuild _update_source: Stage0 adapts prebuilt artifacts packages by replacing
linux-x64 nupkgs with linux-musl-x64 versions. Any updates to artifacts will
require executing this function to update source with up to date nuget packages
