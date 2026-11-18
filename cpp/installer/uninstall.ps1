$ErrorActionPreference = "Stop"
$destino = Join-Path $env:LOCALAPPDATA "Programs\Conversor WebP C++"
$exe = Join-Path $destino "ConversorWebPC.exe"

if (Test-Path $exe) {
    & $exe --uninstall-context | Out-Null
    & $exe --uninstall-sendto | Out-Null
}

$shell = New-Object -ComObject WScript.Shell
$inicio = Join-Path $env:APPDATA "Microsoft\Windows\Start Menu\Programs"
$escritorio = [Environment]::GetFolderPath("Desktop")
foreach ($lnk in @((Join-Path $inicio "Conversor WebP C++.lnk"),
                   (Join-Path $escritorio "Conversor WebP C++.lnk"))) {
    if (Test-Path $lnk) { Remove-Item $lnk -Force }
}

if (Test-Path $destino) { Remove-Item $destino -Recurse -Force }
Write-Host "Conversor WebP C++ desinstalado."
