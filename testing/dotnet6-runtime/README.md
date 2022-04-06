# Description
APKBUILD for dotnet6-runtime. Note that this doesn't actually build anything.
It depends on dotnet6-build, which handles the actual dotnet build process
This is a workaround to abuild / lua-aports not supporting custom pkgver
for subpackages. While there are pending MRs for this feature, the feature
is yet to be introduced. See abuild!137 and lua-aports!4

# Generated packages
* aspnetcore6-runtime
* aspnetcore6-targeting-pack
* dotnet6-apphost-pack (used by dotnet6-runtime)
* dotnet6-hostfxr (used by dotnet-host)
* dotnet6-runtime
* dotnet6-targeting-pack
* dotnet-host
 
# How to build dotnet6 stack
* Build testing/dotnet6-stage0 (builds stage0 dotnet bootstrap)
* Build testing/dotnet6-build ("untainted" build of dotnet, and packages sdk)
* Build testing/dotnet6-runtime (packages runtime bits from dotnet6-build)
