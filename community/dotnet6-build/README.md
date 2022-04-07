# Description
APKBUILD for dotnet6-build. This is sourced by dotnet6-runtime which handles
the runtime packaging due to this aport building products with varying versions

# Generated packages
* dotnet6-build (aimed for internal use as bootstrap)
* dotnet6-build-artifacts (aimed for internal use as bootstrap)
* dotnet6-sdk
* dotnet6-templates (required by sdk)
* dotnet-zsh-completion
* dotnet-bash-completion
* dotnet-doc
* netstandard21-targeting-pack

 
# How to build dotnet6 stack
* Build testing/dotnet6-stage0 (builds stage0 bootstrap)
* Build testing/dotnet6-build ("untainted" build of dotnet, packages sdk)
* Build testing/dotnet6-runtime (packages runtime bits from dotnet6-build)
