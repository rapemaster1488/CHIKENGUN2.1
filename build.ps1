# ============================================================================
# CHIKENGUN 2.5 — ПЕРО СУДЬБЫ (Zero-Pain Windows Builder: MSVC / CMake)
# Использование в PowerShell:
#   .\build.ps1                # Собрать клиент и PBR текстуры (1024)
#   .\build.ps1 -Ultra4K       # Собрать с текстурами 4096x4096
#   .\build.ps1 -TestsOnly     # Собрать и прогнать только C++17 модульные тесты
# ============================================================================
[CmdletBinding()]
param(
    [switch]$Ultra4K,
    [switch]$TestsOnly,
    [int]$Jobs = [Environment]::ProcessorCount
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$TexSize = if ($Ultra4K) { 4096 } else { 1024 }
$BuildDir = Join-Path $Root "build-win"

function Require-Command([string]$Name, [string]$Hint) {
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw "Не найдена команда '$Name'. $Hint"
    }
}

Require-Command "cmake" "Установите CMake (winget install Kitware.CMake)"

function Invoke-Checked {
    param([scriptblock]$Script, [string]$ErrMsg)
    & $Script
    if ($LASTEXITCODE -ne 0) { throw "$ErrMsg (exit code $LASTEXITCODE)" }
}

if ($TestsOnly) {
    Write-Host "[Tests] Конфигурация и запуск C++17 модульных тестов..."
    Invoke-Checked { cmake -S $Root -B $BuildDir -DBUILD_GAME=OFF -DTEX_SIZE=$TexSize } "CMake configure failed"
    Invoke-Checked { cmake --build $BuildDir --config Release --parallel $Jobs --target gen_textures_4k test_core } "CMake build failed"
    Invoke-Checked { ctest --test-dir $BuildDir -C Release --output-on-failure } "CTest failed"
    exit 0
}

Require-Command "git" "Установите Git (winget install Git.Git)"

$Python = if (Get-Command "python" -ErrorAction SilentlyContinue) { "python" }
          elseif (Get-Command "python3" -ErrorAction SilentlyContinue) { "python3" }
          else { $null }

if ($Python) {
    if (-not (Test-Path "$Root\Data\Sounds\BanyaSteam.wav") -or -not (Test-Path "$Root\Data\Sounds\Shot_Railgun.wav")) {
        Write-Host "[1/3] Генерация звуков (tools/gen_sounds.py)..."
        Invoke-Checked { & $Python "$Root\tools\gen_sounds.py" } "Sound generation failed"
    }
}

# Один раз собираем и кэшируем статический Urho3D 1.9.0 в .cache\urho3d-sdk (OpenGL + GLSL shaders)
$LocalSdk = Join-Path $Root ".cache\urho3d-sdk"
$HasSdk = (Test-Path "$LocalSdk\include\Urho3D\Engine\Application.h") -and (
    (Test-Path "$LocalSdk\lib\Urho3D.lib") -or
    (Test-Path "$LocalSdk\lib\Urho3D\Urho3D.lib") -or
    (Test-Path "$LocalSdk\lib\libUrho3D.a") -or
    (Test-Path "$LocalSdk\lib\Urho3D\libUrho3D.a")
)

if (-not $HasSdk -and -not $env:URHO3D_HOME) {
    Write-Host "[Urho3D] Скачиваю и собираю кэшированный статический Urho3D 1.9.0 (OpenGL)..."
    $UrhoSrc = Join-Path $Root ".cache\urho3d-src"
    $UrhoBld = Join-Path $Root ".cache\urho3d-build"
    New-Item -ItemType Directory -Force -Path (Join-Path $Root ".cache") | Out-Null
    if (-not (Test-Path "$UrhoSrc\.git")) {
        if (Test-Path $UrhoSrc) { Remove-Item -Recurse -Force $UrhoSrc }
        Invoke-Checked { git clone --depth 1 --branch 1.9.0 https://github.com/urho3d/Urho3D.git $UrhoSrc } "git clone Urho3D failed"
    }
    if (Test-Path $UrhoBld) { Remove-Item -Recurse -Force $UrhoBld }
    $UrhoCmakeArgs = @(
        '-S', $UrhoSrc,
        '-B', $UrhoBld,
        '-DCMAKE_BUILD_TYPE=Release',
        '-DCMAKE_POLICY_VERSION_MINIMUM=3.5',
        "-DCMAKE_INSTALL_PREFIX=$LocalSdk",
        '-DLIB_SUFFIX=',
        '-DURHO3D_USE_LIB_DEB=1',
        '-DURHO3D_LIB_TYPE=STATIC',
        '-DURHO3D_OPENGL=ON',
        '-DURHO3D_D3D11=OFF',
        '-DURHO3D_WIN32_CONSOLE=OFF',
        '-DURHO3D_C++11=ON',
        '-DURHO3D_PCH=OFF',
        '-DURHO3D_SAMPLES=OFF',
        '-DURHO3D_TOOLS=OFF',
        '-DURHO3D_LUA=OFF',
        '-DURHO3D_LUAJIT=OFF',
        '-DURHO3D_ANGELSCRIPT=OFF',
        '-DURHO3D_NETWORK=OFF',
        '-DURHO3D_DATABASE_ODBC=OFF',
        '-DURHO3D_DATABASE_SQLITE=OFF',
        '-DURHO3D_URHO2D=OFF',
        '-DURHO3D_WEBP=OFF',
        '-DURHO3D_TESTING=OFF',
        '-DURHO3D_DOCS=OFF'
    )
    Invoke-Checked { & cmake @UrhoCmakeArgs } "Urho3D CMake configure failed"
    Invoke-Checked { & cmake --build $UrhoBld --config Release --parallel $Jobs } "Urho3D build failed"
    Invoke-Checked { & cmake --install $UrhoBld --config Release } "Urho3D install failed"
    Remove-Item -Recurse -Force $UrhoBld -ErrorAction SilentlyContinue
}

Write-Host "[2/3] Сборка CHIKENGUN 2.5 (MyShooter.exe + gen_textures_4k + test_core)..."
$ProjCmakeArgs = @(
    '-S', $Root,
    '-B', $BuildDir,
    '-DBUILD_GAME=ON',
    '-DAUTO_FETCH_URHO3D=ON',
    "-DTEX_SIZE=$TexSize",
    "-DURHO3D_HOME=$LocalSdk"
)
Invoke-Checked { & cmake @ProjCmakeArgs } "Project CMake configure failed"
Invoke-Checked { & cmake --build $BuildDir --config Release --parallel $Jobs } "Project CMake build failed"

$GenExe = @("$BuildDir\Release\gen_textures_4k.exe", "$BuildDir\gen_textures_4k.exe") | Where-Object { Test-Path $_ } | Select-Object -First 1
$AppExe = @("$BuildDir\Release\MyShooter.exe", "$BuildDir\MyShooter.exe") | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $AppExe) { throw "Не найден собранный бинарник MyShooter.exe!" }

$AppDir = Split-Path -Parent $AppExe
$TexOut = Join-Path $AppDir "Data\Textures"
$StampFile = Join-Path $TexOut ".4k_pbr_stamp"
$CurStamp = if (Test-Path $StampFile) { (Get-Content $StampFile -Raw).Trim() } else { "" }

if ($GenExe -and $CurStamp -ne "$TexSize") {
    Write-Host "[3/3] Генерация PBR текстур (${TexSize}x${TexSize}) -> $TexOut..."
    Invoke-Checked { & $GenExe $TexOut $TexSize } "Texture generation failed"
}

Write-Host "Готово! Запуск: .\run.bat"
