# Description
APKBUILD for dotnet5-sdk. Note that this doesn't actually build anything.
It depends on dotnet5-build, which handles the actual dotnet build process
This is a workaround to abuild / lua-aports not supporting custom pkgver
for subpackages. While there are pending MRs for this feature, the feature
is yet to be introduced. See abuild!137 and lua-aports!4

# Generated packages
* dotnet5 (virtual package that pulls dotnet5-sdk, thus all dotnet products)
* dotnet5-sdk
* dotnet5-templates (required by sdk)
* netstandard5-targeting-pack (provides netstandard21-targeting-pack)
 
# How to build dotnet5 stack
* Build testing/dotnet5-stage0 (provides prebuilt bits for first bootstrap bld)
* Build testing/dotnet5-bootstrap (provides "tainted" SDK for first build)
* Build testing/dotnet5-build ("untainted" build of dotnet)
* Build testing/dotnet5-runtime (packages runtime bits from dotnet5-build)
* Build testing/dotnet5-sdk (packages sdk bits from dotnet5-build)
