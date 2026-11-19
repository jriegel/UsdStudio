@echo off
REM Builds pxr/imaging/hgiVulkan (Hydra's Vulkan graphics backend) against the
REM installed OpenUSD and copies the result into that install tree.
REM Prerequisites: BuildUsd.bat has been run, and the Vulkan SDK is installed
REM (VULKAN_SDK points at it).  See Build.md.

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

if not defined VULKAN_SDK (
    echo ERROR: VULKAN_SDK is not set. Install the Vulkan SDK and point VULKAN_SDK at it.
    pause
    exit /b 1
)

set SRC_DIR=%~dp0BuildHgiVulkan
set BUILD_DIR=%~dp0HgiVulkan_Build
set USD_SRC_DIR=%~dp0OpenUsd_Src
set USD_ROOT=%~dp0OpenUsd_Build_Release

if not exist "%USD_ROOT%\pxrConfig.cmake" (
    echo ERROR: OpenUSD is not installed at %USD_ROOT%. Run BuildUsd.bat first.
    pause
    exit /b 1
)

cmake -S "%SRC_DIR%" -B "%BUILD_DIR%" -DOPENUSD_SRC="%USD_SRC_DIR%" -DOPENUSD_ROOT="%USD_ROOT%"
if errorlevel 1 exit /b 1

cmake --build "%BUILD_DIR%" --config Release
if errorlevel 1 exit /b 1

cmake --install "%BUILD_DIR%" --config Release --prefix "%USD_ROOT%"
if errorlevel 1 exit /b 1

echo.
echo Installed usd_hgiVulkan.dll into %USD_ROOT%\lib
exit /b 0
