// packaging/installer.cpp
// Self-contained Windows installer for Project Lawliet.
// Extracts lawliet.exe and lawliet.ico to %LOCALAPPDATA%\Programs\Lawliet,
// creates Start Menu and Desktop shortcuts, registers in Windows Add/Remove Programs,
// and optionally launches the application.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <string>

#define IDI_MAIN_ICON 1
#define IDR_EXE_PAYLOAD 100
#define IDR_ICO_PAYLOAD 101

static bool CreateShortcut(const std::wstring& shortcutPath,
                           const std::wstring& targetExePath,
                           const std::wstring& iconPath,
                           const std::wstring& description,
                           const std::wstring& workingDir)
{
    IShellLinkW* psl = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&psl);
    if (FAILED(hr) || !psl) return false;

    psl->SetPath(targetExePath.c_str());
    psl->SetDescription(description.c_str());
    psl->SetWorkingDirectory(workingDir.c_str());
    if (!iconPath.empty()) {
        psl->SetIconLocation(iconPath.c_str(), 0);
    }

    IPersistFile* ppf = nullptr;
    hr = psl->QueryInterface(IID_IPersistFile, (void**)&ppf);
    if (SUCCEEDED(hr) && ppf) {
        hr = ppf->Save(shortcutPath.c_str(), TRUE);
        ppf->Release();
    }
    psl->Release();
    return SUCCEEDED(hr);
}

static bool ExtractResourceToFile(int resId, const std::wstring& outputPath)
{
    HRSRC hRes = FindResourceW(nullptr, MAKEINTRESOURCEW(resId), MAKEINTRESOURCEW(10) /* RT_RCDATA */);
    if (!hRes) return false;
    HGLOBAL hGlobal = LoadResource(nullptr, hRes);
    if (!hGlobal) return false;
    DWORD size = SizeofResource(nullptr, hRes);
    void* data = LockResource(hGlobal);
    if (!data || size == 0) return false;

    HANDLE hFile = CreateFileW(outputPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    BOOL ok = WriteFile(hFile, data, size, &written, nullptr);
    CloseHandle(hFile);
    return ok && (written == size);
}

static void WriteUninstaller(const std::wstring& uninstPath, const std::wstring& installDir)
{
    std::string script =
        "@echo off\r\n"
        "echo Uninstalling Lawliet...\r\n"
        "taskkill /F /IM lawliet.exe >nul 2>&1\r\n"
        "del /F /Q \"%USERPROFILE%\\Desktop\\Lawliet.lnk\" >nul 2>&1\r\n"
        "del /F /Q \"%APPDATA%\\Microsoft\\Windows\\Start Menu\\Programs\\Lawliet.lnk\" >nul 2>&1\r\n"
        "reg delete \"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Lawliet\" /f >nul 2>&1\r\n"
        "start /b \"\" cmd /c \"timeout /t 1 /nobreak >nul & rd /s /q \\\"" +
        std::string(installDir.begin(), installDir.end()) + "\\\"\"\r\n"
        "exit\r\n";

    HANDLE hFile = CreateFileW(uninstPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(hFile, script.data(), static_cast<DWORD>(script.size()), &written, nullptr);
        CloseHandle(hFile);
    }
}

static void RegisterUninstall(const std::wstring& installDir, const std::wstring& iconPath, const std::wstring& uninstPath)
{
    HKEY hKey = nullptr;
    const wchar_t* subKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Lawliet";
    if (RegCreateKeyExW(HKEY_CURRENT_USER, subKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        auto setStr = [&](const wchar_t* name, const std::wstring& val) {
            RegSetValueExW(hKey, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(val.c_str()), static_cast<DWORD>((val.length() + 1) * sizeof(wchar_t)));
        };
        auto setDword = [&](const wchar_t* name, DWORD val) {
            RegSetValueExW(hKey, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&val), sizeof(DWORD));
        };

        setStr(L"DisplayName", L"Lawliet - Dual-Pitch Voice Changer");
        setStr(L"DisplayVersion", L"0.1.0");
        setStr(L"Publisher", L"Project Lawliet");
        setStr(L"DisplayIcon", iconPath);
        setStr(L"InstallLocation", installDir);
        std::wstring uninstCmd = L"\"" + uninstPath + L"\"";
        setStr(L"UninstallString", uninstCmd);
        setDword(L"NoModify", 1);
        setDword(L"NoRepair", 1);
        RegCloseKey(hKey);
    }
}

int WINAPI WinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, LPSTR /*lpCmdLine*/, int /*nCmdShow*/)
{
    CoInitialize(nullptr);

    // 1. Resolve %LOCALAPPDATA%\Programs\Lawliet
    wchar_t localAppPath[MAX_PATH];
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, localAppPath))) {
        MessageBoxW(nullptr, L"Unable to locate Local AppData directory.", L"Lawliet Installer Error", MB_ICONERROR | MB_OK);
        CoUninitialize();
        return 1;
    }

    std::wstring programsDir = std::wstring(localAppPath) + L"\\Programs";
    CreateDirectoryW(programsDir.c_str(), nullptr);

    std::wstring installDir = programsDir + L"\\Lawliet";
    CreateDirectoryW(installDir.c_str(), nullptr);

    std::wstring destExe = installDir + L"\\lawliet.exe";
    std::wstring destIco = installDir + L"\\lawliet.ico";
    std::wstring destUninst = installDir + L"\\uninstall.bat";

    // Close any running instance if open
    HWND existingWnd = FindWindowA(nullptr, "Lawliet Voice Changer");
    if (existingWnd) {
        PostMessageA(existingWnd, WM_CLOSE, 0, 0);
        Sleep(300);
    }

    // 2. Extract payload files
    if (!ExtractResourceToFile(IDR_EXE_PAYLOAD, destExe)) {
        MessageBoxW(nullptr, L"Failed to extract application executable.\nPlease ensure you have write permissions.", L"Lawliet Installer Error", MB_ICONERROR | MB_OK);
        CoUninitialize();
        return 1;
    }
    ExtractResourceToFile(IDR_ICO_PAYLOAD, destIco);

    // 3. Write uninstaller script and register in Windows
    WriteUninstaller(destUninst, installDir);
    RegisterUninstall(installDir, destIco, destUninst);

    // 4. Create Start Menu Shortcut
    wchar_t startMenuPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROGRAMS, nullptr, 0, startMenuPath))) {
        std::wstring shortcut = std::wstring(startMenuPath) + L"\\Lawliet.lnk";
        CreateShortcut(shortcut, destExe, destIco, L"Lawliet - Dual-Pitch Voice Changer", installDir);
    }

    // 5. Create Desktop Shortcut
    wchar_t desktopPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, desktopPath))) {
        std::wstring shortcut = std::wstring(desktopPath) + L"\\Lawliet.lnk";
        CreateShortcut(shortcut, destExe, destIco, L"Lawliet - Dual-Pitch Voice Changer", installDir);
    }

    // 6. Success notification & launch prompt
    std::wstring successMsg =
        L"Project Lawliet has been successfully installed!\n\n"
        L"Location:\n" + installDir + L"\n\n"
        L"Shortcuts created on your Desktop and in the Start Menu.\n\n"
        L"Would you like to launch Lawliet now?";

    int choice = MessageBoxW(nullptr, successMsg.c_str(), L"Lawliet Setup Completed", MB_ICONINFORMATION | MB_YESNO | MB_DEFBUTTON1);
    if (choice == IDYES) {
        ShellExecuteW(nullptr, L"open", destExe.c_str(), nullptr, installDir.c_str(), SW_SHOWNORMAL);
    }

    CoUninitialize();
    return 0;
}
