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

### Troubleshooting: a vcpkg port fails in `applocal.ps1`

If configuring fails with a port such as `pkgconf:x64-windows` reported as
`BUILD_FAILED` and the failing command is `pwsh.exe ... scripts/buildsystems/msbuild/applocal.ps1`
with `Error code: 70`, the port itself built fine. What failed is vcpkg's
post-build step, which runs `applocal.ps1` in PowerShell. Look for
`copy-tool-dependencies-*-err.log` in the port's build tree
(`--x-buildtrees-root`, here `C:\temp\pkgconf`) and you will find instead of a
compiler error:

```
The type initializer for 'System.Management.Automation.ModuleIntrinsics' threw an exception.
```

That is a startup crash of the Microsoft Store (MSIX) preview of PowerShell 7.7,
which is reached through the `%LOCALAPPDATA%\Microsoft\WindowsApps\pwsh.exe`
app-execution alias. It happens when the Documents folder is redirected, e.g. to
OneDrive, while `PSModulePath` in the registry is empty; see
[PowerShell#26536](https://github.com/PowerShell/PowerShell/issues/26536).
PowerShell 7.6.x does not have the bug.

`Setup.bat` avoids it by running `vcpkg fetch powershell-core`, which installs a
portable PowerShell into `<vcpkg>\downloads\tools\` and makes vcpkg use that copy
instead of any `pwsh.exe` on `PATH`. To fix the machine itself instead, remove or
update the Store PowerShell preview, install the PowerShell 7.6.x MSI from
<https://github.com/PowerShell/PowerShell/releases>, or set a non-empty
`PSModulePath` value under `HKCU\Environment`.

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

