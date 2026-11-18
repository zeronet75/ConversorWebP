param(
    [switch]$ContextMenu,
    [switch]$SendTo,
    [switch]$Desktop
)

$ErrorActionPreference = "Stop"
$origen = Split-Path $PSScriptRoot -Parent
$bin = Join-Path $origen "bin"
$destino = Join-Path $env:LOCALAPPDATA "Programs\Conversor WebP C++"
$exe = Join-Path $destino "ConversorWebPC.exe"

if (-not (Test-Path (Join-Path $bin "ConversorWebPC.exe"))) {
    throw "No existe bin\ConversorWebPC.exe. Compila primero con build.bat."
}

New-Item -ItemType Directory -Path $destino -Force | Out-Null
Copy-Item (Join-Path $bin "ConversorWebPC.exe") $destino -Force
foreach ($archivo in @("MANUAL_DE_USO.html", "icon.ico", "7zr.exe", "LEEME.txt", "ConversorWebPC.exe.manifest")) {
    $ruta = Join-Path $bin $archivo
    if (Test-Path $ruta) { Copy-Item $ruta $destino -Force }
}
Copy-Item (Join-Path $origen "README.md") (Join-Path $destino "LEEME_C++.md") -Force
if (Test-Path (Join-Path $origen "BENCHMARK.md")) {
    Copy-Item (Join-Path $origen "BENCHMARK.md") $destino -Force
}
if (Test-Path (Join-Path $origen "docs")) {
    Copy-Item (Join-Path $origen "docs") $destino -Recurse -Force
}

$shell = New-Object -ComObject WScript.Shell
$inicio = Join-Path $env:APPDATA "Microsoft\Windows\Start Menu\Programs"
$acceso = $shell.CreateShortcut((Join-Path $inicio "Conversor WebP C++.lnk"))
$acceso.TargetPath = $exe
$acceso.WorkingDirectory = $destino
$acceso.IconLocation = (Join-Path $destino "icon.ico")
$acceso.Save()

if ($Desktop) {
    $escritorio = [Environment]::GetFolderPath("Desktop")
    $acceso = $shell.CreateShortcut((Join-Path $escritorio "Conversor WebP C++.lnk"))
    $acceso.TargetPath = $exe
    $acceso.WorkingDirectory = $destino
    $acceso.IconLocation = (Join-Path $destino "icon.ico")
    $acceso.Save()
}
if ($ContextMenu) { & $exe --install-context | Out-Null }
if ($SendTo) { & $exe --install-sendto | Out-Null }

@"
Conversor WebP C++ instalado en:
$destino

Para desinstalarlo:
powershell -ExecutionPolicy Bypass -File "$PSScriptRoot\uninstall.ps1"
"@ | Write-Host
