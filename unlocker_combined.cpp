#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <restartmanager.h>
#include <stdio.h>
#include <tchar.h>
#include <psapi.h>
#include <string>
#include <vector>
#include <algorithm>

#pragma comment(lib, "rstrtmgr.lib")
#pragma comment(lib, "psapi.lib")

enum class ErrorCode {
    SUCCESS = 0,
    INVALID_ARGUMENTS,
    FILE_NOT_FOUND,
    RM_SESSION_FAILED,
    RM_REGISTER_FAILED,
    RM_GETLIST_FAILED,
    NO_PROCESSES_USING_FILE,
    PROCESS_TERMINATION_FAILED,
    FILE_DELETE_FAILED,
    PERMISSION_DENIED,
    UNKNOWN_ERROR
};

struct ProcessInfo {
    DWORD processId;
    std::wstring processName;
    std::wstring appName;
};

std::wstring GetProcessNameFromPid(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hProcess) {
        return L"<未知进程>";
    }

    WCHAR processName[MAX_PATH] = L"<未知>";
    if (GetModuleFileNameExW(hProcess, nullptr, processName, MAX_PATH)) {
        CloseHandle(hProcess);
        return processName;
    }

    if (GetProcessImageFileNameW(hProcess, processName, MAX_PATH)) {
        CloseHandle(hProcess);
        std::wstring name = processName;
        size_t pos = name.rfind(L'\\');
        if (pos != std::wstring::npos) {
            return L"\\Device\\HarddiskVolume" + name.substr(pos);
        }
        return name;
    }

    CloseHandle(hProcess);
    return L"<未知进程>";
}

std::vector<ProcessInfo> GetProcessesUsingFile(const std::wstring& filePath) {
    std::vector<ProcessInfo> processes;

    DWORD sessionHandle = 0;
    WCHAR sessionKey[CCH_RM_SESSION_KEY + 1] = {0};

    DWORD ret = RmStartSession(&sessionHandle, 0, sessionKey);
    if (ret != ERROR_SUCCESS) {
        return processes;
    }

    LPCWSTR filePaths[] = { filePath.c_str() };
    ret = RmRegisterResources(sessionHandle, 1, filePaths, 0, nullptr, 0, nullptr);
    if (ret != ERROR_SUCCESS) {
        RmEndSession(sessionHandle);
        return processes;
    }

    UINT procInfoCount = 0;
    UINT procInfoNeeded = 0;
    DWORD rebootReasons = 0;

    ret = RmGetList(sessionHandle, &procInfoNeeded, &procInfoCount, nullptr, &rebootReasons);
    if (ret != ERROR_MORE_DATA && ret != ERROR_SUCCESS) {
        RmEndSession(sessionHandle);
        return processes;
    }

    if (procInfoNeeded == 0) {
        RmEndSession(sessionHandle);
        return processes;
    }

    std::vector<RM_PROCESS_INFO> procInfos(procInfoNeeded);
    procInfoCount = procInfoNeeded;

    ret = RmGetList(sessionHandle, &procInfoNeeded, &procInfoCount, procInfos.data(), &rebootReasons);
    if (ret != ERROR_SUCCESS) {
        RmEndSession(sessionHandle);
        return processes;
    }

    for (UINT i = 0; i < procInfoCount; i++) {
        ProcessInfo info;
        info.processId = procInfos[i].Process.dwProcessId;
        info.processName = GetProcessNameFromPid(info.processId);
        info.appName = procInfos[i].strAppName;
        processes.push_back(info);
    }

    RmEndSession(sessionHandle);
    return processes;
}

bool TerminateProcessById(DWORD pid, const std::wstring& processName) {
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
    if (!hProcess) {
        return false;
    }

    bool success = false;
    if (TerminateProcess(hProcess, 0)) {
        if (WaitForSingleObject(hProcess, 5000) == WAIT_OBJECT_0) {
            success = true;
        }
    }

    CloseHandle(hProcess);
    return success;
}

bool CloseFileHandles(const std::wstring& filePath) {
    DWORD sessionHandle = 0;
    WCHAR sessionKey[CCH_RM_SESSION_KEY + 1] = {0};

    DWORD ret = RmStartSession(&sessionHandle, 0, sessionKey);
    if (ret != ERROR_SUCCESS) {
        return false;
    }

    LPCWSTR filePaths[] = { filePath.c_str() };
    ret = RmRegisterResources(sessionHandle, 1, filePaths, 0, nullptr, 0, nullptr);
    if (ret != ERROR_SUCCESS) {
        RmEndSession(sessionHandle);
        return false;
    }

    UINT procInfoCount = 0;
    UINT procInfoNeeded = 0;
    DWORD rebootReasons = 0;

    ret = RmGetList(sessionHandle, &procInfoNeeded, &procInfoCount, nullptr, &rebootReasons);
    if (ret != ERROR_MORE_DATA && ret != ERROR_SUCCESS) {
        RmEndSession(sessionHandle);
        return false;
    }

    if (procInfoNeeded == 0) {
        RmEndSession(sessionHandle);
        return true;
    }

    std::vector<RM_PROCESS_INFO> procInfos(procInfoNeeded);
    procInfoCount = procInfoNeeded;

    ret = RmGetList(sessionHandle, &procInfoNeeded, &procInfoCount, procInfos.data(), &rebootReasons);
    if (ret != ERROR_SUCCESS) {
        RmEndSession(sessionHandle);
        return false;
    }

    bool allClosed = true;
    for (UINT i = 0; i < procInfoCount; i++) {
        DWORD pid = procInfos[i].Process.dwProcessId;
        std::wstring procName = GetProcessNameFromPid(pid);

        wprintf(L"[信息] 正在终止进程: %s (PID: %lu)\n", procName.c_str(), pid);

        if (!TerminateProcessById(pid, procName)) {
            wprintf(L"[警告] 无法终止进程: %s (PID: %lu)\n", procName.c_str(), pid);
            allClosed = false;
        }
        else {
            wprintf(L"[成功] 已终止进程: %s (PID: %lu)\n", procName.c_str(), pid);
        }
    }

    RmEndSession(sessionHandle);
    return allClosed;
}

