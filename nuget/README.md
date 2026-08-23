# Ldx12 native NuGet package

The package is intended for native x64 Visual Studio 2022 projects. It contains only the public headers, `Ldx12d.lib` for Debug, `Ldx12.lib` for Release and the MSBuild integration files. It does not contain Ldx12 implementation `.cpp` files.

## Build and test locally

Download `nuget.exe` from <https://www.nuget.org/downloads> and place it at `build/tools/nuget.exe`, or pass its path explicitly:

```powershell
./nuget/Pack.ps1 -Version 0.1.0-local -NuGetExe C:/Tools/NuGet/nuget.exe
./nuget/TestPackage.ps1 -Version 0.1.0-local -NuGetExe C:/Tools/NuGet/nuget.exe
```

The package is written to `packages/Ldx12.0.1.0-local.nupkg`. The test extracts it through NuGet and compiles a native VS2022 consumer in both Debug and Release.

To inspect it in the Visual Studio package manager before publishing:

1. Open `Tools > NuGet Package Manager > Package Manager Settings`.
2. Add a package source named `Ldx12 Local` pointing to the repository's `packages` directory.
3. Open `Manage NuGet Packages`, select `Ldx12 Local`, enable `Include prerelease` and search for `Ldx12`.

## Publish to NuGet.org

Ldx12 and the NuGet package use the MIT License. Third-party attribution is included in `THIRD_PARTY_NOTICES.md`, and both files are embedded in the package.

Configure a NuGet.org Trusted Publishing policy with:

- Repository owner: `alfonsmagd`
- Repository: `LightDX12`
- Workflow file: `compile-and-ctest-windows-cmake.yml`
- Environment: `release`

Add the GitHub Actions secret `NUGET_USER` containing the NuGet.org profile name. Then create and push a semantic-version tag:

```powershell
git tag v0.1.0
git push origin v0.1.0
```

The existing workflow builds the package, verifies the VS2022 Debug and Release consumers, obtains a short-lived NuGet.org credential through OIDC and publishes `Ldx12.0.1.0.nupkg`. Branches and pull requests only build and test a prerelease package; they never publish it.

NuGet.org package versions cannot be overwritten. Publish a newer tag when a package needs a correction.

After NuGet.org validates and indexes the package, select the `nuget.org` source in Visual Studio's package manager and search for `Ldx12`. It can also be installed from the Visual Studio Package Manager Console:

```powershell
Install-Package Ldx12 -Version 0.1.0
```

## Consumer behavior

Installing `Ldx12` in a native x64 `.vcxproj` automatically provides:

- `build/native/include` as an include directory;
- C++20 and Ldx12 public preprocessor definitions;
- `Ldx12d.lib` for Debug or `Ldx12.lib` for Release;
- the required Direct3D 12 system libraries.

The package reports a clear build error for non-x64 Visual Studio platforms. CMake consumers should continue using the installed CMake package and `find_package(Ldx12 CONFIG REQUIRED)` instead of modifying generated `.vcxproj` files through NuGet.
