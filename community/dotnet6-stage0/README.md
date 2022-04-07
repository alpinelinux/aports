# Description
APKBUILD for dotnet6-stage0, initial bootstrap for dotnet

# Generated packages
* dotnet6-stage0-bootstrap
* dotnet6-stage0-artifacts 

# How to build dotnet6 stack
* Build testing/dotnet6-stage0 (builds stage0 dotnet bootstrap)
* Build testing/dotnet6-build ("untainted" build of dotnet, and packages sdk)
* Build testing/dotnet6-runtime (packages runtime bits from dotnet6-build

# Special functions
* abuild _update_source: Stage0 adapts prebuilt artifacts packages by replacing
linux-x64 nupkgs with linux-musl-x64 versions. Any updates to artifacts will
require executing this function to update source with up to date nuget packages
