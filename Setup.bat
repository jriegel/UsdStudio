@echo off
REM SetupScript Juergen Riegel 
echo  [32mSetup the build environment.[0m

set "VS_VCVARS="
if exist "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" set "VS_VCVARS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
if not defined VS_VCVARS if exist "C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars64.bat" set "VS_VCVARS=C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars64.bat"

if not defined VS_VCVARS (
    echo ERROR: Visual Studio 2026 Community or Professional was not found.
    pause
    exit /b 1
)

call "%VS_VCVARS%"
if errorlevel 1 (
    echo ERROR: Failed to initialize the Visual Studio build environment.
    pause
    exit /b 1
)

REM Check for pxrConfig.cmake, not just the directory: an interrupted build_usd.py run
REM leaves the directory behind with only half of the dependencies in it, and
REM BuildUsd.bat recreates it from scratch.
if not exist "%~dp0ThirdParty\OpenUsd_Build_Release\pxrConfig.cmake" (
    echo OpenUSD build not found. Starting BuildUsd.bat...
    pushd "%~dp0ThirdParty"
    if errorlevel 1 (
        echo ERROR: Could not enter the ThirdParty directory.
        pause
        exit /b 1
    )
    call BuildUsd.bat
    if errorlevel 1 (
        popd
        echo ERROR: OpenUSD build failed.
        pause
        exit /b 1
    )
    popd
)

echo  [94mSearch for VCPKG:[0m

WHERE vcpkg
IF %ERRORLEVEL% NEQ 0 (
    ECHO [91mERROR: vcpkg.exe was not found![0m  
    pause
    exit 1
)
FOR /F "tokens=* USEBACKQ" %%F IN (`where.exe vcpkg.exe`) DO (
 SET VCPKG_DIR=%%~dpF
 GoTo Found
)
:Found
echo Using VCPKG in %VCPKG_DIR%

echo  [94mMake vcpkg use a portable PowerShell:[0m
REM vcpkg runs applocal.ps1 through pwsh.exe after building a port. If it resolves
REM the Microsoft Store (MSIX app-execution-alias) pwsh.exe, that pwsh can abort at
REM startup with "The type initializer for
REM 'System.Management.Automation.ModuleIntrinsics' threw an exception" when the
REM Documents folder is redirected to a OneDrive path. vcpkg then reports the port
REM as BUILD_FAILED even though the port itself compiled fine.
REM See https://github.com/PowerShell/PowerShell/issues/26536
REM 'vcpkg fetch powershell-core' downloads and extracts a portable PowerShell into
REM vcpkg's own tools directory and prints its path. vcpkg's build system uses that
REM portable copy instead of any pwsh.exe found on PATH, so fetching it up front keeps
REM the port builds away from the Microsoft Store copy.
set "VCPKG_PWSH="
FOR /F "tokens=* USEBACKQ" %%F IN (`"%VCPKG_DIR%vcpkg.exe" fetch powershell-core 2^>nul`) DO (
 SET VCPKG_PWSH=%%F
)
if defined VCPKG_PWSH (
    echo Using PowerShell for vcpkg: %VCPKG_PWSH%
) else (
    echo WARNING: vcpkg has no portable PowerShell, it may fall back to the
    echo          Microsoft Store pwsh.exe alias, which can fail at startup.
    echo          See Build.md for the manual workaround.
)

echo  [94mSearch for cMake:[0m
WHERE cmake
IF %ERRORLEVEL% NEQ 0 (
    ECHO [91mERROR: cmake.exe whas not found in path! Download and install CMAKE first! [0m
    ECHO See: https://cmake.org/ on details for you system.
    pause
    exit 1
)
   
  
echo  [94mCheck for compiler version:[0m
SET GENERATOR_STRING="Visual Studio 18 2026"
echo Using cMake generator: %GENERATOR_STRING%

cmake.exe -G %GENERATOR_STRING% -B "%~dp0WinBuild\VS2026" -S "%~dp0." -A x64 "-DCMAKE_TOOLCHAIN_FILE=%VCPKG_DIR%scripts\buildsystems\vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows -DVCPKG_INSTALL_OPTIONS="--x-buildtrees-root=C:\temp"

