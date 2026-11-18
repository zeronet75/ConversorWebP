param(
    [string]$Python = "python",
    [switch]$SinFFmpeg
)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$version = (Select-String -Path (Join-Path $root "conversor_webp.py") -Pattern 'APP_VERSION = "([^"]+)"').Matches[0].Groups[1].Value
$dist = Join-Path $root "dist"
$portable = Join-Path $root "dist_portable\ConversorWebP"
$iss = Join-Path $root "instalador\ConversorWebP.iss"
$iscc = Join-Path $env:LOCALAPPDATA "Programs\Inno Setup 6\ISCC.exe"

Write-Host "Empaquetando edicion Python $version ..."
Get-Process -Name ConversorWebP -ErrorAction SilentlyContinue | Stop-Process -Force
Push-Location $root
try {
    & $Python -m PyInstaller --noconfirm --clean ConversorWebP.spec
    if ($LASTEXITCODE -ne 0) { throw "PyInstaller fallo." }

    New-Item -ItemType Directory -Path $portable -Force | Out-Null
    Copy-Item (Join-Path $dist "ConversorWebP.exe") $portable -Force
    foreach ($archivo in @("LEEME.txt", "MANUAL_DE_USO.html", "LICENSE", "icon.ico")) {
        Copy-Item (Join-Path $root $archivo) $portable -Force
    }
    Set-Content (Join-Path $portable "portable.txt") "" -NoNewline

    if (-not $SinFFmpeg) {
        & (Join-Path $PSScriptRoot "preparar_ffmpeg.ps1") -Destino $portable
        & (Join-Path $PSScriptRoot "preparar_ffmpeg.ps1") -Destino $dist
        Copy-Item (Join-Path $dist "LICENCIA_FFMPEG.txt") $portable -Force
    }

    $zip = Join-Path $dist "ConversorWebP-Portable-$version.zip"
    if (Test-Path $zip) { Remove-Item $zip -Force }
    Compress-Archive -Path $portable -DestinationPath $zip -CompressionLevel Optimal
    Write-Host "ZIP portable: $zip"

    if (-not (Test-Path $iscc)) { throw "No se encontro Inno Setup 6: $iscc" }
    & $iscc $iss
    if ($LASTEXITCODE -ne 0) { throw "Inno Setup fallo." }
}
finally {
    Pop-Location
}
