@echo off
setlocal
set VS=C:\Program Files\Microsoft Visual Studio\18\Community
set MSVC=%VS%\VC\Tools\MSVC\14.51.36231
set KITS=C:\Program Files (x86)\Windows Kits\10
set KITVER=10.0.26100.0

set PATH=%MSVC%\bin\Hostx64\x64;%KITS%\bin\%KITVER%\x64;%PATH%
set INCLUDE=%MSVC%\include;%KITS%\Include\%KITVER%\ucrt;%KITS%\Include\%KITVER%\shared;%KITS%\Include\%KITVER%\um;%KITS%\Include\%KITVER%\winrt
set LIB=%MSVC%\lib\x64;%KITS%\Lib\%KITVER%\ucrt\x64;%KITS%\Lib\%KITVER%\um\x64

set CMAKE=C:\Users\Portatil de Julia.DESKTOP-H8SCKBL\AppData\Local\vcpkg\downloads\tools\cmake-4.4.0-windows\cmake-4.4.0-windows-x86_64\bin\cmake.exe
set NINJA=C:\Users\Portatil de Julia.DESKTOP-H8SCKBL\AppData\Local\vcpkg\downloads\tools\ninja-1.13.2-windows\ninja.exe
set SRC=%~dp0..\third_party\libwebp-1.6.0
set BUILD=%~dp0..\third_party\build-libwebp

if not exist "%CMAKE%" (
  echo No se encontro cmake: %CMAKE%
  exit /b 1
)
if not exist "%NINJA%" (
  echo No se encontro ninja: %NINJA%
  exit /b 1
)

"%CMAKE%" -S "%SRC%" -B "%BUILD%" -G Ninja ^
  -DCMAKE_MAKE_PROGRAM="%NINJA%" ^
  -DCMAKE_C_COMPILER=cl ^
  -DCMAKE_CXX_COMPILER=cl ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded ^
  -DCMAKE_POLICY_DEFAULT_CMP0091=NEW ^
  -DBUILD_SHARED_LIBS=OFF ^
  -DWEBP_BUILD_ANIM_UTILS=OFF ^
  -DWEBP_BUILD_CWEBP=OFF ^
  -DWEBP_BUILD_DWEBP=OFF ^
  -DWEBP_BUILD_GIF2WEBP=OFF ^
  -DWEBP_BUILD_IMG2WEBP=OFF ^
  -DWEBP_BUILD_VWEBP=OFF ^
  -DWEBP_BUILD_WEBPINFO=OFF ^
  -DWEBP_BUILD_WEBPMUX=OFF ^
  -DWEBP_BUILD_EXTRAS=OFF ^
  -DWEBP_BUILD_WEBP_JS=OFF
if errorlevel 1 exit /b 1

"%CMAKE%" --build "%BUILD%" --config Release
if errorlevel 1 exit /b 1

echo.
echo Bibliotecas generadas:
dir /b "%BUILD%\*.lib"
