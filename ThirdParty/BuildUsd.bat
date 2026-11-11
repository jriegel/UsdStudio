@echo off
REM Build OpenUSD from source in both Debug and Release configurations
echo  Building OpenUSD from source.

echo  Search for Python:
WHERE python
IF %ERRORLEVEL% NEQ 0 (
    ECHO ERROR: python.exe was not found in path!
    ECHO Please install Python and add it to your PATH.
    pause
    exit 1
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
python %USD_SRC_DIR%/build_scripts/build_usd.py %USD_BUILD_DIR% --imaging --python --no-docs --no-tests --build-variant release

