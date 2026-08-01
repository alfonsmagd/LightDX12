# LightDX12

Minimal Direct3D 12 repository containing the `LightD3D12` library and two samples:

- `Triangle`
- `CBSRVCubes`

The project depends only on the Windows SDK; it has no package-manager or Git submodule dependencies.

## Requirements

- Windows 10 or newer
- Visual Studio 2022 with the Desktop development with C++ workload
- Windows 10/11 SDK with DXC
- CMake 3.24 or newer available in `PATH`

## Generate the solution

Run `GenerateSolution.bat`. It creates `build\LightDX12.sln` with the `LightD3D12`, `Triangle`, and `CBSRVCubes` targets.
