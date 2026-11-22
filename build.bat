@echo off
setlocal
where cl >nul 2>nul
if errorlevel 1 (
  echo Microsoft C++ compiler (cl.exe) was not found.
  echo Open an x86 Native Tools Command Prompt for Visual Studio.
  exit /b 1
)
if not exist build mkdir build
cl /nologo /EHsc /O2 /DUNICODE /D_UNICODE /D_WIN32_WINNT=0x0600 /DWINVER=0x0600 /I. src\main.cpp /link /SUBSYSTEM:WINDOWS,6.00 /OUT:build\ShadowPlay.exe shell32.lib ole32.lib user32.lib gdi32.lib
if errorlevel 1 exit /b 1
echo.
echo Build finished: build\ShadowPlay.exe
endlocal
