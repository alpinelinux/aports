# Description
APKBUILD for dotnet31-runtime. Note that this doesn't actually build anything.
It depends on dotnet31-build, which handles the actual dotnet build process
This is a workaround to abuild / lua-aports not supporting custom pkgver
for subpackages. While there are pending MRs for this feature, the feature
is yet to be introduced. See abuild!137 and lua-aports!4

# Generated packages
* aspnetcore31-runtime
* aspnetcore31-targeting-pack
* dotnet31-apphost-pack (used by dotnet31-runtime)
* dotnet31-host (provides dotnet-host)
* dotnet31-host-zsh-completion (provides dotnet-host-zsh-completion)
* dotnet31-host-bash-completion (provides dotnet-host-bash-completion)
* dotnet31-host-doc (docs for dotnet31)
* dotnet31-hostfxr (provides fxr for host)
* dotnet31-runtime
* dotnet31-targeting-pack
 
# How to build dotnet31 stack
* Build testing/dotnet31-stage0 (provides prebuilt bits for first bootstrap bld)
* Build testing/dotnet31-bootstrap (provides "tainted" SDK for first build)
* Build testing/dotnet31-build ("untainted" build of dotnet)
* Build testing/dotnet31-runtime (packages runtime bits from dotnet31-build)
* Build testing/dotnet31-sdk (packages sdk bits from dotnet31-build)
