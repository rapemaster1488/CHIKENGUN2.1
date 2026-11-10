@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

if /i "%~1"=="--web" goto launch_web
if /i "%~1"=="/web" goto launch_web

if exist "build\Release\MyShooter.exe" (
    start "" "build\Release\MyShooter.exe"
    exit /b 0
)
if exist "build\MyShooter.exe" (
    start "" "build\MyShooter.exe"
    exit /b 0
)

:launch_web
set "CG_PORT=8080"

where py >nul 2>nul
if %errorlevel%==0 (
    start "" powershell -NoProfile -WindowStyle Hidden -Command "Start-Sleep -Milliseconds 700; Start-Process 'http://127.0.0.1:%CG_PORT%/index.html'"
    py -3 tools\serve.py --host 0.0.0.0 --port %CG_PORT% --auto-port
    exit /b %errorlevel%
)

where python >nul 2>nul
if %errorlevel%==0 (
    start "" powershell -NoProfile -WindowStyle Hidden -Command "Start-Sleep -Milliseconds 700; Start-Process 'http://127.0.0.1:%CG_PORT%/index.html'"
    python tools\serve.py --host 0.0.0.0 --port %CG_PORT% --auto-port
    exit /b %errorlevel%
)

where node >nul 2>nul
if %errorlevel%==0 (
    start "" "http://127.0.0.1:%CG_PORT%/index.html"
    node -e "const http=require('http'),fs=require('fs'),path=require('path'),r=process.cwd(),m={'.html':'text/html; charset=utf-8','.js':'application/javascript','.png':'image/png','.wav':'audio/wav','.ogg':'audio/ogg','.xml':'application/xml','.css':'text/css'};const s=http.createServer((q,w)=>{let u=decodeURIComponent((q.url||'/').split('?')[0]);let p=path.normalize(u==='/'?'/index.html':u).replace(/^(\.\.[\/\\])+/,'');let f=path.join(r,p);if(!f.startsWith(r)){w.writeHead(403);return w.end();}fs.readFile(f,(e,d)=>{if(e){w.writeHead(404);return w.end();}w.writeHead(200,{'Content-Type':m[path.extname(f).toLowerCase()]||'application/octet-stream'});w.end(d);});});(function go(p){s.once('error',()=>go(p+1));s.listen(p,'127.0.0.1',()=>console.log('[CHIKENGUN 2.3] Open http://127.0.0.1:'+p+'/index.html'));})(%CG_PORT%);"
    exit /b %errorlevel%
)

where powershell >nul 2>nul
if %errorlevel%==0 (
    echo [CHIKENGUN 2.3] Starting built-in Windows PowerShell HTTP server...
    powershell -NoProfile -ExecutionPolicy Bypass -Command ^
      "$root = (Get-Location).Path;" ^
      "$port = 8080;" ^
      "$listener = $null;" ^
      "while ($port -lt 8100) { try { $l = New-Object System.Net.HttpListener; $l.Prefixes.Add(\"http://127.0.0.1:$port/\"); $l.Start(); $listener = $l; break } catch { $port++ } }" ^
      "if (-not $listener) { Start-Process (Join-Path $root 'index.html'); exit 0 }" ^
      "$mime = @{ '.html'='text/html; charset=utf-8'; '.js'='application/javascript; charset=utf-8'; '.css'='text/css'; '.png'='image/png'; '.jpg'='image/jpeg'; '.svg'='image/svg+xml'; '.wav'='audio/wav'; '.ogg'='audio/ogg'; '.xml'='application/xml'; '.json'='application/json'; '.ttf'='font/ttf' };" ^
      "$url = \"http://127.0.0.1:$port/index.html\";" ^
      "Write-Host \"[CHIKENGUN 2.3] Serving at $url (close this window to stop)\";" ^
      "Start-Process $url;" ^
      "while ($listener.IsListening) {" ^
      "  $ctx = $listener.GetContext();" ^
      "  $rel = [Uri]::UnescapeDataString($ctx.Request.Url.AbsolutePath.TrimStart('/'));" ^
      "  if ([string]::IsNullOrWhiteSpace($rel)) { $rel = 'index.html' }" ^
      "  $fp = [System.IO.Path]::GetFullPath((Join-Path $root $rel));" ^
      "  if ($fp.StartsWith($root) -and (Test-Path $fp -PathType Leaf)) {" ^
      "    $ext = [System.IO.Path]::GetExtension($fp).ToLowerInvariant();" ^
      "    $ctx.Response.ContentType = if ($mime.ContainsKey($ext)) { $mime[$ext] } else { 'application/octet-stream' };" ^
      "    $bytes = [System.IO.File]::ReadAllBytes($fp);" ^
      "    $ctx.Response.ContentLength64 = $bytes.Length;" ^
      "    $ctx.Response.OutputStream.Write($bytes, 0, $bytes.Length);" ^
      "  } else { $ctx.Response.StatusCode = 404 }" ^
      "  $ctx.Response.Close();" ^
      "}"
    exit /b 0
)

start "" "index.html"
