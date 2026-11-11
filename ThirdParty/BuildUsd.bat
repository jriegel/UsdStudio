@echo off
REM Build OpenUSD from source in both Debug and Release configurations
echo  [32mBuilding OpenUSD from source.[0m

echo  [94mSearch for Python:[0m
WHERE python
IF %ERRORLEVEL% NEQ 0 (
    ECHO [91mERROR: python.exe was not found in path![0m
    ECHO Please install Python and add it to your PATH.
    pause
    exit 1
)

echo  [94mSet Python library path:[0m
set PYTHON_LIB_DIR=C:\Users\mail\AppData\Local\Programs\Python\Python312\libs
set PYTHON_INCLUDE_DIR=C:\Users\mail\AppData\Local\Programs\Python\Python312\include
set PYTHON_EXECUTABLE=C:\Users\mail\AppData\Local\Programs\Python\Python312\python.exe
set PYTHON_LIBRARY=%PYTHON_LIB_DIR%\python312.lib
set PYTHON_LIBRARY_DEBUG=%PYTHON_LIB_DIR%\python312_d.lib
set PYTHON_VERSION=3.12

if not exist %PYTHON_LIB_DIR% (
    ECHO [91mERROR: Python libs directory not found at %PYTHON_LIB_DIR%![0m
    ECHO Please adjust PYTHON_LIB_DIR to match your Python installation.
    pause
    exit 1
)
if not exist %PYTHON_INCLUDE_DIR% (
    ECHO [91mERROR: Python include directory not found at %PYTHON_INCLUDE_DIR%![0m
    ECHO Please adjust PYTHON_INCLUDE_DIR to match your Python installation.
    pause
    exit 1
)
if not exist %PYTHON_LIBRARY% (
    ECHO [91mERROR: Python library not found at %PYTHON_LIBRARY%![0m
    ECHO Please adjust PYTHON_LIBRARY to match your Python installation.
    pause
    exit 1
)
if not exist %PYTHON_LIBRARY_DEBUG% (
    ECHO [91mERROR: Python debug library not found at %PYTHON_LIBRARY_DEBUG%![0m
    ECHO Please adjust PYTHON_LIBRARY_DEBUG to match your Python installation.
    pause
    exit 1
)
echo Using Python from %PYTHON_EXECUTABLE%
echo Using Python include from %PYTHON_INCLUDE_DIR%
echo Using Python library (Release) from %PYTHON_LIBRARY%
echo Using Python library (Debug) from %PYTHON_LIBRARY_DEBUG%

set USD_SRC_DIR=OpenUsd_Src
set USD_BUILD_DIR=OpenUsd_Build_Release
set USD_BUILD_DIR_DEBUG=OpenUsd_Build_Debug

echo  [94mSetting environment variables for CMake:[0m
set CMAKE_PREFIX_PATH=%PYTHON_LIB_DIR%;%CMAKE_PREFIX_PATH%
set LIB=%PYTHON_LIB_DIR%;%LIB%
set LIBPATH=%PYTHON_LIB_DIR%;%LIBPATH%

echo  [94mCleaning old build directories:[0m
if exist %USD_BUILD_DIR% (
    echo Removing %USD_BUILD_DIR%
    rmdir /s /q %USD_BUILD_DIR%
)

if exist %USD_BUILD_DIR_DEBUG% (
    echo Removing %USD_BUILD_DIR_DEBUG%
    rmdir /s /q %USD_BUILD_DIR_DEBUG%
)

echo  [94mCreating fresh build directories:[0m
if not exist %USD_BUILD_DIR% (
    mkdir %USD_BUILD_DIR%
)

if not exist %USD_BUILD_DIR_DEBUG% (
    mkdir %USD_BUILD_DIR_DEBUG%
)

rem Debug-Version bauen (not possible at the moment)
rem python %USD_SRC_DIR%/build_scripts/build_usd.py %USD_BUILD_DIR_DEBUG% --build-python-info %PYTHON_EXECUTABLE% %PYTHON_INCLUDE_DIR% %PYTHON_LIBRARY% %PYTHON_VERSION%  --imaging --no-docs --no-tests --build-variant debug

rem Release-Version bauen
python %USD_SRC_DIR%/build_scripts/build_usd.py %USD_BUILD_DIR% --build-python-info %PYTHON_EXECUTABLE% %PYTHON_INCLUDE_DIR% %PYTHON_LIBRARY% %PYTHON_VERSION% --imaging --python --no-docs --no-tests --build-variant release

