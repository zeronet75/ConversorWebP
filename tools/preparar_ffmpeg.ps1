param(
    [string]$Destino = "",
    [string]$Cache = (Join-Path (Split-Path $PSScriptRoot -Parent) "third_party\ffmpeg"),
    [switch]$Forzar
)

$ErrorActionPreference = "Stop"
$url = "https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-essentials.zip"
$zip = Join-Path $Cache "ffmpeg-release-essentials.zip"
$ffmpeg = Join-Path $Cache "ffmpeg.exe"
$ffprobe = Join-Path $Cache "ffprobe.exe"
$licencia = Join-Path $Cache "LICENCIA_FFMPEG.txt"

New-Item -ItemType Directory -Path $Cache -Force | Out-Null

if ($Forzar -or -not (Test-Path $ffmpeg) -or -not (Test-Path $ffprobe)) {
    if ($Forzar -or -not (Test-Path $zip)) {
        Write-Host "Descargando FFmpeg desde $url ..."
        Invoke-WebRequest -Uri $url -OutFile $zip -UseBasicParsing
    }
    $extraido = Join-Path $Cache "_extraido"
    if (Test-Path $extraido) { Remove-Item $extraido -Recurse -Force }
    Write-Host "Extrayendo FFmpeg ..."
    Expand-Archive -Path $zip -DestinationPath $extraido -Force
    $srcFfmpeg = Get-ChildItem $extraido -Recurse -Filter "ffmpeg.exe" |
        Where-Object { $_.Directory.Name -eq "bin" } | Select-Object -First 1
    $srcFfprobe = Get-ChildItem $extraido -Recurse -Filter "ffprobe.exe" |
        Where-Object { $_.Directory.Name -eq "bin" } | Select-Object -First 1
    if (-not $srcFfmpeg -or -not $srcFfprobe) {
        throw "El ZIP de FFmpeg no contiene ffmpeg.exe y ffprobe.exe."
    }
    Copy-Item $srcFfmpeg.FullName $ffmpeg -Force
    Copy-Item $srcFfprobe.FullName $ffprobe -Force

    $srcLicencia = Get-ChildItem $extraido -Recurse -Filter "LICENSE" | Select-Object -First 1
    if ($srcLicencia) {
        $aviso = @"
FFmpeg (build estatica de gyan.dev) se distribuye bajo la GNU General Public License v3.
Codigo fuente: https://ffmpeg.org/download.html
Build utilizada: https://www.gyan.dev/ffmpeg/builds/
Se incluye el texto completo de la licencia a continuacion.

"@
        Set-Content $licencia ($aviso + (Get-Content $srcLicencia.FullName -Raw)) -Encoding UTF8
    }
    Remove-Item $extraido -Recurse -Force
    Write-Host "FFmpeg preparado en $Cache"
}

if ($Destino) {
    New-Item -ItemType Directory -Path $Destino -Force | Out-Null
    Copy-Item $ffmpeg $Destino -Force
    Copy-Item $ffprobe $Destino -Force
    if (Test-Path $licencia) { Copy-Item $licencia $Destino -Force }
    Write-Host "FFmpeg copiado a $Destino"
}
