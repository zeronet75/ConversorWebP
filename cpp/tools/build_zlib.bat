@echo off
setlocal
set VS=C:\Program Files\Microsoft Visual Studio\18\Community
set MSVC=%VS%\VC\Tools\MSVC\14.51.36231
set KITS=C:\Program Files (x86)\Windows Kits\10
set KITVER=10.0.26100.0
set ROOT=%~dp0..
set CMAKE=%LOCALAPPDATA%\vcpkg\downloads\tools\cmake-4.4.0-windows\cmake-4.4.0-windows-x86_64\bin\cmake.exe
set NINJA=%LOCALAPPDATA%\vcpkg\downloads\tools\ninja-1.13.2-windows\ninja.exe
set SRC=%ROOT%\third_party\zlib-1.3.1
set BUILD=%ROOT%\third_party\build-zlib
set INSTALL=%ROOT%\third_party\install-zlib

set PATH=%MSVC%\bin\Hostx64\x64;%KITS%\bin\%KITVER%\x64;%PATH%
set INCLUDE=%MSVC%\include;%KITS%\Include\%KITVER%\ucrt;%KITS%\Include\%KITVER%\shared;%KITS%\Include\%KITVER%\um
set LIB=%MSVC%\lib\x64;%KITS%\Lib\%KITVER%\ucrt\x64;%KITS%\Lib\%KITVER%\um\x64

"%CMAKE%" -S "%SRC%" -B "%BUILD%" -G Ninja ^
  -DCMAKE_MAKE_PROGRAM="%NINJA%" ^
  -DCMAKE_C_COMPILER=cl ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded ^
  -DCMAKE_INSTALL_PREFIX="%INSTALL%" ^
  -DBUILD_SHARED_LIBS=OFF
if errorlevel 1 exit /b 1
"%CMAKE%" --build "%BUILD%" --config Release
if errorlevel 1 exit /b 1
"%CMAKE%" --install "%BUILD%" --config Release
if errorlevel 1 exit /b 1
echo zlib instalado en %INSTALL%
