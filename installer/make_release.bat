@echo off
rem Makes the Windows installer for SourceForge: installer\output\WSPR-Scheduler-2-<version>-setup.exe
rem
rem Before running it:
rem  1. Qt Creator: choose the Release build (bottom left, the computer icon), then Build.
rem  2. Once only: put the MySQL files in installer\thirdparty (see installer\README.md):
rem       thirdparty\sqldrivers\qsqlmysql.dll
rem       thirdparty\libmysql.dll  thirdparty\libcrypto-3-x64.dll  thirdparty\libssl-3-x64.dll
rem     and, if you want them in the installer, thirdparty\wspr_sked2_help.rtf (the F1 help) and
rem     thirdparty\wspr_db.sql (both copied to C:\WSPR_Sked, never over ones already there)
rem  3. Inno Setup 7 (or 6) installed (https://jrsoftware.org/isdl.php).
rem Then double-click this file.
setlocal EnableExtensions EnableDelayedExpansion
set "HERE=%~dp0"
set "ROOT=%HERE%.."
set "STAGE=%HERE%stage"
set "THIRD=%HERE%thirdparty"

rem ---- the Release build folder (the newest one with WsprSked.exe in it) ----
set "BUILD="
for /f "delims=" %%D in ('dir /b /ad /o:d "%ROOT%\build\*Release*" 2^>nul') do (
    if exist "%ROOT%\build\%%D\WsprSked.exe" set "BUILD=%ROOT%\build\%%D"
)
if not defined BUILD (
    echo No Release build found in %ROOT%\build
    echo In Qt Creator choose the Release build and Build it first.
    goto :fail
)
echo Release build: %BUILD%

rem ---- Qt and MinGW folders, from the build's CMake cache ----
set "QT6DIR="
set "CXX="
for /f "tokens=1,* delims==" %%A in ('findstr /b /c:"Qt6_DIR:" "%BUILD%\CMakeCache.txt"') do set "QT6DIR=%%B"
for /f "tokens=1,* delims==" %%A in ('findstr /b /c:"CMAKE_CXX_COMPILER:" "%BUILD%\CMakeCache.txt"') do set "CXX=%%B"
if not defined QT6DIR (
    echo Qt6_DIR not found in %BUILD%\CMakeCache.txt
    goto :fail
)
set "QT6DIR=%QT6DIR:/=\%"
for %%Q in ("%QT6DIR%\..\..\..\bin") do set "QTBIN=%%~fQ"
if not exist "%QTBIN%\windeployqt.exe" (
    echo windeployqt.exe not found in %QTBIN%
    goto :fail
)
if defined CXX (
    set "CXX=!CXX:/=\!"
    for %%C in ("!CXX!") do set "PATH=%%~dpC;!PATH!"
)
set "PATH=%QTBIN%;%PATH%"
echo Qt: %QTBIN%

rem ---- MySQL files (not part of Qt) ----
set "MISSING="
for %%F in (sqldrivers\qsqlmysql.dll libmysql.dll libcrypto-3-x64.dll libssl-3-x64.dll) do (
    if not exist "%THIRD%\%%F" (
        echo Missing: %THIRD%\%%F
        set "MISSING=1"
    )
)
if defined MISSING (
    echo Copy these from the build folder you run WSPR Scheduler 2 from - see installer\README.md
    goto :fail
)

rem ---- stage: everything that gets installed ----
if exist "%STAGE%" rd /s /q "%STAGE%"
mkdir "%STAGE%\sqldrivers"
copy /y "%BUILD%\WsprSked.exe" "%STAGE%\WsprSked.exe" >nul
if errorlevel 1 goto :fail
windeployqt --release --no-translations --compiler-runtime --no-system-d3d-compiler --no-opengl-sw "%STAGE%\WsprSked.exe"
if errorlevel 1 goto :fail
rem the MinGW run-time, in case windeployqt couldn't find the compiler
for %%F in (libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll) do (
    if not exist "%STAGE%\%%F" if exist "%QTBIN%\%%F" copy /y "%QTBIN%\%%F" "%STAGE%\%%F" >nul
)
copy /y "%THIRD%\sqldrivers\qsqlmysql.dll" "%STAGE%\sqldrivers\qsqlmysql.dll" >nul
if errorlevel 1 goto :fail
for %%F in (libmysql.dll libcrypto-3-x64.dll libssl-3-x64.dll) do (
    copy /y "%THIRD%\%%F" "%STAGE%\%%F" >nul
    if errorlevel 1 goto :fail
)
if not exist "%STAGE%\tls" echo Warning: no tls folder - the Remote tab's web page won't start

rem ---- compile the installer ----
set "ISCC="
rem Inno Setup 7 or 6 (7 comes as 32-bit and 64-bit editions, in Program Files or Program Files (x86))
for %%V in (7 6) do for %%I in ("%ProgramFiles%\Inno Setup %%V\ISCC.exe" "%ProgramFiles(x86)%\Inno Setup %%V\ISCC.exe" "%LocalAppData%\Programs\Inno Setup %%V\ISCC.exe") do (
    if not defined ISCC if exist %%I set "ISCC=%%~I"
)
if not defined ISCC for %%I in (ISCC.exe) do if not "%%~$PATH:I"=="" set "ISCC=%%~$PATH:I"
if not defined ISCC (
    echo Inno Setup not found - install it from https://jrsoftware.org/isdl.php
    echo or tell this file where ISCC.exe is: set "ISCC=C:\...\ISCC.exe" near this line
    goto :fail
)
echo Inno Setup: %ISCC%
set "EXTRA="
if exist "%THIRD%\wspr_sked2_help.rtf" (
    set "EXTRA=!EXTRA! /DHaveHelp"
    echo Including wspr_sked2_help.rtf
)
if exist "%THIRD%\wspr_db.sql" (
    set "EXTRA=!EXTRA! /DHaveDbSql"
    echo Including wspr_db.sql
)
if exist "%HERE%output" rd /s /q "%HERE%output"
"%ISCC%" /Q %EXTRA% "%HERE%wspr-scheduler-2.iss"
if errorlevel 1 goto :fail
if not exist "%HERE%output\*.exe" goto :fail

echo.
echo Done:
dir /b "%HERE%output\*.exe"
echo in %HERE%output
echo.
pause
exit /b 0

:fail
echo.
echo Installer NOT made.
pause
exit /b 1
