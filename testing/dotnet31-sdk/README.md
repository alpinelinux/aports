# Description
APKBUILD for dotnet31-sdk. Note that this doesn't actually build anything.
It depends on dotnet31-build, which handles the actual dotnet build process
This is a workaround to abuild / lua-aports not supporting custom pkgver
for subpackages. While there are pending MRs for this feature, the feature
is yet to be introduced. See abuild!137 and lua-aports!4

# Generated packages
* dotnet31 (virtual package that pulls dotnet31-sdk, thus all dotnet products)
* dotnet31-sdk
* dotnet31-templates (required by sdk)
* netstandard5-targeting-pack (provides netstandard21-targeting-pack)
 
# How to build dotnet31 stack
* Build testing/dotnet31-stage0 (provides prebuilt bits for first bootstrap bld)
* Build testing/dotnet31-bootstrap (provides "tainted" SDK for first build)
* Build testing/dotnet31-build ("untainted" build of dotnet)
* Build testing/dotnet31-runtime (packages runtime bits from dotnet31-build)
* Build testing/dotnet31-sdk (packages sdk bits from dotnet31-build)
