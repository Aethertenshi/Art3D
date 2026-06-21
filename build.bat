@echo off
setlocal

rem Art3DC++ Build Script
rem Uses MSBuild from Visual Studio
rem Usage:   build.bat                (Debug x64)
rem          build.bat Release        (Release x64)
rem          build.bat Debug x64

set CONFIG=Debug
set PLATFORM=x64

if not "%~1"=="" set CONFIG=%~1
if not "%~2"=="" set PLATFORM=%~2

rem Locate MSBuild via vswhere if available, or use known VS paths
set MSBUILD=
for %%v in (18 17 16) do (
    if exist "C:\Program Files\Microsoft Visual Studio\%%v\Community\MSBuild\Current\Bin\MSBuild.exe" (
        set MSBUILD=C:\Program Files\Microsoft Visual Studio\%%v\Community\MSBuild\Current\Bin\MSBuild.exe
        goto :found
    )
    if exist "C:\Program Files\Microsoft Visual Studio\%%v\Professional\MSBuild\Current\Bin\MSBuild.exe" (
        set MSBUILD=C:\Program Files\Microsoft Visual Studio\%%v\Professional\MSBuild\Current\Bin\MSBuild.exe
        goto :found
    )
    if exist "C:\Program Files\Microsoft Visual Studio\%%v\Enterprise\MSBuild\Current\Bin\MSBuild.exe" (
        set MSBUILD=C:\Program Files\Microsoft Visual Studio\%%v\Enterprise\MSBuild\Current\Bin\MSBuild.exe
        goto :found
    )
)

rem Fallback: try vswhere
for /f "usebackq tokens=*" %%i in (`"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe 2^>nul`) do (
    set MSBUILD=%%i
    goto :found
)

echo ERROR: MSBuild not found. Install Visual Studio 2022+ with C++ workload.
exit /b 1

:found
echo Building %CONFIG% ^| %PLATFORM%
echo MSBuild: %MSBUILD%
echo.

"%MSBUILD%" Art3DC++.vcxproj /p:Configuration=%CONFIG% /p:Platform=%PLATFORM% /m /nologo /v:minimal

if %ERRORLEVEL% equ 0 (
    echo.
    echo Build succeeded.
) else (
    echo.
    echo Build failed with error %ERRORLEVEL%.
    exit /b %ERRORLEVEL%
)
