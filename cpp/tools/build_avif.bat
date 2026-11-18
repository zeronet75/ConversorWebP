@echo off
setlocal
set VS=C:\Program Files\Microsoft Visual Studio\18\Community
set MSVC=%VS%\VC\Tools\MSVC\14.51.36231
set KITS=C:\Program Files (x86)\Windows Kits\10
set KITVER=10.0.26100.0
set ROOT=%~dp0..
set CMAKE=%LOCALAPPDATA%\vcpkg\downloads\tools\cmake-4.4.0-windows\cmake-4.4.0-windows-x86_64\bin\cmake.exe
set NINJA=%LOCALAPPDATA%\vcpkg\downloads\tools\ninja-1.13.2-windows\ninja.exe
set SRC=%ROOT%\third_party\libavif-1.4.2
set BUILD=%ROOT%\third_party\build-avif
set INSTALL=%ROOT%\third_party\install-avif
set AOM=%ROOT%\third_party\install-aom

set PATH=%MSVC%\bin\Hostx64\x64;%KITS%\bin\%KITVER%\x64;%PATH%
set INCLUDE=%MSVC%\include;%KITS%\Include\%KITVER%\ucrt;%KITS%\Include\%KITVER%\shared;%KITS%\Include\%KITVER%\um;%KITS%\Include\%KITVER%\winrt
set LIB=%MSVC%\lib\x64;%KITS%\Lib\%KITVER%\ucrt\x64;%KITS%\Lib\%KITVER%\um\x64

if not exist "%CMAKE%" ( echo No se encontro cmake & exit /b 1 )
if not exist "%NINJA%" ( echo No se encontro ninja & exit /b 1 )

"%CMAKE%" -S "%SRC%" -B "%BUILD%" -G Ninja ^
  -DCMAKE_MAKE_PROGRAM="%NINJA%" ^
  -DCMAKE_C_COMPILER=cl ^
  -DCMAKE_CXX_COMPILER=cl ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded ^
  -DCMAKE_INSTALL_PREFIX="%INSTALL%" ^
  -DBUILD_SHARED_LIBS=OFF ^
  -DAVIF_BUILD_APPS=OFF ^
  -DAVIF_BUILD_TESTS=OFF ^
  -DAVIF_BUILD_EXAMPLES=OFF ^
  -DAVIF_CODEC_AOM=SYSTEM ^
  -DAVIF_CODEC_DAV1D=OFF ^
  -DAVIF_CODEC_LIBGAV1=OFF ^
  -DAVIF_CODEC_RAV1E=OFF ^
  -DAVIF_CODEC_SVT=OFF ^
  -DAVIF_CODEC_AVM=OFF ^
  -DAVIF_LIBYUV=OFF ^
  -DAVIF_LIBSHARPYUV=OFF ^
  -DAVIF_LIBXML2=OFF ^
  -DAOM_INCLUDE_DIR="%AOM%\include" ^
  -DAOM_LIBRARY="%AOM%\lib\aom.lib"
if errorlevel 1 exit /b 1

"%CMAKE%" --build "%BUILD%" --config Release
if errorlevel 1 exit /b 1

"%CMAKE%" --install "%BUILD%" --config Release
if errorlevel 1 exit /b 1

echo.
echo libavif instalado en %INSTALL%
dir /b "%INSTALL%\lib"
