@echo off
setlocal
cd /d "%~dp0"

set "EXE="
if exist "build-win\Release\MyShooter.exe" set "EXE=build-win\Release\MyShooter.exe"
if not defined EXE if exist "build-win\MyShooter.exe" set "EXE=build-win\MyShooter.exe"
if not defined EXE if exist "build\MyShooter.exe" set "EXE=build\MyShooter.exe"

if not defined EXE (
    echo [CHIKENGUN 2.5] Building Windows native client via build.ps1...
    powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" %*
    if errorlevel 1 exit /b %errorlevel%
    if exist "build-win\Release\MyShooter.exe" set "EXE=build-win\Release\MyShooter.exe"
    if not defined EXE if exist "build-win\MyShooter.exe" set "EXE=build-win\MyShooter.exe"
)

if not defined EXE (
    echo ERROR: MyShooter.exe not found after build.
    exit /b 1
)

for %%F in ("%EXE%") do set "APP_DIR=%%~dpF"
cd /d "%APP_DIR%"
"%~dp0%EXE%" %*
