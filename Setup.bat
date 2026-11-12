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

if not exist "%~dp0ThirdParty\OpenUsd_Build_Release\" (
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

