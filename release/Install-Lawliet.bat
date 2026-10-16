@echo off
setlocal
echo ========================================================
echo        Project Lawliet - Windows Installer
echo ========================================================
echo.

set TARGET_DIR=%LOCALAPPDATA%\Programs\Lawliet
if not exist "%TARGET_DIR%" mkdir "%TARGET_DIR%"

echo [1/4] Copying executable and icon...
copy /Y "%~dp0lawliet.exe" "%TARGET_DIR%\lawliet.exe" >nul
if exist "%~dp0..\assets\icons\lawliet.ico" (
    copy /Y "%~dp0..\assets\icons\lawliet.ico" "%TARGET_DIR%\lawliet.ico" >nul
)

echo [2/4] Creating Start Menu shortcut...
powershell -NoProfile -Command "$ws = New-Object -ComObject WScript.Shell; $s = $ws.CreateShortcut(\"$env:APPDATA\Microsoft\Windows\Start Menu\Programs\Lawliet.lnk\"); $s.TargetPath = \"$env:LOCALAPPDATA\Programs\Lawliet\lawliet.exe\"; $s.WorkingDirectory = \"$env:LOCALAPPDATA\Programs\Lawliet\"; $s.Description = \"Lawliet - Dual-Pitch Voice Changer\"; if (Test-Path \"$env:LOCALAPPDATA\Programs\Lawliet\lawliet.ico\") { $s.IconLocation = \"$env:LOCALAPPDATA\Programs\Lawliet\lawliet.ico,0\" }; $s.Save()"

echo [3/4] Creating Desktop shortcut...
powershell -NoProfile -Command "$ws = New-Object -ComObject WScript.Shell; $s = $ws.CreateShortcut(\"$env:USERPROFILE\Desktop\Lawliet.lnk\"); $s.TargetPath = \"$env:LOCALAPPDATA\Programs\Lawliet\lawliet.exe\"; $s.WorkingDirectory = \"$env:LOCALAPPDATA\Programs\Lawliet\"; $s.Description = \"Lawliet - Dual-Pitch Voice Changer\"; if (Test-Path \"$env:LOCALAPPDATA\Programs\Lawliet\lawliet.ico\") { $s.IconLocation = \"$env:LOCALAPPDATA\Programs\Lawliet\lawliet.ico,0\" }; $s.Save()"

echo [4/4] Writing uninstaller...
(
echo @echo off
echo echo Uninstalling Lawliet...
echo taskkill /F /IM lawliet.exe 2^>nul
echo del /F /Q "%%USERPROFILE%%\Desktop\Lawliet.lnk" 2^>nul
echo del /F /Q "%%APPDATA%%\Microsoft\Windows\Start Menu\Programs\Lawliet.lnk" 2^>nul
echo reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\Lawliet" /f 2^>nul
echo start /b "" cmd /c "timeout /t 1 /nobreak ^>nul ^& rd /s /q \"%TARGET_DIR%\""
) > "%TARGET_DIR%\uninstall.bat"

rem Register in Windows Add/Remove Programs
reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\Lawliet" /v "DisplayName" /d "Lawliet - Dual-Pitch Voice Changer" /f >nul 2>&1
reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\Lawliet" /v "DisplayVersion" /d "0.1.0" /f >nul 2>&1
reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\Lawliet" /v "Publisher" /d "Project Lawliet" /f >nul 2>&1
reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\Lawliet" /v "DisplayIcon" /d "%TARGET_DIR%\lawliet.ico" /f >nul 2>&1
reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\Lawliet" /v "InstallLocation" /d "%TARGET_DIR%" /f >nul 2>&1
reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\Lawliet" /v "UninstallString" /d "\"%TARGET_DIR%\uninstall.bat\"" /f >nul 2>&1

echo.
echo ========================================================
echo Installation complete!
echo Installed to: %TARGET_DIR%
echo Shortcuts added to Start Menu and Desktop.
echo ========================================================
echo.
set /p LAUNCH="Would you like to launch Lawliet now? (Y/N): "
if /i "%LAUNCH%"=="Y" (
    start "" "%TARGET_DIR%\lawliet.exe"
)
endlocal
