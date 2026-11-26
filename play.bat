@echo off
REM ============================================================================
REM CHIKENGUN 2.4 - LOCAL LAUNCH ON WINDOWS (no internet, no install except Python)
REM   play.bat            - build offline single-file version and open it
REM   play.bat --server   - local server on http://127.0.0.1:8080
REM   play.bat --rebuild  - force rebuild of CHIKENGUN_OFFLINE.html
REM ============================================================================
setlocal
cd /d "%~dp0"

set MODE=offline
set REBUILD=0
:parse
if "%~1"=="" goto run
if /i "%~1"=="--server"  set MODE=server
if /i "%~1"=="--rebuild" set REBUILD=1
shift
goto parse

:run
where python >nul 2>nul
if errorlevel 1 (
  where py >nul 2>nul
  if errorlevel 1 (
    echo [play] Python not found. Install it from python.org ^(tick "Add to PATH"^),
    echo        then run play.bat again. The game itself needs no Python to play.
    pause
    exit /b 1
  )
  set PY=py
) else (
  set PY=python
)

if /i "%MODE%"=="server" (
  echo [play] Server: http://127.0.0.1:8080/  ^(Ctrl+C to stop^)
  start "" "http://127.0.0.1:8080/"
  %PY% -m http.server 8080
  goto end
)

if "%REBUILD%"=="1" (
  echo [play] Building offline single-file version...
  %PY% tools\build_web.py
) else (
  if not exist "CHIKENGUN_OFFLINE.html" (
    echo [play] Building offline single-file version...
    %PY% tools\build_web.py
  )
)

echo [play] Opening CHIKENGUN_OFFLINE.html ...
start "" "CHIKENGUN_OFFLINE.html"
echo [play] Done. If the browser did not open, just double-click CHIKENGUN_OFFLINE.html

:end
endlocal
