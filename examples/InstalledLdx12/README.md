# Ldx12 consumer example

This project is intentionally independent from the Ldx12 source tree. The `LDX12_USE_FETCHCONTENT` option selects how it obtains the library:

- `ON` by default: download and build Ldx12 through `FetchContent`.
- `OFF`: use an installed package through `find_package`.

The executable also includes `Ldx12Native.hpp` to verify that optional native D3D12 access is installed correctly.
Consumers use the installed public headers and `Ldx12.lib`; the library's `.cpp` files are already compiled into the static library and are not distributed.

## Installed package

From the repository root, build and install the static library:

```bat
cmake -S . -B build-ldx12 ^
    -DLDX12_BUILD_APP=OFF ^
    -DLDX12_BUILD_EXAMPLES=OFF ^
    -DLDX12_BUILD_TESTS=OFF ^
    -DLDX12_INSTALL=ON
cmake --build build-ldx12 --config Release --target Ldx12 --parallel
cmake --install build-ldx12 --config Release --prefix C:\Ldx12
```

Configure, build and run this consumer:

```bat
cmake -S examples/InstalledLdx12 -B build-consumer -DLDX12_USE_FETCHCONTENT=OFF -DCMAKE_PREFIX_PATH=C:\Ldx12
cmake --build build-consumer --config Release --parallel
build-consumer\Release\Ldx12InstalledExample.exe
```

## FetchContent

No previous installation is required. From this example directory, the shortest flow is:

```bat
mkdir build
cd build
cmake ..
cmake --build . --config Release --parallel
Release\Ldx12InstalledExample.exe
```

`LDX12_FETCH_GIT_TAG` defaults to `main`. A project can pin a release or commit:

```bat
cmake -S examples/InstalledLdx12 -B build-consumer -DLDX12_USE_FETCHCONTENT=ON -DLDX12_FETCH_GIT_TAG=<tag-or-commit>
```
