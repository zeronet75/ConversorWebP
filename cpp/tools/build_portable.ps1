param(
    [string]$Version = "2.10.0"
)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$bin = Join-Path $root "bin"
$stage = Join-Path ([IO.Path]::GetTempPath()) "ConversorWebPC-portable"
$zip = Join-Path $root "ConversorWebPC-$Version-cpp-portable.zip"

if (-not (Test-Path (Join-Path $bin "ConversorWebPC.exe"))) {
    throw "No existe bin\ConversorWebPC.exe. Compila primero con build.bat."
}

if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Path $stage | Out-Null

Copy-Item (Join-Path $bin "ConversorWebPC.exe") $stage
Copy-Item (Join-Path $bin "MANUAL_DE_USO.html") $stage
Copy-Item (Join-Path $bin "icon.ico") $stage
foreach ($extra in @("LEEME.txt", "ConversorWebPC.exe.manifest")) {
    $ruta = Join-Path $bin $extra
    if (Test-Path $ruta) { Copy-Item $ruta $stage }
}
foreach ($dll in @("7zr.exe", "ConversorWebPContext.dll")) {
    $ruta = Join-Path $bin $dll
    if (Test-Path $ruta) { Copy-Item $ruta $stage }
}
Copy-Item (Join-Path $root "README.md") (Join-Path $stage "LEEME_C++.md")
foreach ($script in @("INSTALAR_MENU_MODERNO.bat", "QUITAR_MENU_MODERNO.bat")) {
    $ruta = Join-Path $root "installer\$script"
    if (Test-Path $ruta) { Copy-Item $ruta $stage }
}
if (Test-Path (Join-Path $root "BENCHMARK.md")) {
    Copy-Item (Join-Path $root "BENCHMARK.md") $stage
}

if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip -CompressionLevel Optimal

Write-Host "ZIP portable creado: $zip"
Write-Host ("Tamano: {0:N0} KB" -f ((Get-Item $zip).Length / 1KB))
