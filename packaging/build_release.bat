@echo off
setlocal
set PATH=C:\msys64\ucrt64\bin;%PATH%

echo ========================================================
echo       Building Project Lawliet Release and Installer
echo ========================================================

echo.
echo [1/3] Building Lawliet Release Binary...
call "%~dp0..\build.bat"
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Application build failed.
    exit /b %ERRORLEVEL%
)

echo.
echo [2/3] Compiling Windows Installer Resource...
windres -i "%~dp0installer.rc" -o "%~dp0installer_rc.o"
if %ERRORLEVEL% neq 0 (
    echo [ERROR] windres failed.
    exit /b %ERRORLEVEL%
)

echo.
echo [3/3] Linking Lawliet-Setup.exe...
if not exist "%~dp0..\release" mkdir "%~dp0..\release"
c++ -O2 -s -mwindows "%~dp0installer.cpp" "%~dp0installer_rc.o" -o "%~dp0..\release\Lawliet-Setup.exe" -static -static-libgcc -static-libstdc++ -lole32 -luuid -lshell32
del /f /q "%~dp0installer_rc.o" 2>nul

copy /y "%~dp0..\build\windows-release\bin\lawliet.exe" "%~dp0..\release\lawliet.exe" >nul

echo.
echo ========================================================
echo Build complete!
echo Standalone Portable: %~dp0..\release\lawliet.exe
echo Windows Installer:   %~dp0..\release\Lawliet-Setup.exe
echo ========================================================
endlocal
