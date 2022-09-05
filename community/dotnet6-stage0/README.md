# Description
APKBUILD for dotnet6-stage0, initial bootstrap for dotnet

# Generated packages
* dotnet6-stage0-bootstrap
* dotnet6-stage0-artifacts 

# How to build dotnet6 stack
* Build testing/dotnet6-stage0 (builds stage0 dotnet bootstrap)
* Build testing/dotnet6-build ("untainted" build of dotnet, and packages sdk)
* Build testing/dotnet6-runtime (packages runtime bits from dotnet6-build

# Notes on pulled bootstraps
stage0 uses bootstraps built using `dotnet6-cross` aport available [here](https://gitlab.alpinelinux.org/ayakael/dotnet6-cross)
To bootstrap a new architecture, that aport can be used to crossgen the minimum components from `x86_64`, which stage0 can then
pull to build out the full stack from within the official aport.
