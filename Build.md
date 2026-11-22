# Build UsdStudio

[VfxPlatform](https://vfxplatform.com/)

## Windows build

Install Visual Studio 2026 Community or Professional with C++ build tools,
CMake 4.2 or newer,
Python with development headers, and vcpkg. Make sure `python`, `cmake`, and
the desired `vcpkg.exe` are on `PATH`.
Setup uses a usable SDK selected by `VULKAN_SDK` or the Windows environment
registry; otherwise, it attempts to install the Vulkan SDK through WinGet.

From a Command Prompt in the repository root, configure the project:

```bat
Setup.bat
```

Setup builds OpenUSD if its installation directory is absent, then configures
CMake in `WinBuild\VS2026`, leaving an existing VS 2022 `WinBuild` cache
untouched. The Windows OpenUSD build includes USD imaging but omits the
standalone `usdview` application, so PySide is not required. Setup also builds
HgiVulkan when its installed library or plugin is missing, including on a retry
after an interrupted HgiVulkan build. Alternatively, if OpenUSD is already
built, configure CMake directly (adjust the vcpkg path):

```bat
set "VCPKG_ROOT=C:\Tools\vcpkg"
cmake -S . -B WinBuild\VS2026 -G "Visual Studio 18 2026" -A x64 "-DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows
```

Multi-configuration generators offer only `Release` and `RelWithDebInfo`,
matching the OpenUSD release build. Reconfigure an existing build to update its
configuration list.

Build the Release configuration:

```bat
cmake --build WinBuild\VS2026 --config Release --parallel
```

```bat
cmake --build WinBuild\VS2026 --config RelWithDebInfo --parallel
```

The Release executables are placed in `WinBuild\VS2026\bin\Release`. To build a single
target, add `--target UsdStudio` or `--target UsdStudioCmd`.

### Hydra viewer prerequisites (HgiVulkan)

The Hydra viewport (`View > Hydra Viewer`) renders Storm through
`HgiVulkan`, which the OpenUSD build script does not package on Windows.
`ThirdParty\BuildUsd.bat` uses `--usd-imaging` so the `usdImaging` /
`usdImagingGL` bridge libraries are installed. Then build and install the
HgiVulkan shared library into the same OpenUSD tree with

```bat
ThirdParty\BuildHgiVulkan.bat
```

It needs the Vulkan SDK (default `C:\VulkanSDK`, override with `VULKAN_SDK`)
and vendors VulkanMemoryAllocator in `ThirdParty\vma`. CMake picks the
installed `usd_hgiVulkan` import up automatically when present. The Qt6
vcpkg manifest enables the `qtbase` `vulkan` feature for QRhi.

Building `UsdStudio` also deploys its Qt runtime and linked OpenUSD DLLs and
plugin metadata next to the executable.

### Running the tests

The headless test executables are registered with CTest:

```bat
ctest --test-dir WinBuild\VS2026 -C Release --output-on-failure
```

`UsdStudioAppTests` covers the Qt-free command framework (command registry,
argument parsing, undo/redo) against `Data\Kitchen_set`; `UsdStudioCliTests`
covers the JSON CLI contract (envelope, flags, exit codes, batch mode) against
the same data; `UsdStudioGuiTests`
covers the Qt `QUndoStack` bridge; `UsdStageModelTests` covers the lazy
`UsdStageModel` behind the Scene Graph dock (chunked `fetchMore`, path/index
lookups, filtering and `UsdNotice` refresh) and also uses `Data\Kitchen_set`.
All four need the Release configuration built first.

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

## Command line interface (`UsdStudioCmd`)

`UsdStudioCmd.exe` (built from `App\Main`) is a headless, non-interactive
front-end designed for scripts and AI agents. It speaks the same command
registry as the GUI; the GUI itself never parses text.

```bat
WinBuild\VS2026\bin\Release\UsdStudioCmd.exe prim.define /Foo --type Xform
WinBuild\VS2026\bin\Release\UsdStudioCmd.exe --command "prim.define /Foo" --command "attr.set /Foo width 1.5"
WinBuild\VS2026\bin\Release\UsdStudioCmd.exe --agent-help
```

Each process starts with an empty document, so multi-step work runs as one
batch of `--command "..."` lines (or piped stdin lines). Every operation
answers with one deterministic JSON object on stdout; logs and notes never
touch stdout and can be discarded with `2>nul`. Self-documentation:
`--help`, `--schema` (machine-readable flags, commands, queries, actions,
value types and exit codes), `--agent-help` (short token-cheap guide),
`--types`. Useful flags: `--limit`, `--fields`, `--dry-run`, `--text`,
`--pretty`/`--compact`, `--yes`. Errors carry a machine `code`, a resource in
`context` and a human `hint`; unknown operation names include nearest-name
suggestions. Exit codes equal the error codes: `0` ok, `2` usage, `3`
invalidValue, `4` notFound, `5` failed/precondition, `6` internal error.
The CLI executable does not deploy the OpenUSD DLLs next to itself; put the
OpenUSD `bin` directory (e.g. `ThirdParty\OpenUsd_Build_Release\bin`) on
`PATH` when running it.

### Remote control of the running GUI

`--remote` sends the operations to the running UsdStudio GUI instead of a
local process: live document, shared undo history, identical envelopes (each
answer gains an `"exit":<code>` member which the CLI mirrors as its process
exit code). The GUI listens on the named pipe `usdstudio-remote` by default
(toggle in the File menu, active pipe name in the status bar); a second GUI
instance falls back to `usdstudio-remote-<pid>`, selectable with
`--pipe <name>`. No listener: exit `5` with a hint.

```bat
WinBuild\VS2026\bin\Release\UsdStudioCmd.exe --remote stage.info
WinBuild\VS2026\bin\Release\UsdStudioCmd.exe --remote stage.find Stove --limit 5
WinBuild\VS2026\bin\Release\UsdStudioCmd.exe --remote attr.set /Foo width 1.5
WinBuild\VS2026\bin\Release\UsdStudioCmd.exe --remote history.undo
```

For agents driving a live session, see `Doc\Agent-CLI-Skill.md`.

## Linux and macOS build

Install a C++ compiler, CMake 3.30 or newer, Python 3 with development
headers, and vcpkg. Install the system development libraries required by
OpenUSD and the vcpkg Qt ports; on macOS, install the Xcode command-line tools.
On Debian/Ubuntu, vcpkg's dependency builds also require `bison`, `flex`,
`autoconf`, `autoconf-archive`, `automake`, and `libtool`. For Qt's X11
backend, install `^libxcb.*-dev`, `libx11-xcb-dev`, `libglu1-mesa-dev`,
`libxrender-dev`, `libxi-dev`, `libxkbcommon-dev`, `libxkbcommon-x11-dev`,
and `libegl1-mesa-dev`.
The Hydra viewer requires a Vulkan-capable driver and the Vulkan SDK,
including shaderc and the Vulkan utility headers. On Linux x86-64, `Setup.sh`
downloads the pinned LunarG SDK into `ThirdParty/VulkanSDK`, checks its SHA-256
checksum, and reuses it on subsequent runs. It does not install GPU drivers.
An existing SDK can be selected with `VULKAN_SDK` instead; on other platforms,
install and select a suitable SDK manually. Qt must include Vulkan support, Qt Quick,
ShaderTools, and the matching Qt Gui private development headers. The
repository supplies the Vulkan Memory Allocator headers used by OpenUSD.
Follow the SDK's environment setup instructions so its runtime libraries
are discoverable when running the application.
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

The GUI needs OpenUSD with USD Imaging and HgiVulkan. `BuildUsd.sh` enables
both. To add or rebuild HgiVulkan against an existing OpenUSD installation
with USD Imaging, run `./ThirdParty/BuildHgiVulkan.sh`; it rebuilds and
replaces Hgi and HgiInterop as well as HgiVulkan. Stop applications using
that installation first, and use matching OpenUSD sources. If USD Imaging is
missing, rebuild OpenUSD with `./ThirdParty/BuildUsd.sh` and rerun Setup.
Likewise, rebuild OpenUSD if its CMake package exports a native HgiVulkan
target but the corresponding library is missing.
Both scripts automatically find the SDK installed by Setup.

The executables are in `UnixBuild/bin`. Unlike the Windows build, Qt and
OpenUSD runtimes are not deployed alongside the GUI executable.
Launch the GUI with `./RunUsdStudio.sh` (or pass a different build directory
as its first argument). OpenUSD reads `HGI_ENABLE_VULKAN` during library
initialization, so the launcher sets it to `1` before starting the executable.
Alternatively, export `HGI_ENABLE_VULKAN=1` before running the GUI directly.
Run `ctest --test-dir UnixBuild --output-on-failure` after building; GUI
tests require a usable Qt platform plugin. Remote control uses Unix-domain
sockets in `TMPDIR` (or `/tmp` when `TMPDIR` is unset).