ErrorCode UnlockFile(const std::wstring& filePath) {
    DWORD attrs = GetFileAttributesW(filePath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
            return ErrorCode::FILE_NOT_FOUND;
        }
        else if (err == ERROR_ACCESS_DENIED) {
            return ErrorCode::PERMISSION_DENIED;
        }
        return ErrorCode::UNKNOWN_ERROR;
    }

    std::vector<ProcessInfo> processes = GetProcessesUsingFile(filePath);

    if (processes.empty()) {
        return ErrorCode::SUCCESS;
    }

    if (CloseFileHandles(filePath)) {
        processes = GetProcessesUsingFile(filePath);
        if (processes.empty()) {
            return ErrorCode::SUCCESS;
        }
        else {
            return ErrorCode::PROCESS_TERMINATION_FAILED;
        }
    }
    else {
        return ErrorCode::PROCESS_TERMINATION_FAILED;
    }
}

ErrorCode SmashFile(const std::wstring& filePath) {
    DWORD attrs = GetFileAttributesW(filePath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
            return ErrorCode::FILE_NOT_FOUND;
        }
        else if (err == ERROR_ACCESS_DENIED) {
            return ErrorCode::PERMISSION_DENIED;
        }
        return ErrorCode::UNKNOWN_ERROR;
    }

    std::vector<ProcessInfo> processes = GetProcessesUsingFile(filePath);

    if (!processes.empty()) {
        CloseFileHandles(filePath);
        Sleep(500);
    }

    if (attrs & FILE_ATTRIBUTE_READONLY) {
        SetFileAttributesW(filePath.c_str(), attrs & ~FILE_ATTRIBUTE_READONLY);
    }

    if (DeleteFileW(filePath.c_str())) {
        return ErrorCode::SUCCESS;
    }

    DWORD lastError = GetLastError();

    if (lastError == ERROR_ACCESS_DENIED) {
        WCHAR tempPath[MAX_PATH];
        WCHAR tempFile[MAX_PATH];
        if (GetTempPathW(MAX_PATH, tempPath) &&
            GetTempFileNameW(tempPath, L"del", 0, tempFile)) {

            if (MoveFileExW(filePath.c_str(), tempFile, MOVEFILE_REPLACE_EXISTING)) {
                if (MoveFileExW(tempFile, nullptr, MOVEFILE_DELAY_UNTIL_REBOOT)) {
                    return ErrorCode::SUCCESS;
                }
                DeleteFileW(tempFile);
            }
            else {
                DeleteFileW(tempFile);
            }
        }

        if (MoveFileExW(filePath.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT)) {
            return ErrorCode::SUCCESS;
        }
    }

    return ErrorCode::FILE_DELETE_FAILED;
}

