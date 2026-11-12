# Build UsdStudio

[VfxPlatform](https://vfxplatform.com/)

## Windows build

Install Visual Studio 2026 Community or Professional with C++ build tools,
CMake 4.2 or newer,
Python with development headers, and vcpkg. Make sure `python`, `cmake`, and
the desired `vcpkg.exe` are on `PATH`.

From a Command Prompt in the repository root, configure the project:

```bat
Setup.bat
```

Setup builds OpenUSD if its installation directory is absent, then configures
CMake in `WinBuild\VS2026`, leaving an existing VS 2022 `WinBuild` cache
untouched. Alternatively, if OpenUSD is already built, configure
CMake directly (adjust the vcpkg path):

```bat
set "VCPKG_ROOT=C:\Tools\vcpkg"
cmake -S . -B WinBuild\VS2026 -G "Visual Studio 18 2026" -A x64 "-DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows
```

Build the Release configuration:

```bat
cmake --build WinBuild\VS2026 --config Release --parallel
```

```bat
cmake --build WinBuild\VS2026 --config RelWithDebInfo --parallel
```

The Release executables are placed in `WinBuild\VS2026\bin\Release`. To build a single
target, add `--target UsdStudio` or `--target UsdStudioCmd`.

Building `UsdStudio` also deploys its Qt runtime and linked OpenUSD DLLs and
plugin metadata next to the executable.

The OpenUSD build installed by `ThirdParty\BuildUsd.bat` includes OpenSubdiv.
CMake uses its package configuration from that installation by default. To use
a different compatible installation, set `OpenSubdiv_DIR` when configuring.


