@echo off
regsvr32 /s "%~dp0ConversorWebPContext.dll"
if errorlevel 1 (
  echo No se pudo registrar el menu contextual moderno.
  pause
  exit /b 1
)
echo Menu contextual moderno instalado.
pause
