param(
    [string]$Thumbprint = $env:CERT_THUMBPRINT,
    [string]$Pfx = $env:CERT_PFX,
    [string]$Password = $env:CERT_PASSWORD,
    [string]$Version = "2.13.0",
    [switch]$Check
)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$artefactos = @(
    (Join-Path $root "dist\ConversorWebP.exe"),
    (Join-Path $root "dist_instalador\ConversorWebP-Setup-$Version.exe"),
    (Join-Path $root "cpp\bin\ConversorWebPC.exe"),
    (Join-Path $root "cpp\dist\ConversorWebPC-$Version-setup.exe")
)

foreach ($f in $artefactos) {
    if (-not (Test-Path -LiteralPath $f)) {
        throw "No existe el artefacto: $f"
    }
}

if ($Check) {
    Write-Host "Artefactos encontrados:"
    $artefactos | ForEach-Object { Write-Host "  $_" }
    Write-Host "Certificado configurado: $([bool]($Thumbprint -or $Pfx))"
    exit 0
}

if (-not $Thumbprint -and -not $Pfx) {
    throw "Define CERT_THUMBPRINT o CERT_PFX/CERT_PASSWORD para firmar."
}

$signtool = (Get-Command signtool.exe -ErrorAction SilentlyContinue).Source
if (-not $signtool) {
    $kits = "${env:ProgramFiles(x86)}\Windows Kits\10\bin"
    $signtool = Get-ChildItem $kits -Filter signtool.exe -Recurse -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '\\x64\\' } |
        Select-Object -First 1 -ExpandProperty FullName
}
if (-not $signtool) {
    throw "No se encontro signtool.exe (Windows SDK)."
}

foreach ($f in $artefactos) {
    if ($Pfx) {
        & $signtool sign /fd SHA256 /td SHA256 /tr http://timestamp.digicert.com /f $Pfx /p $Password $f
    } else {
        & $signtool sign /fd SHA256 /td SHA256 /tr http://timestamp.digicert.com /sha1 $Thumbprint $f
    }
    if ($LASTEXITCODE -ne 0) { throw "Fallo al firmar: $f" }
    Write-Host "Firmado: $f"
}
