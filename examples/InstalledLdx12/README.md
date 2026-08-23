# Ldx12 consumer example

This is a strict installed-package consumer. It only calls `find_package(Ldx12 CONFIG REQUIRED)` and therefore uses the public headers and the already compiled `Ldx12.lib`. It neither adds the Ldx12 source directory nor downloads it with `FetchContent`, so none of the library's `.cpp` files are part of this project.

The executable also includes `Ldx12Native.hpp` to verify that optional native D3D12 access is present in the installed package.

## Build and install Ldx12

From the repository root, build and install the static library:

```bat
cmake -S . -B build-ldx12 ^
    -DLDX12_BUILD_APP=OFF ^
    -DLDX12_BUILD_EXAMPLES=OFF ^
    -DLDX12_BUILD_TESTS=OFF ^
    -DLDX12_INSTALL=ON
cmake --build build-ldx12 --config Debug --target Ldx12 --parallel
cmake --build build-ldx12 --config Release --target Ldx12 --parallel
cmake --install build-ldx12 --config Debug --prefix C:\Ldx12
cmake --install build-ldx12 --config Release --prefix C:\Ldx12
```

The resulting installation contains `include/Ldx12`, `lib/Ldx12d.lib` for Debug, `lib/Ldx12.lib` for Release and the CMake package files under `lib/cmake/Ldx12`. It does not contain the implementation `.cpp` files. CMake automatically selects the library matching the consumer configuration.

## Build the example

Point `CMAKE_PREFIX_PATH` at that installation, then configure, build and run the independent consumer:

```bat
cmake -S examples/InstalledLdx12 -B build-consumer -DCMAKE_PREFIX_PATH=C:\Ldx12
cmake --build build-consumer --config Release --parallel
build-consumer\Release\Ldx12InstalledExample.exe
```
