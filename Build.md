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
CMake uses its OpenSubdiv and TBB package configurations from that installation
by default when available. To use different compatible installations, set
`OpenSubdiv_DIR` or `TBB_DIR` when configuring.

## Linux and macOS build

Install a C++ compiler, CMake 3.30 or newer, Python 3 with development
headers, and vcpkg. Install the system development libraries required by
OpenUSD and the vcpkg Qt ports; on macOS, install the Xcode command-line tools.
On Debian/Ubuntu, vcpkg's dependency builds also require `bison`, `flex`,
`autoconf`, `autoconf-archive`, `automake`, and `libtool`. For Qt's X11
backend, install `^libxcb.*-dev`, `libx11-xcb-dev`, `libglu1-mesa-dev`,
`libxrender-dev`, `libxi-dev`, `libxkbcommon-dev`, `libxkbcommon-x11-dev`,
and `libegl1-mesa-dev`.
The OpenUSD build downloads and compiles its dependencies and may take a long
time. Set `VCPKG_ROOT` to a writable vcpkg checkout, or put its executable on
`PATH`.

From any directory, run:

```sh
/path/to/UsdStudio/Setup.sh
cmake --build /path/to/UsdStudio/UnixBuild --parallel
```

Setup builds OpenUSD when its `pxrConfig.cmake` is missing, then configures a
Release build in `UnixBuild`. Additional arguments to `Setup.sh` are passed to
CMake, for example `./Setup.sh -DBUILD_GUI=OFF`. To build or update OpenUSD
separately, run `./ThirdParty/BuildUsd.sh`; it reuses its build directories
rather than deleting them. Arguments are passed to OpenUSD's build script, for
example `./ThirdParty/BuildUsd.sh --jobs 4`. Python bindings are installed
inside the OpenUSD installation directory.

The executables are in `UnixBuild/bin`. Unlike the Windows build, Qt and
OpenUSD runtimes are not deployed alongside the GUI executable.

