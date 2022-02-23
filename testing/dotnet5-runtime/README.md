# Description
APKBUILD for dotnet5-runtime. Note that this doesn't actually build anything.
It depends on dotnet5-build, which handles the actual dotnet build process
This is a workaround to abuild / lua-aports not supporting custom pkgver
for subpackages. While there are pending MRs for this feature, the feature
is yet to be introduced. See abuild!137 and lua-aports!4

# Generated packages
* aspnetcore5-runtime
* aspnetcore5-targeting-pack
* dotnet5-apphost-pack (used by dotnet5-runtime)
* dotnet5-host (provides dotnet-host)
* dotnet5-host-zsh-completion (provides dotnet-host-zsh-completion)
* dotnet5-host-bash-completion (provides dotnet-host-bash-completion)
* dotnet5-host-doc (docs for dotnet5)
* dotnet5-hostfxr (provides fxr for host)
* dotnet5-runtime
* dotnet5-targeting-pack
 
# How to build dotnet5 stack
* Build testing/dotnet5-stage0 (provides prebuilt bits for first bootstrap bld)
* Build testing/dotnet5-bootstrap (provides "tainted" SDK for first build)
* Build testing/dotnet5-build ("untainted" build of dotnet)
* Build testing/dotnet5-runtime (packages runtime bits from dotnet5-build)
* Build testing/dotnet5-sdk (packages sdk bits from dotnet5-build)
