@echo off
REM SetupScript Juergen Riegel mail@juergen-riegel.net
echo  [32mSetup the build environment.[0m

echo  [94mSearch for cMake:[0m
WHERE cmake
IF %ERRORLEVEL% NEQ 0 (
    ECHO [91mERROR: cmake.exe whas not found in path! Download and install cmake first! [0m
    ECHO See: https://cmake.org/download/ on details for you system.
    pause
    exit 1
)

echo  [94mSearch for VCPKG:[0m
WHERE vcpkg
IF %ERRORLEVEL% NEQ 0 (
    ECHO [91mERROR: vcpkg.exe whas not found in path! Download and bootstrap vcpkg first! [0m
    ECHO See: https://github.com/microsoft/vcpkg on details for you system.
    pause
    exit 1
)
FOR /F "tokens=* USEBACKQ" %%F IN (`where.exe vcpkg.exe`) DO (
 SET VCPKG_DIR=%%~dpF
 GoTo Found
)
:Found
echo Using VCPKG in %VCPKG_DIR%

set actDir="%~dp0"


echo  [94mInstall dependencies:[0m



echo  [94mCheck for compiler version:[0m
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community" (
	SET VC_DIR=C:\Program Files\Microsoft Visual Studio\2022\Community
	SET GENERATOR_STRING="Visual Studio 17 2022"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional" (
	SET VC_DIR=C:\Program Files\Microsoft Visual Studio\2022\Community
	SET GENERATOR_STRING="Visual Studio 17 2022"
)else (
	ECHO [91mERROR: compiler whas not found ! [0m
	pause
	exit 1
)
echo Using cMake generator: %GENERATOR_STRING%

echo  [94mSetup Visual Studio Build env:[0m
call "%VC_DIR%\VC\Auxiliary\Build\vcvars64.bat"

rem allways clean build
rmdir /s /q USD-Build

cd OpenUSD

echo  [94mCall USD Build script for release:[0m
python ^
	build_scripts\build_usd.py ^
		--build-variant relwithdebuginfo ^
		--ptex ^
		--embree ^
		--alembic ^
		--hdf5 ^
		--draco ^
		%~dp0\USD-Build" 
IF %ERRORLEVEL% NEQ 0 (
    ECHO [91mERROR: build_scripts\build_usd.py returned with error! [0m
	cd ..
    pause
    exit 1
)

cd ..

echo  [32mBuild finished.[0m

exit 0

echo  [94mCall USD Build script for release:[0m
python ^
	build_scripts\build_usd.py ^
		--build-variant debug ^
		--debug-python ^
		--ptex ^
		--openvdb ^
		--embree ^
		--opencolorio ^
		--alembic ^
		--hdf5 ^
		--draco ^
		%~dp0\USD-Dbg-Build" 
IF %ERRORLEVEL% NEQ 0 (
    ECHO [91mERROR: build_scripts\build_usd.py returned with error! [0m
	cd ..
    pause
    exit 1
)

cd ..

echo  [32mBuild finished.[0m


