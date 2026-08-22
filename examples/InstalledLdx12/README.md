# Installed Ldx12 example

This project is intentionally independent from the Ldx12 source tree. It only consumes the installed CMake package.

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
cmake -S examples/InstalledLdx12 -B build-consumer -DCMAKE_PREFIX_PATH=C:\Ldx12
cmake --build build-consumer --config Release --parallel
build-consumer\Release\Ldx12InstalledExample.exe
```
