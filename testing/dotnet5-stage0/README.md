# Description
APKBUILD for dotnet5-stage0, prebuilt bits sourced from Microsoft

# Generated packages
* dotnet5-stage0-bootstrap
* dotnet5-stage0-artifacts 

# How to build dotnet5 stack
* Build testing/dotnet5-stage0 (provides prebuilt bits for first bootstrap bld)
* Build testing/dotnet5-bootstrap (provides "tainted" SDK for first build)
* Build testing/dotnet5-build ("untainted" build of dotnet)
* Build testing/dotnet5-runtime (packages runtime bits from dotnet5-build)
* Build testing/dotnet5-sdk (packages sdk bits from dotnet5-build)

# Special functions
* abuild _update_source: Stage0 adapts prebuilt artifacts packages by replacing
linux-x64 nupkgs with linux-musl-x64 versions. Any updates to artifacts will
require executing this function to update source with up to date nuget packages