bool IsDirectoryPath(const std::wstring& path) {
    DWORD attrs = GetFileAttributesW(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    return (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static std::vector<std::wstring> GetAllFilesInDirectory(const std::wstring& dirPath) {
    std::vector<std::wstring> files;
    std::vector<std::wstring> dirs;
    
    dirs.push_back(dirPath);
    
    while (!dirs.empty()) {
        std::wstring currentDir = dirs.back();
        dirs.pop_back();
        
        files.push_back(currentDir);
        
        WIN32_FIND_DATAW findData;
        std::wstring searchPath = currentDir + L"\\*";
        
        HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
        if (hFind == INVALID_HANDLE_VALUE) continue;
        
        do {
            std::wstring name = findData.cFileName;
            if (name == L"." || name == L"..") continue;
            
            std::wstring fullPath = currentDir + L"\\" + name;
            
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                dirs.push_back(fullPath);
            } else {
                files.push_back(fullPath);
            }
        } while (FindNextFileW(hFind, &findData));
        
        FindClose(hFind);
    }
    
    return files;
}

ErrorCode UnlockDirectory(const std::wstring& dirPath) {
    std::vector<std::wstring> items = GetAllFilesInDirectory(dirPath);
    
    bool allSuccess = true;
    int processedCount = 0;
    int totalCount = (int)items.size();
    
    for (const auto& item : items) {
        processedCount++;
        wprintf(L"[进度] 处理中: %d/%d - %s\n", processedCount, totalCount, item.c_str());
        
        DWORD attrs = GetFileAttributesW(item.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES) continue;
        
        if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
            std::vector<ProcessInfo> processes = GetProcessesUsingFile(item);
            if (!processes.empty()) {
                wprintf(L"[信息] 目录被占用: %s\n", item.c_str());
                if (!CloseFileHandles(item)) {
                    allSuccess = false;
                }
            }
        } else {
            ErrorCode result = UnlockFile(item);
            if (result != ErrorCode::SUCCESS && result != ErrorCode::NO_PROCESSES_USING_FILE) {
                allSuccess = false;
            }
        }
    }
    
    return allSuccess ? ErrorCode::SUCCESS : ErrorCode::PROCESS_TERMINATION_FAILED;
}

ErrorCode SmashDirectory(const std::wstring& dirPath) {
    wprintf(L"[信息] 开始处理目录: %s\n", dirPath.c_str());
    
    ErrorCode unlockResult = UnlockDirectory(dirPath);
    if (unlockResult != ErrorCode::SUCCESS) {
        wprintf(L"[警告] 部分文件解锁失败，继续尝试删除...\n");
    }
    
    std::vector<std::wstring> items = GetAllFilesInDirectory(dirPath);
    
    std::sort(items.begin(), items.end(), [](const std::wstring& a, const std::wstring& b) {
        return a.length() > b.length();
    });
    
    bool allSuccess = true;
    int processedCount = 0;
    int totalCount = (int)items.size();
    
    for (const auto& item : items) {
        if (item == dirPath) continue;
        
        processedCount++;
        wprintf(L"[进度] 删除中: %d/%d - %s\n", processedCount, totalCount, item.c_str());
        
        DWORD attrs = GetFileAttributesW(item.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES) continue;
        
        if (attrs & FILE_ATTRIBUTE_READONLY) {
            SetFileAttributesW(item.c_str(), attrs & ~FILE_ATTRIBUTE_READONLY);
        }
        
        bool deleted = false;
        if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
            deleted = RemoveDirectoryW(item.c_str()) != 0;
        } else {
            deleted = DeleteFileW(item.c_str()) != 0;
        }
        
        if (!deleted) {
            DWORD lastError = GetLastError();
            if (lastError == ERROR_ACCESS_DENIED) {
                if (MoveFileExW(item.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT)) {
                    wprintf(L"[信息] 将在重启后删除: %s\n", item.c_str());
                    deleted = true;
                }
            }
        }
        
        if (!deleted) {
            wprintf(L"[警告] 删除失败: %s\n", item.c_str());
            allSuccess = false;
        }
    }
    
    DWORD attrs = GetFileAttributesW(dirPath.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES) {
        if (attrs & FILE_ATTRIBUTE_READONLY) {
            SetFileAttributesW(dirPath.c_str(), attrs & ~FILE_ATTRIBUTE_READONLY);
        }
        
        if (RemoveDirectoryW(dirPath.c_str())) {
            wprintf(L"[成功] 已删除目录: %s\n", dirPath.c_str());
        } else {
            DWORD lastError = GetLastError();
            if (lastError == ERROR_ACCESS_DENIED) {
                if (MoveFileExW(dirPath.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT)) {
                    wprintf(L"[信息] 目录将在重启后删除: %s\n", dirPath.c_str());
                } else {
                    wprintf(L"[错误] 无法删除目录: %s\n", dirPath.c_str());
                    allSuccess = false;
                }
            } else {
                wprintf(L"[错误] 无法删除目录: %s (错误: %lu)\n", dirPath.c_str(), lastError);
                allSuccess = false;
            }
        }
    }
    
    return allSuccess ? ErrorCode::SUCCESS : ErrorCode::FILE_DELETE_FAILED;
}

extern "C" {

bool is_directory(const wchar_t* path) {
    if (!path) return false;
    return IsDirectoryPath(path);
}

bool unlock_file(const wchar_t* filePath) {
    if (!filePath) return false;
    ErrorCode result = UnlockFile(filePath);
    return result == ErrorCode::SUCCESS;
}

bool unlock_directory(const wchar_t* dirPath) {
    if (!dirPath) return false;
    ErrorCode result = UnlockDirectory(dirPath);
    return result == ErrorCode::SUCCESS;
}

bool smash_file(const wchar_t* filePath) {
    if (!filePath) return false;
    ErrorCode result = SmashFile(filePath);
    return result == ErrorCode::SUCCESS;
}

bool smash_directory(const wchar_t* dirPath) {
    if (!dirPath) return false;
    ErrorCode result = SmashDirectory(dirPath);
    return result == ErrorCode::SUCCESS;
}

bool unlock(const wchar_t* path) {
    if (!path) return false;
    
    if (IsDirectoryPath(path)) {
        return unlock_directory(path);
    } else {
        return unlock_file(path);
    }
}

bool smash(const wchar_t* path) {
    if (!path) return false;
    
    if (IsDirectoryPath(path)) {
        return smash_directory(path);
    } else {
        return smash_file(path);
    }
}

}
