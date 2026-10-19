// ShellIntegration.cpp
#include "pch.h"
#include <shlobj.h>
#include "ShellIntegration.h"

namespace
{
    // One verb key per item class: "*" is every file, "Directory" every folder.
    const wchar_t* const kClassKeys[] = {
        L"Software\\Classes\\*\\shell\\DiskForge.FileUnlocker",
        L"Software\\Classes\\Directory\\shell\\DiskForge.FileUnlocker",
    };

    const wchar_t kMenuText[] = L"Unlock with File Unlocker";

    std::wstring CommandLineFor(const std::wstring& exe)
    {
        return L"\"" + exe + L"\" \"%1\"";
    }

    bool SetString(HKEY hKey, const wchar_t* valueName, const std::wstring& data)
    {
        return RegSetValueExW(hKey, valueName, 0, REG_SZ,
                              reinterpret_cast<const BYTE*>(data.c_str()),
                              (DWORD)((data.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
    }

    std::wstring ReadDefaultString(const std::wstring& keyPath)
    {
        WCHAR buf[2048] = {};
        DWORD cb = sizeof(buf) - sizeof(WCHAR);
        if (RegGetValueW(HKEY_CURRENT_USER, keyPath.c_str(), nullptr, RRF_RT_REG_SZ,
                         nullptr, buf, &cb) != ERROR_SUCCESS)
            return std::wstring();
        return buf;
    }

    std::wstring ErrorText(LONG code)
    {
        WCHAR* msg = nullptr;
        FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       nullptr, (DWORD)code, 0, (LPWSTR)&msg, 0, nullptr);
        std::wstring text = msg ? msg : L"Unknown error";
        if (msg)
            LocalFree(msg);
        while (!text.empty() && (text.back() == L'\n' || text.back() == L'\r'))
            text.pop_back();
        return text;
    }
}

std::wstring ShellIntegration::ExecutablePath()
{
    std::wstring path(MAX_PATH, L'\0');
    for (;;)
    {
        DWORD len = GetModuleFileNameW(nullptr, &path[0], (DWORD)path.size());
        if (len < path.size())
        {
            path.resize(len);
            return path;
        }
        path.resize(path.size() * 2);
    }
}

bool ShellIntegration::IsRegistered()
{
    const std::wstring expected = CommandLineFor(ExecutablePath());
    for (const wchar_t* key : kClassKeys)
    {
        if (_wcsicmp(ReadDefaultString(std::wstring(key) + L"\\command").c_str(), expected.c_str()) != 0)
            return false;
    }
    return true;
}

bool ShellIntegration::Register(std::wstring& errorOut)
{
    const std::wstring exe = ExecutablePath();

    for (const wchar_t* key : kClassKeys)
    {
        HKEY hVerb = nullptr;
        LONG rc = RegCreateKeyExW(HKEY_CURRENT_USER, key, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hVerb, nullptr);
        if (rc != ERROR_SUCCESS)
        {
            errorOut = L"Could not create the Explorer menu entry: " + ErrorText(rc);
            return false;
        }
        bool ok = SetString(hVerb, nullptr, kMenuText) &&
                  SetString(hVerb, L"MUIVerb", kMenuText) &&
                  SetString(hVerb, L"Icon", L"\"" + exe + L"\",0");
        RegCloseKey(hVerb);

        HKEY hCmd = nullptr;
        if (ok)
        {
            rc = RegCreateKeyExW(HKEY_CURRENT_USER, (std::wstring(key) + L"\\command").c_str(), 0, nullptr, 0,
                                 KEY_SET_VALUE, nullptr, &hCmd, nullptr);
            ok = (rc == ERROR_SUCCESS) && SetString(hCmd, nullptr, CommandLineFor(exe));
            if (hCmd)
                RegCloseKey(hCmd);
        }
        if (!ok)
        {
            errorOut = L"Could not write the Explorer menu entry.";
            return false;
        }
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return true;
}

bool ShellIntegration::IsProcessElevated()
{
    bool elevated = false;
    HANDLE hToken = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
    {
        TOKEN_ELEVATION te = {};
        DWORD cb = 0;
        if (GetTokenInformation(hToken, TokenElevation, &te, sizeof(te), &cb))
            elevated = te.TokenIsElevated != 0;
        CloseHandle(hToken);
    }
    return elevated;
}

bool ShellIntegration::IsOptedOut()
{
    return AfxGetApp()->GetProfileInt(_T("Shell"), _T("ExplorerMenuOptOut"), 0) != 0;
}

void ShellIntegration::SetOptedOut(bool optedOut)
{
    AfxGetApp()->WriteProfileInt(_T("Shell"), _T("ExplorerMenuOptOut"), optedOut ? 1 : 0);
}

void ShellIntegration::EnsureRegistered()
{
    if (IsOptedOut() || IsProcessElevated() || IsRegistered())
        return;
    std::wstring ignored;
    Register(ignored);
}

bool ShellIntegration::Unregister(std::wstring& errorOut)
{
    for (const wchar_t* key : kClassKeys)
    {
        LONG rc = RegDeleteTreeW(HKEY_CURRENT_USER, key);
        if (rc != ERROR_SUCCESS && rc != ERROR_FILE_NOT_FOUND)
        {
            errorOut = L"Could not remove the Explorer menu entry: " + ErrorText(rc);
            return false;
        }
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return true;
}
