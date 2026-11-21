@echo off
setlocal EnableDelayedExpansion

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

echo [1/3] Installing LunarG Vulkan SDK via WinGet...
winget install -e --id KhronosGroup.VulkanSDK --accept-package-agreements --accept-source-agreements
if %ERRORLEVEL% neq 0 (
    echo Note: WinGet returned code %ERRORLEVEL% ^(it may already be installed^). Continuing...
)

echo [2/3] Reloading environment variables from Registry...
endlocal & call :ReloadEnv
goto :Verify

:ReloadEnv
for /f "tokens=1,2*" %%A in ('reg query "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Environment" 2^>nul') do (
    if /i not "%%A"=="PATH" (
        if "%%B"=="REG_SZ" set "%%A=%%C"
        if "%%B"=="REG_EXPAND_SZ" call set "%%A=%%C"
    )
)
for /f "tokens=1,2*" %%A in ('reg query "HKCU\Environment" 2^>nul') do (
    if /i not "%%A"=="PATH" (
        if "%%B"=="REG_SZ" set "%%A=%%C"
        if "%%B"=="REG_EXPAND_SZ" call set "%%A=%%C"
    )
)
set "SYS_PATH="
set "USR_PATH="
for /f "tokens=1,2*" %%A in ('reg query "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Environment" /v Path 2^>nul') do (
    if /i "%%A"=="Path" call set "SYS_PATH=%%C"
)
for /f "tokens=1,2*" %%A in ('reg query "HKCU\Environment" /v Path 2^>nul') do (
    if /i "%%A"=="Path" call set "USR_PATH=%%C"
)
if defined SYS_PATH if defined USR_PATH (
    set "PATH=%SYS_PATH%;%USR_PATH%"
) else if defined SYS_PATH (
    set "PATH=%SYS_PATH%"
) else if defined USR_PATH (
    set "PATH=%USR_PATH%"
)
exit /b 0

:Verify
echo [3/3] Verifying Vulkan installation...
if not defined VULKAN_SDK (
    echo ERROR: VULKAN_SDK is still not set. Try opening a new terminal.
    exit /b 1
)

echo VULKAN_SDK = %VULKAN_SDK%
where vulkaninfo >nul 2>&1
if %ERRORLEVEL% equ 0 (
    echo SUCCESS: vulkaninfo found in PATH!
    vulkaninfo --summary
) else (
    echo WARNING: vulkaninfo not found in PATH. Check %VULKAN_SDK%\Bin
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
    call BuildHgiVulkan.bat
    if errorlevel 1 (
        popd
        echo ERROR: HgiVulkan build failed.
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

