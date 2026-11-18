@echo off
setlocal
set VS=C:\Program Files\Microsoft Visual Studio\18\Community
set MSVC=%VS%\VC\Tools\MSVC\14.51.36231
set KITS=C:\Program Files (x86)\Windows Kits\10
set KITVER=10.0.26100.0

set PATH=%MSVC%\bin\Hostx64\x64;%KITS%\bin\%KITVER%\x64;%PATH%
set INCLUDE=%MSVC%\include;%KITS%\Include\%KITVER%\ucrt;%KITS%\Include\%KITVER%\shared;%KITS%\Include\%KITVER%\um;%KITS%\Include\%KITVER%\winrt
set LIB=%MSVC%\lib\x64;%KITS%\Lib\%KITVER%\ucrt\x64;%KITS%\Lib\%KITVER%\um\x64

set RAIZ=%~dp0
set WEBP=%RAIZ%third_party\libwebp-1.6.0
set WEBPLIB=%RAIZ%third_party\build-libwebp
set AVIF=%RAIZ%third_party\install-avif
set AOM=%RAIZ%third_party\install-aom
set LCMS=%RAIZ%third_party\install-lcms2
set ZLIB=%RAIZ%third_party\install-zlib

if not exist "%RAIZ%bin" mkdir "%RAIZ%bin"

:: Icono embebido en el ejecutable
if not exist "%RAIZ%obj" mkdir "%RAIZ%obj"
rc /nologo /fo "%RAIZ%obj\app.res" "%RAIZ%app.rc"
if errorlevel 1 exit /b 1

:: No usar /OPT:ICF: al fusionar funciones de libavif/AOM el ejecutable fallaba al iniciar.
cl /nologo /std:c++20 /EHsc /O2 /MT /DUNICODE /D_UNICODE /utf-8 /wd5033 ^
   /I"%WEBP%\src" ^
   /I"%AVIF%\include" ^
   /I"%AOM%\include" ^
   /I"%LCMS%\include" ^
   /I"%ZLIB%\include" ^
   "%RAIZ%src\main.cpp" "%RAIZ%obj\app.res" ^
   /Fe:"%RAIZ%bin\ConversorWebPC.exe" ^
   /link /SUBSYSTEM:WINDOWS /ENTRY:wmainCRTStartup /NOIMPLIB /NOEXP ^
   /LIBPATH:"%WEBPLIB%" /LIBPATH:"%AVIF%\lib" /LIBPATH:"%AOM%\lib" /LIBPATH:"%LCMS%\lib" /LIBPATH:"%ZLIB%\lib" ^
   libwebp.lib libwebpmux.lib libwebpdemux.lib libsharpyuv.lib avif.lib aom.lib lcms2.lib zlibstatic.lib ^
   windowscodecs.lib comctl32.lib shlwapi.lib ole32.lib oleaut32.lib uuid.lib ^
   shell32.lib user32.lib gdi32.lib advapi32.lib uxtheme.lib propsys.lib
if errorlevel 1 exit /b 1

call "%RAIZ%build_explorer.bat"
if errorlevel 1 exit /b 1

copy /y "%RAIZ%MANUAL_DE_USO.html" "%RAIZ%bin\" >nul
copy /y "%RAIZ%..\icon.ico" "%RAIZ%bin\" >nul
if exist "%RAIZ%third_party\tools\7zr.exe" copy /y "%RAIZ%third_party\tools\7zr.exe" "%RAIZ%bin\" >nul
echo.
echo Compilado: %RAIZ%bin\ConversorWebPC.exe


