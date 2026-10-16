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
echo [3/5] Linking Lawliet-Setup.exe...
if not exist "%~dp0..\release" mkdir "%~dp0..\release"
c++ -O2 -s -mwindows "%~dp0installer.cpp" "%~dp0installer_rc.o" -o "%~dp0..\release\Lawliet-Setup.exe" -static -static-libgcc -static-libstdc++ -lole32 -luuid -lshell32
del /f /q "%~dp0installer_rc.o" 2>nul

copy /y "%~dp0..\build\windows-release\bin\lawliet.exe" "%~dp0..\release\lawliet.exe" >nul

echo.
echo [4/5] Packaging Portable ZIP...
powershell -NoProfile -Command "Compress-Archive -Path '%~dp0..\release\lawliet.exe', '%~dp0..\assets\icons\lawliet.ico', '%~dp0..\LICENSE', '%~dp0..\README.md', '%~dp0..\THIRD_PARTY_NOTICES.md' -DestinationPath '%~dp0..\release\Lawliet-v0.1.0-Windows-x64-Portable.zip' -Force"

echo.
echo [5/5] Generating SHA256 Checksums...
powershell -NoProfile -Command "Get-ChildItem -File '%~dp0..\release' | Where-Object { $_.Name -ne 'SHA256SUMS.txt' } | ForEach-Object { (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLower() + '  ' + $_.Name } | Set-Content -Encoding ascii '%~dp0..\release\SHA256SUMS.txt'"

echo.
echo ========================================================
echo Build complete!
echo Standalone Portable: %~dp0..\release\lawliet.exe
echo Windows Installer:   %~dp0..\release\Lawliet-Setup.exe
echo Portable ZIP:        %~dp0..\release\Lawliet-v0.1.0-Windows-x64-Portable.zip
echo Checksums:           %~dp0..\release\SHA256SUMS.txt
echo ========================================================
endlocal
