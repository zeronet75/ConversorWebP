@echo off
setlocal
set VS=C:\Program Files\Microsoft Visual Studio\18\Community
set MSVC=%VS%\VC\Tools\MSVC\14.51.36231
set KITS=C:\Program Files (x86)\Windows Kits\10
set KITVER=10.0.26100.0
set ROOT=%~dp0..
set SRC=%ROOT%\third_party\Little-CMS-lcms2.16
set OUT=%ROOT%\third_party\build-lcms2
set INSTALL=%ROOT%\third_party\install-lcms2

set PATH=%MSVC%\bin\Hostx64\x64;%KITS%\bin\%KITVER%\x64;%PATH%
set INCLUDE=%MSVC%\include;%KITS%\Include\%KITVER%\ucrt;%KITS%\Include\%KITVER%\shared;%KITS%\Include\%KITVER%\um
set LIB=%MSVC%\lib\x64;%KITS%\Lib\%KITVER%\ucrt\x64;%KITS%\Lib\%KITVER%\um\x64

if not exist "%OUT%" mkdir "%OUT%"
if not exist "%INSTALL%\include" mkdir "%INSTALL%\include"
if not exist "%INSTALL%\lib" mkdir "%INSTALL%\lib"

pushd "%OUT%"
cl /nologo /c /O2 /MT /D_CRT_SECURE_NO_WARNINGS /I"%SRC%\include" "%SRC%\src\cms*.c"
if errorlevel 1 ( popd & exit /b 1 )
lib /nologo /OUT:lcms2.lib cms*.obj
if errorlevel 1 ( popd & exit /b 1 )
popd

copy /y "%SRC%\include\lcms2.h" "%INSTALL%\include\" >nul
copy /y "%SRC%\include\lcms2_plugin.h" "%INSTALL%\include\" >nul
copy /y "%OUT%\lcms2.lib" "%INSTALL%\lib\" >nul
echo.
echo lcms2 instalado en %INSTALL%
