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


REM Check if the directory thirdParty\USD-Build exists
IF NOT EXIST "thirdParty\USD-Build" (
    ECHO [91mERROR: The directory thirdParty\USD-Build was not found![0m
    ECHO Please use thirdParty/BuildUsd.bat to build the USD library first.
    pause
    goto End
)


echo  [94mCheck for compiler version:[0m
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community" (
  SET GENERATOR_STRING="Visual Studio 17 2022"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional" (
  SET GENERATOR_STRING="Visual Studio 17 2022"
)else (
  ECHO [91mERROR: compiler whas not found ! [0m
  pause
  exit 1
)
echo Using cMake generator: %GENERATOR_STRING%

echo  [94mGenerate cMake[0m

cmake.exe ^
	-B WinBuild ^
	-G %GENERATOR_STRING% ^
	-A x64 ^
	-DCMAKE_TOOLCHAIN_FILE=%VCPKG_DIR%/scripts/buildsystems/vcpkg.cmake ^
	-DVCPKG_TARGET_TRIPLET=x64-windows  ^
	.
IF %ERRORLEVEL% NEQ 0 (
    ECHO [91mERROR: cMake generation![0m
    pause
    exit 1
)



echo  [94m Initial build RelWithDebInfo[0m

cmake.exe ^
	--build ^
	WinBuild ^
	--config RelWithDebInfo 
	
IF %ERRORLEVEL% NEQ 0 (
    ECHO [91mERROR: cMake Build ![0m
    pause
    exit 1
)

@echo on

md WinBuild\bin\RelWithDebInfo\lib\usd
xcopy /Y thirdParty\USD-Build\lib\*.dll WinBuild\bin\RelWithDebInfo\
xcopy /Y thirdParty\USD-Build\lib\*.pdb WinBuild\bin\RelWithDebInfo\
xcopy /s /Y thirdParty\USD-Build\lib\usd WinBuild\bin\RelWithDebInfo\lib\usd
xcopy /s /Y thirdParty\USD-Build\bin\ WinBuild\bin\RelWithDebInfo\
call WinBuild\vcpkg_installed\x64-windows\tools\Qt6\bin\windeployqt6.exe WinBuild\bin\RelWithDebInfo\UsdStudio.exe


rem debug not working so far
goto End

echo  [94m Initial build Debug[0m

cmake.exe ^
	--build ^
	WinBuild ^
	--config Debug 
	
IF %ERRORLEVEL% NEQ 0 (
    ECHO [91m ERROR: cMake generation![0m
    pause
    exit 1
)

call WinBuild\vcpkg_installed\x64-windows\tools\Qt6\bin\windeployqt.debug.bat WinBuild\bin\Debug\UsdStudio.exe

:End

echo 

echo  [94m Finish Setup Developer Environment[0m