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
if not exist "%RAIZ%bin" mkdir "%RAIZ%bin"
cl /nologo /std:c++20 /EHsc /O2 /MT /DUNICODE /D_UNICODE /utf-8 /LD ^
   "%RAIZ%src\explorer_command.cpp" ^
   /Fe:"%RAIZ%bin\ConversorWebPContext.dll" ^
   /link /NOIMPLIB /NOEXP ^
   shell32.lib shlwapi.lib ole32.lib advapi32.lib user32.lib
if errorlevel 1 exit /b 1
echo.
echo Compilado: %RAIZ%bin\ConversorWebPContext.dll
