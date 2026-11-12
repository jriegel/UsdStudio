@echo off
REM Build OpenUSD from source in Release configuration
echo  Building OpenUSD from source


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

echo  Search for Python:
WHERE python
IF %ERRORLEVEL% NEQ 0 (
    ECHO ERROR: python.exe was not found in path!
    ECHO Please install Python and add it to your PATH.
    pause
    exit /b 1
)

set USD_SRC_DIR=OpenUsd_Src
set USD_BUILD_DIR=OpenUsd_Build_Release


echo  Cleaning old build directories:
if exist %USD_BUILD_DIR% (
    echo Removing %USD_BUILD_DIR%
    rmdir /s /q %USD_BUILD_DIR%
)

echo  Creating fresh build directories:
if not exist %USD_BUILD_DIR% (
    mkdir %USD_BUILD_DIR%
)


rem Release-Version bauen
python %USD_SRC_DIR%/build_scripts/build_usd.py %USD_BUILD_DIR% --imaging --no-docs --no-tests --embree --build-variant release --generator "Visual Studio 18 2026"
