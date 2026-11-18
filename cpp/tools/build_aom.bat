@echo off
setlocal
set VS=C:\Program Files\Microsoft Visual Studio\18\Community
set MSVC=%VS%\VC\Tools\MSVC\14.51.36231
set KITS=C:\Program Files (x86)\Windows Kits\10
set KITVER=10.0.26100.0
set ROOT=%~dp0..
set CMAKE=%LOCALAPPDATA%\vcpkg\downloads\tools\cmake-4.4.0-windows\cmake-4.4.0-windows-x86_64\bin\cmake.exe
set NINJA=%LOCALAPPDATA%\vcpkg\downloads\tools\ninja-1.13.2-windows\ninja.exe
set NASM=%ROOT%\third_party\tools\nasm-2.16.03
set SRC=%ROOT%\third_party\aom-3.12.1
set BUILD=%ROOT%\third_party\build-aom
set INSTALL=%ROOT%\third_party\install-aom
set PERL=C:\Program Files\Git\usr\bin\perl.exe

set PATH=%MSVC%\bin\Hostx64\x64;%NASM%;%KITS%\bin\%KITVER%\x64;%PATH%
set INCLUDE=%MSVC%\include;%KITS%\Include\%KITVER%\ucrt;%KITS%\Include\%KITVER%\shared;%KITS%\Include\%KITVER%\um;%KITS%\Include\%KITVER%\winrt
set LIB=%MSVC%\lib\x64;%KITS%\Lib\%KITVER%\ucrt\x64;%KITS%\Lib\%KITVER%\um\x64

if not exist "%CMAKE%" ( echo No se encontro cmake & exit /b 1 )
if not exist "%NINJA%" ( echo No se encontro ninja & exit /b 1 )
if not exist "%NASM%\nasm.exe" ( echo No se encontro nasm & exit /b 1 )
if exist "%PERL%" set PATH=C:\Program Files\Git\usr\bin;%PATH%

"%CMAKE%" -S "%SRC%" -B "%BUILD%" -G Ninja ^
  -DCMAKE_MAKE_PROGRAM="%NINJA%" ^
  -DCMAKE_C_COMPILER=cl ^
  -DCMAKE_CXX_COMPILER=cl ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded ^
  -DCMAKE_INSTALL_PREFIX="%INSTALL%" ^
  -DBUILD_SHARED_LIBS=OFF ^
  -DENABLE_TESTS=OFF ^
  -DENABLE_EXAMPLES=OFF ^
  -DENABLE_TOOLS=OFF ^
  -DENABLE_DOCS=OFF ^
  -DCONFIG_AV1_ENCODER=1 ^
  -DCONFIG_AV1_DECODER=1 ^
  -DCONFIG_PIC=1 ^
  -DCONFIG_RUNTIME_CPU_DETECT=0 ^
  -DAOM_TARGET_CPU=x86_64
if errorlevel 1 exit /b 1

"%CMAKE%" --build "%BUILD%" --config Release
if errorlevel 1 exit /b 1

"%CMAKE%" --install "%BUILD%" --config Release
if errorlevel 1 exit /b 1

echo.
echo libaom instalado en %INSTALL%
dir /b "%INSTALL%\lib"
