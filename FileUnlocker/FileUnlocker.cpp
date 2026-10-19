// FileUnlocker.cpp
#include "pch.h"
#include <restartmanager.h>
#include <tlhelp32.h>
#include <algorithm>
#include <cwctype>
#include "FileUnlocker.h"

#pragma comment(lib, "Rstrtmgr.lib")

namespace
{
    std::wstring GetProcessName(DWORD pid)
    {
        std::wstring name = L"Unknown Process";
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot != INVALID_HANDLE_VALUE)
        {
            PROCESSENTRY32W pe;
            pe.dwSize = sizeof(pe);
            if (Process32FirstW(hSnapshot, &pe))
            {
                do
                {
                    if (pe.th32ProcessID == pid)
                    {
                        name = pe.szExeFile;
                        break;
                    }
                } while (Process32NextW(hSnapshot, &pe));
            }
            CloseHandle(hSnapshot);
        }
        return name;
    }

    std::wstring GetProcessImagePath(DWORD pid)
    {
        std::wstring path;
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (hProcess)
        {
            WCHAR buf[MAX_PATH * 4] = {};
            DWORD len = (DWORD)_countof(buf);
            if (QueryFullProcessImageNameW(hProcess, 0, buf, &len))
                path.assign(buf, len);
            CloseHandle(hProcess);
        }
        return path;
    }

    // Collects the files under a folder for Restart Manager, which only
    // accepts file names. Reparse points are skipped so a junction can't
    // send the walk somewhere unrelated (or round in a loop).
    void CollectFolderFiles(const std::wstring& folder, std::vector<std::wstring>& out, size_t maxFiles)
    {
        if (out.size() >= maxFiles)
            return;

        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileExW((folder + L"\\*").c_str(), FindExInfoBasic, &fd,
                                        FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
        if (hFind == INVALID_HANDLE_VALUE)
            return;

        do
        {
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
                continue;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
                continue;

            std::wstring child = folder + L"\\" + fd.cFileName;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                CollectFolderFiles(child, out, maxFiles);
            else
                out.push_back(child);
        } while (out.size() < maxFiles && FindNextFileW(hFind, &fd));

        FindClose(hFind);
    }

    std::wstring ToLower(std::wstring s)
    {
        std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return (wchar_t)std::towlower(c); });
        return s;
    }
}

bool FileUnlocker::IsCriticalSystemProcess(const std::wstring& processName)
{
    static const wchar_t* kCritical[] = {
        L"csrss.exe", L"wininit.exe", L"winlogon.exe", L"services.exe",
        L"lsass.exe", L"smss.exe", L"system", L"registry"
    };

    std::wstring lower = ToLower(processName);
    for (const wchar_t* c : kCritical)
        if (lower == c)
            return true;
    return false;
}

std::vector<LockingProcess> FileUnlocker::FindLockingProcesses(const std::wstring& filePath)
{
    std::vector<LockingProcess> lockingProcesses;
    DWORD dwSession = 0;
    WCHAR szSessionKey[CCH_RM_SESSION_KEY + 1] = { 0 };

    DWORD dwError = RmStartSession(&dwSession, 0, szSessionKey);
    if (dwError != ERROR_SUCCESS)
        return lockingProcesses;

    std::vector<std::wstring> files;
    DWORD attrs = GetFileAttributesW(filePath.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY))
        CollectFolderFiles(filePath, files, kMaxFolderFiles);
    else
        files.push_back(filePath);

    if (files.empty())
    {
        RmEndSession(dwSession);
        return lockingProcesses;
    }

    std::vector<PCWSTR> fileNames;
    fileNames.reserve(files.size());
    for (const auto& f : files)
        fileNames.push_back(f.c_str());

    dwError = RmRegisterResources(dwSession, (UINT)fileNames.size(), fileNames.data(), 0, nullptr, 0, nullptr);
    if (dwError != ERROR_SUCCESS)
    {
        RmEndSession(dwSession);
        return lockingProcesses;
    }

    UINT dwProcInfoNeeded = 0;
    UINT dwProcInfo = 0;
    DWORD dwReason = 0;

    dwError = RmGetList(dwSession, &dwProcInfoNeeded, &dwProcInfo, nullptr, &dwReason);
    if (dwError == ERROR_MORE_DATA && dwProcInfoNeeded > 0)
    {
        std::vector<RM_PROCESS_INFO> rgAffectedApps(dwProcInfoNeeded);
        dwProcInfo = dwProcInfoNeeded;

        dwError = RmGetList(dwSession, &dwProcInfoNeeded, &dwProcInfo, rgAffectedApps.data(), &dwReason);
        if (dwError == ERROR_SUCCESS)
        {
            for (UINT i = 0; i < dwProcInfo; ++i)
            {
                LockingProcess lp;
                lp.processId = rgAffectedApps[i].Process.dwProcessId;

                bool seen = false; // a folder check can report the same process once per file
                for (const auto& existing : lockingProcesses)
                    seen = seen || existing.processId == lp.processId;
                if (seen)
                    continue;

                if (wcslen(rgAffectedApps[i].strAppName) > 0)
                    lp.processName = rgAffectedApps[i].strAppName;
                else
                    lp.processName = GetProcessName(lp.processId);
                lp.imagePath = GetProcessImagePath(lp.processId);

                lockingProcesses.push_back(lp);
            }
        }
    }

    RmEndSession(dwSession);
    return lockingProcesses;
}

bool FileUnlocker::TerminateLockingProcess(DWORD processId, const std::wstring& processName, std::wstring& errorOut)
{
    if (processId == GetCurrentProcessId())
    {
        errorOut = L"Refusing to terminate this application's own process.";
        return false;
    }
    if (IsCriticalSystemProcess(processName))
    {
        errorOut = L"Refusing to terminate " + processName + L" - it's a core Windows process; " +
            L"forcing it closed can crash or destabilize the system.";
        return false;
    }

    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processId);
    if (hProcess == nullptr || hProcess == INVALID_HANDLE_VALUE)
    {
        errorOut = L"Could not open the process (it may already have exited, or access was denied).";
        return false;
    }

    BOOL ok = TerminateProcess(hProcess, 1);
    CloseHandle(hProcess);

    if (!ok)
    {
        errorOut = L"TerminateProcess failed.";
        return false;
    }
    return true;
}

bool FileUnlocker::UnlockFile(const std::wstring& filePath, bool forceTerminate)
{
    std::vector<LockingProcess> locks = FindLockingProcesses(filePath);
    if (locks.empty())
        return true;

    bool allUnlocked = true;
    for (const auto& proc : locks)
    {
        if (!forceTerminate)
        {
            allUnlocked = false;
            continue;
        }

        std::wstring err;
        if (!TerminateLockingProcess(proc.processId, proc.processName, err))
            allUnlocked = false;
    }

    return allUnlocked;
}
