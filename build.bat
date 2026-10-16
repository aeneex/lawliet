@echo off
setlocal
set PATH=C:\msys64\ucrt64\bin;%PATH%

echo [build.bat] Configuring windows-release preset...
cmake --preset windows-release
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake configure failed.
    exit /b %ERRORLEVEL%
)

echo [build.bat] Building windows-release...
cmake --build --preset windows-release
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Build failed.
    exit /b %ERRORLEVEL%
)

echo.
echo ========================================================
echo Build successful!
echo Executable: %~dp0build\windows-release\bin\lawliet.exe
echo ========================================================
endlocal
