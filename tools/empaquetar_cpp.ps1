param(
    [switch]$SinFFmpeg,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$cpp = Join-Path $root "cpp"
$bin = Join-Path $cpp "bin"
$version = (Select-String -Path (Join-Path $root "conversor_webp.py") -Pattern 'APP_VERSION = "([^"]+)"').Matches[0].Groups[1].Value
$iscc = Join-Path $env:LOCALAPPDATA "Programs\Inno Setup 6\ISCC.exe"

Write-Host "Empaquetando edicion C++ $version ..."
Get-Process -Name ConversorWebPC -ErrorAction SilentlyContinue | Stop-Process -Force

if (-not $SkipBuild) {
    & cmd.exe /c "`"$(Join-Path $cpp 'build.bat')`""
    if ($LASTEXITCODE -ne 0) { throw "La compilacion C++ fallo." }
}

if (-not $SinFFmpeg) {
    & (Join-Path $PSScriptRoot "preparar_ffmpeg.ps1") -Destino $bin
}

& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $cpp "tools\build_portable.ps1") -Version $version
if ($LASTEXITCODE -ne 0) { throw "El portable C++ fallo." }

if (-not (Test-Path $iscc)) { throw "No se encontro Inno Setup 6: $iscc" }
& $iscc (Join-Path $cpp "installer\ConversorWebPC.iss")
if ($LASTEXITCODE -ne 0) { throw "Inno Setup fallo." }
