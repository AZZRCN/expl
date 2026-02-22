#include <stdio.h>
#include <conio.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <io.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <cstdint>
#include <thread>
#include <mutex>
#include <atomic>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <wincrypt.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <functional>
#include <ctime>
#include <chrono>
#include <map>
#include <lm.h>
#include <lmshare.h>
#include <lmapibuf.h>
#include <mmsystem.h>
#include <bcrypt.h>
#include <ncrypt.h>

#include "dlcore_combined.cpp"
#include "unlocker_combined.cpp"
#include "7z/SevenZipSDK.h"

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "netapi32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "ncrypt.lib")

using namespace std;
using namespace dl;

#define MAX_PATH_LEN 1024
#define MAX_CMD_LEN 1024
#define MAX_MATCHES 100

typedef enum {
    ERR_VIRTUAL_KEY_INPUT = -1,
    LEFT = -129,
    RIGHT = -130,
    UP = -131,
    DOWN = -132,
    BACKSPACE = -133,
    TAB = -134,
    ENTER = -135,
    LEFT_ASCII = 75,
    RIGHT_ASCII = 77,
    UP_ASCII = 72,
    DOWN_ASCII = 80,
    BACKSPACE_ASCII = 8,
    TAB_ASCII = 9,
    ENTER_ASCII = 13,
    VIRTUAL_KEY = 224
} KeyCode;

struct FileInfo {
    wstring name;
    uint32_t attrib;
    FileInfo(const wstring& n, uint32_t a) : name(n), attrib(a) {}
};

struct AppState {
    vector<wstring> currentPath;
    vector<FileInfo> files;
    vector<FileInfo> dirs;
    wchar_t cmdBuffer[MAX_CMD_LEN];
    int cmdLength;
    int cursorPos;
    vector<wstring> matches;
    vector<uint32_t> matchAttribs;
    vector<bool> matchIsDir;
    int matchIndex;
    bool showMatches;
    bool isEnvVarMatch;
    int envVarStartPos;
    int scrollOffset;
    wstring statusMsg;
    bool showHelp;
    int helpScrollOffset;
    vector<wstring> cmdHistory;
    vector<wstring> cmdResults;
    bool showHistory;
    int historyScrollOffset;
    
    DownloadManager* downloadManager;
    vector<string> downloadTaskIds;
    mutex downloadMutex;
    atomic<bool> downloadRunning;
    string currentDownloadId;
    int downloadProgress;
    uint64_t downloadSpeed;
    uint64_t downloadTotal;
    uint64_t downloadDownloaded;
    string downloadFileName;
    bool showDownloadProgress;
    
    vector<TaskInfo> downloadHistory;
    bool showDownloadHistory;
    int downloadHistoryIndex;
    
    SevenZip::SevenZipArchive* sevenZipArchive;
};

struct CommandHint {
    wstring name;
    vector<wstring> params;
    wstring desc;
};

int fcase();
COORD getConsoleSize();
void clearScreen();
void setCursor(int x, int y);
void truncateString(wstring& str, int maxLen);
wstring getAttribStr(uint32_t attrib, bool isDir);
vector<wstring> splitString(const wstring& str, wchar_t delimiter = L' ');
wstring expandEnvVars(const wstring& str);
vector<wstring> getEnvVarNames(const wstring& prefix = L"");
vector<CommandHint> getCommandHints();
wstring getCommandHint(const wstring& cmd, int argIndex);

wstring StringToWString(const string& str);
string WStringToString(const wstring& wstr);

wstring mergePath(const vector<wstring>& path);
bool isAbsolutePath(const wstring& path);
vector<wstring> parseAbsolutePath(const wstring& path);
bool isValidDrive(const wstring& drive);
wstring findFirstValidDrive();
bool isAtDriveRoot(const vector<wstring>& path);
vector<wstring> parseRelativePath(const vector<wstring>& basePath, const wstring& relPath);
bool pathExists(const vector<wstring>& path);
vector<wstring> findValidParentPath(vector<wstring> path);
wstring findDirCaseInsensitive(const vector<FileInfo>& dirs, const wstring& name);
wstring findFileCaseInsensitive(const vector<FileInfo>& files, const wstring& name);
bool getFiles(AppState* state);
wstring resolveFullPath(AppState* state, const wstring& arg);

void findMatches(AppState* state);
void applyMatch(AppState* state);

void setStatus(AppState* state, bool success, const string& cmd, const string& arg, const string& error);
void setStatus(AppState* state, bool success, const string& cmd, const string& error);
void setStatus(AppState* state, bool success, const string& cmd);

void cmdIpconfig();
void cmdPing(const string& target);
void cmdNetstat(const string& option);
void cmdDownloadBlocking(AppState* state, const string& url, const string& fileName);
void executeCommand(AppState* state);

void renderHelp(AppState* state);
void renderHistory(AppState* state);
void renderDownloadHistory(AppState* state);
void renderDownloadProgress(AppState* state, int startY, int availableHeight);
void render(AppState* state);

int fcase() {
    int c1;
    switch (c1 = getch()) {
    case VIRTUAL_KEY:
        switch (getch()) {
        case LEFT_ASCII: return LEFT;
        case RIGHT_ASCII: return RIGHT;
        case UP_ASCII: return UP;
        case DOWN_ASCII: return DOWN;
        default: return ERR_VIRTUAL_KEY_INPUT;
        }
    case BACKSPACE_ASCII: return BACKSPACE;
    case TAB_ASCII: return TAB;
    case ENTER_ASCII: return ENTER;
    default: return c1;
    }
}

COORD getConsoleSize() {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    COORD size;
    size.X = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    size.Y = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    return size;
}

void clearScreen() {
    COORD size = getConsoleSize();
    COORD coordScreen = {0, 0};
    DWORD cCharsWritten;
    DWORD dwConSize = size.X * size.Y;
    FillConsoleOutputCharacter(GetStdHandle(STD_OUTPUT_HANDLE), ' ', dwConSize, coordScreen, &cCharsWritten);
    FillConsoleOutputAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE, dwConSize, coordScreen, &cCharsWritten);
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coordScreen);
}

void setCursor(int x, int y) {
    COORD coord;
    coord.X = x;
    coord.Y = y;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
}

void truncateString(wstring& str, int maxLen) {
    if ((int)str.size() > maxLen) {
        if (maxLen > 3) {
            str = str.substr(0, maxLen - 3) + L"...";
        } else {
            str = str.substr(0, maxLen);
        }
    }
}

wstring getAttribStr(uint32_t attrib, bool isDir) {
    if (isDir) {
        return L"DIR ";
    }
    wstring result = L"----";
    if (attrib & _A_RDONLY) result[0] = L'R';
    if (attrib & _A_HIDDEN) result[1] = L'H';
    if (attrib & _A_SYSTEM) result[2] = L'S';
    if (attrib & _A_ARCH)   result[3] = L'A';
    return result;
}

vector<wstring> splitString(const wstring& str, wchar_t delimiter) {
    vector<wstring> ret;
    wstring tmp;
    bool inQuote = false;
    for (size_t i = 0; i < str.size(); i++) {
        wchar_t c = str[i];
        if (c == L'"') {
            inQuote = !inQuote;
        } else if (c == delimiter && !inQuote) {
            if (!tmp.empty()) {
                ret.push_back(tmp);
                tmp.clear();
            }
        } else {
            tmp.push_back(c);
        }
    }
    if (!tmp.empty()) ret.push_back(tmp);
    return ret;
}

wstring expandEnvVars(const wstring& str) {
    wstring result;
    size_t i = 0;
    while (i < str.size()) {
        if (str[i] == L'%') {
            size_t end = str.find(L'%', i + 1);
            if (end != wstring::npos && end > i + 1) {
                wstring varName = str.substr(i + 1, end - i - 1);
                char* envValue = getenv(WStringToString(varName).c_str());
                if (envValue) {
                    result += StringToWString(string(envValue));
                } else {
                    result += str.substr(i, end - i + 1);
                }
                i = end + 1;
            } else {
                result += str[i];
                i++;
            }
        } else {
            result += str[i];
            i++;
        }
    }
    return result;
}

vector<wstring> getEnvVarNames(const wstring& prefix) {
    vector<wstring> result;
    extern char** environ;
    char** env = environ;
    while (*env) {
        string entry = *env;
        size_t eqPos = entry.find('=');
        if (eqPos != string::npos) {
            string varName = entry.substr(0, eqPos);
            wstring wVarName = StringToWString(varName);
            if (prefix.empty() || 
                (wVarName.size() >= prefix.size() && 
                 _wcsnicmp(wVarName.c_str(), prefix.c_str(), prefix.size()) == 0)) {
                result.push_back(wVarName);
            }
        }
        env++;
    }
    return result;
}

vector<CommandHint> getCommandHints() {
    vector<CommandHint> hints;
    hints.push_back({L"cd", {L"path"}, L"Change directory"});
    hints.push_back({L"run", {L"file"}, L"Open file"});
    hints.push_back({L"copy", {L"src", L"dst"}, L"Copy file"});
    hints.push_back({L"move", {L"src", L"dst"}, L"Move/rename"});
    hints.push_back({L"mkdir", {L"name"}, L"Create directory"});
    hints.push_back({L"rm", {L"target"}, L"Delete file/dir"});
    hints.push_back({L"ren", {L"old", L"new"}, L"Rename file/dir"});
    hints.push_back({L"attrib", {L"flags", L"file"}, L"View/change file attributes"});
    hints.push_back({L"cat", {L"file"}, L"Display file content"});
    hints.push_back({L"head", {L"file", L"lines"}, L"Show first N lines"});
    hints.push_back({L"tail", {L"file", L"lines"}, L"Show last N lines"});
    hints.push_back({L"touch", {L"file"}, L"Create/update file"});
    hints.push_back({L"grep", {L"pattern", L"file"}, L"Search in file"});
    hints.push_back({L"find", {L"pattern", L"path"}, L"Find files by name"});
    hints.push_back({L"tree", {L"path"}, L"Show directory tree"});
    hints.push_back({L"wc", {L"file"}, L"Count lines/words/chars"});
    hints.push_back({L"sort", {L"file"}, L"Sort file lines"});
    hints.push_back({L"uniq", {L"file"}, L"Count unique lines"});
    hints.push_back({L"diff", {L"file1", L"file2"}, L"Compare files"});
    hints.push_back({L"unlock", {L"file/dir"}, L"Unlock locked file/directory"});
    hints.push_back({L"smash", {L"file/dir"}, L"Force delete file/directory"});
    hints.push_back({L"7z", {L"cmd", L"args"}, L"7-Zip: compress/extract"});
    hints.push_back({L"7zlist", {L"archive"}, L"List archive contents"});
    hints.push_back({L"7zextract", {L"archive", L"output"}, L"Extract archive"});
    hints.push_back({L"download", {L"url", L"filename"}, L"Download file from URL"});
    hints.push_back({L"dl", {L"url", L"filename"}, L"Blocking download"});
    hints.push_back({L"dlstatus", {}, L"Show download status"});
    hints.push_back({L"dlhistory", {}, L"Show download history"});
    hints.push_back({L"curl", {L"url"}, L"HTTP request"});
    hints.push_back({L"ipconfig", {}, L"Show IP configuration"});
    hints.push_back({L"ping", {L"host"}, L"Ping a host"});
    hints.push_back({L"netstat", {}, L"Show network connections"});
    hints.push_back({L"dig", {L"hostname"}, L"DNS lookup"});
    hints.push_back({L"ps", {}, L"List processes"});
    hints.push_back({L"kill", {L"pid|name"}, L"Kill process"});
    hints.push_back({L"hash", {L"file", L"algo"}, L"Calculate hash (md5/sha1/sha256)"});
    hints.push_back({L"clip", {L"text"}, L"Copy to clipboard"});
    hints.push_back({L"paste", {L"file"}, L"Paste from clipboard"});
    hints.push_back({L"sysinfo", {}, L"Show system information"});
    hints.push_back({L"du", {L"path"}, L"Directory size analysis"});
    hints.push_back({L"df", {}, L"Show disk space"});
    hints.push_back({L"ls", {}, L"List directory"});
    hints.push_back({L"edit", {L"file"}, L"Edit file in notepad"});
    hints.push_back({L"open", {L"path"}, L"Open in explorer"});
    hints.push_back({L"set", {L"VAR=value"}, L"Set env var"});
    hints.push_back({L"get", {L"VAR"}, L"Get env var"});
    hints.push_back({L"echo", {L"text"}, L"Print text"});
    hints.push_back({L"which", {L"command"}, L"Find command location"});
    hints.push_back({L"whoami", {}, L"Show current user"});
    hints.push_back({L"hostname", {}, L"Show computer name"});
    hints.push_back({L"time", {}, L"Show current time"});
    hints.push_back({L"date", {}, L"Show current date"});
    hints.push_back({L"uptime", {}, L"Show system uptime"});
    hints.push_back({L"help", {}, L"Show help"});
    hints.push_back({L"history", {}, L"Show command history"});
    hints.push_back({L"pwd", {}, L"Print working dir"});
    hints.push_back({L"cls", {}, L"Clear screen"});
    hints.push_back({L"exit", {}, L"Exit program"});
    return hints;
}

wstring getCommandHint(const wstring& cmd, int argIndex) {
    vector<CommandHint> hints = getCommandHints();
    for (const auto& h : hints) {
        if (h.name == cmd) {
            wstring result = cmd + L"[" + to_wstring(h.params.size()) + L"]";
            for (size_t i = 0; i < h.params.size(); i++) {
                result += L" ";
                if (argIndex == -1) {
                    result += L"<" + h.params[i] + L">";
                } else if ((int)i == argIndex) {
                    result += L"<" + h.params[i] + L">";
                } else {
                    result += h.params[i];
                }
            }
            return result;
        }
    }
    return cmd + L"[?]";
}

wstring StringToWString(const string& str) {
    if (str.empty()) return wstring();
    int size = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, NULL, 0);
    wstring result(size - 1, 0);
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, &result[0], size);
    return result;
}

string WStringToString(const wstring& wstr) {
    if (wstr.empty()) return string();
    int size = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    string result(size - 1, 0);
    WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, &result[0], size, NULL, NULL);
    return result;
}

wstring mergePath(const vector<wstring>& path) {
    wstring result;
    for (size_t i = 0; i < path.size(); i++) {
        if (i != 0) result += L"\\";
        result += path[i];
    }
    return result;
}

bool isAbsolutePath(const wstring& path) {
    if (path.size() >= 2 && path[1] == L':') {
        return true;
    }
    return false;
}

vector<wstring> parseAbsolutePath(const wstring& path) {
    vector<wstring> result;
    wstring current;
    for (size_t i = 0; i < path.size(); i++) {
        if (path[i] == L'\\' || path[i] == L'/') {
            if (!current.empty()) {
                result.push_back(current);
                current.clear();
            }
        } else {
            current += path[i];
        }
    }
    if (!current.empty()) {
        result.push_back(current);
    }
    return result;
}

bool isValidDrive(const wstring& drive) {
    if (drive.size() != 2 || drive[1] != L':') {
        return false;
    }
    wchar_t driveLetter = towupper(drive[0]);
    if (driveLetter < L'A' || driveLetter > L'Z') {
        return false;
    }
    wstring drivePath = drive + L"\\";
    return GetDriveTypeW(drivePath.c_str()) != DRIVE_NO_ROOT_DIR;
}

wstring findFirstValidDrive() {
    for (wchar_t c = L'C'; c <= L'Z'; c++) {
        wstring drive = wstring(1, c) + L":";
        if (isValidDrive(drive)) {
            return drive;
        }
    }
    for (wchar_t c = L'A'; c <= L'B'; c++) {
        wstring drive = wstring(1, c) + L":";
        if (isValidDrive(drive)) {
            return drive;
        }
    }
    return L"C:";
}

bool isAtDriveRoot(const vector<wstring>& path) {
    if (path.size() != 1) return false;
    wstring drive = path[0];
    return (drive.size() == 2 && drive[1] == L':');
}

vector<wstring> parseRelativePath(const vector<wstring>& basePath, const wstring& relPath) {
    vector<wstring> result = basePath;
    
    vector<wstring> parts;
    wstring current;
    for (size_t i = 0; i < relPath.size(); i++) {
        if (relPath[i] == L'\\' || relPath[i] == L'/') {
            if (!current.empty()) {
                parts.push_back(current);
                current.clear();
            }
        } else {
            current += relPath[i];
        }
    }
    if (!current.empty()) {
        parts.push_back(current);
    }
    
    for (const wstring& part : parts) {
        if (part == L".") {
            continue;
        } else if (part == L"..") {
            if (!result.empty() && !isAtDriveRoot(result)) {
                result.pop_back();
            }
        } else {
            result.push_back(part);
        }
    }
    
    return result;
}

bool pathExists(const vector<wstring>& path) {
    wstring testPath = mergePath(path);
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW((testPath + L"\\*").c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        return false;
    }
    FindClose(hFind);
    return true;
}

vector<wstring> findValidParentPath(vector<wstring> path) {
    while (!path.empty() && !pathExists(path)) {
        if (isAtDriveRoot(path)) {
            if (!isValidDrive(path[0])) {
                wstring validDrive = findFirstValidDrive();
                path.clear();
                path.push_back(validDrive);
            }
            break;
        }
        path.pop_back();
    }
    if (path.empty()) {
        path.push_back(findFirstValidDrive());
    }
    return path;
}

wstring findDirCaseInsensitive(const vector<FileInfo>& dirs, const wstring& name) {
    for (const auto& dir : dirs) {
        if (_wcsicmp(dir.name.c_str(), name.c_str()) == 0) {
            return dir.name;
        }
    }
    return L"";
}

wstring findFileCaseInsensitive(const vector<FileInfo>& files, const wstring& name) {
    for (const auto& file : files) {
        if (_wcsicmp(file.name.c_str(), name.c_str()) == 0) {
            return file.name;
        }
    }
    return L"";
}

bool getFiles(AppState* state) {
    state->files.clear();
    state->dirs.clear();
    
    wstring path = mergePath(state->currentPath);
    wstring searchPath;
    
    if (path.empty()) {
        searchPath = L".\\*";
    } else {
        searchPath = path + L"\\*";
    }
    
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
    
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (wcscmp(findData.cFileName, L".") == 0) continue;
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                state->dirs.push_back(FileInfo(findData.cFileName, findData.dwFileAttributes));
            } else {
                state->files.push_back(FileInfo(findData.cFileName, findData.dwFileAttributes));
            }
        } while (FindNextFileW(hFind, &findData));
        FindClose(hFind);
        return false;
    } else {
        if (!state->currentPath.empty() && !isValidDrive(state->currentPath[0])) {
            wstring validDrive = findFirstValidDrive();
            state->currentPath.clear();
            state->currentPath.push_back(validDrive);
            getFiles(state);
            return true;
        }
        return false;
    }
}

wstring resolveFullPath(AppState* state, const wstring& arg) {
    wstring expanded = expandEnvVars(arg);
    if (isAbsolutePath(expanded)) {
        return expanded;
    }
    return mergePath(state->currentPath) + L"\\" + expanded;
}

void findMatches(AppState* state) {
    state->matches.clear();
    state->matchAttribs.clear();
    state->matchIsDir.clear();
    state->matchIndex = 0;
    state->showMatches = false;
    state->isEnvVarMatch = false;
    state->envVarStartPos = -1;
    
    int cursorPos = state->cursorPos;
    
    int argStart = 0;
    for (int i = cursorPos - 1; i >= 0; i--) {
        if (state->cmdBuffer[i] == L' ') {
            argStart = i + 1;
            break;
        }
    }
    
    int argLen = cursorPos - argStart;
    wstring argStr;
    if (argLen > 0) {
        argStr = wstring(state->cmdBuffer + argStart, argLen);
    }
    
    int lastUnclosedPercent = -1;
    int percentCount = 0;
    for (int i = argStart; i < cursorPos; i++) {
        if (state->cmdBuffer[i] == L'%') {
            if (lastUnclosedPercent == -1 || percentCount % 2 == 0) {
                lastUnclosedPercent = i;
            }
            percentCount++;
        }
    }
    
    if (lastUnclosedPercent != -1 && percentCount % 2 == 1) {
        int varStart = lastUnclosedPercent + 1;
        int varLen = cursorPos - varStart;
        wstring varPrefix;
        if (varLen > 0) {
            varPrefix = wstring(state->cmdBuffer + varStart, varLen);
        }
        
        vector<wstring> envVars = getEnvVarNames(varPrefix);
        for (const wstring& varName : envVars) {
            state->matches.push_back(varName);
            state->matchAttribs.push_back(0);
            state->matchIsDir.push_back(false);
        }
        
        state->isEnvVarMatch = true;
        state->envVarStartPos = lastUnclosedPercent;
        
        if (!state->matches.empty()) {
            state->showMatches = true;
        }
        return;
    }
    
    bool hasUndefinedVar = false;
    wstring undefinedVarName;
    size_t checkPos = 0;
    while ((checkPos = argStr.find(L'%', checkPos)) != wstring::npos) {
        size_t endPos = argStr.find(L'%', checkPos + 1);
        if (endPos != wstring::npos && endPos > checkPos + 1) {
            wstring varName = argStr.substr(checkPos + 1, endPos - checkPos - 1);
            wstring expandedVar = expandEnvVars(L"%" + varName + L"%");
            if (expandedVar == L"%" + varName + L"%") {
                hasUndefinedVar = true;
                undefinedVarName = varName;
                break;
            }
            checkPos = endPos + 1;
        } else {
            break;
        }
    }
    
    if (hasUndefinedVar) {
        state->showMatches = true;
        state->isEnvVarMatch = true;
        state->matches.clear();
        return;
    }
    
    wstring expandedArg = expandEnvVars(argStr);
    
    size_t lastSlash = expandedArg.rfind(L'\\');
    wstring basePath;
    wstring matchPrefix;
    
    if (lastSlash == wstring::npos) {
        matchPrefix = expandedArg;
    } else {
        basePath = expandedArg.substr(0, lastSlash);
        matchPrefix = expandedArg.substr(lastSlash + 1);
    }
    
    vector<wstring> targetPath;
    
    if (!basePath.empty() && isAbsolutePath(basePath)) {
        targetPath = parseAbsolutePath(basePath);
    } else if (!basePath.empty()) {
        targetPath = parseRelativePath(state->currentPath, basePath);
    } else {
        targetPath = state->currentPath;
    }
    
    vector<FileInfo> targetDirs;
    vector<FileInfo> targetFiles;
    
    wstring targetPathStr = mergePath(targetPath);
    wstring searchPathStr = targetPathStr.empty() ? L".\\*" : targetPathStr + L"\\*";
    
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPathStr.c_str(), &findData);
    
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (wcscmp(findData.cFileName, L".") == 0) continue;
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                targetDirs.push_back(FileInfo(findData.cFileName, findData.dwFileAttributes));
            } else {
                targetFiles.push_back(FileInfo(findData.cFileName, findData.dwFileAttributes));
            }
        } while (FindNextFileW(hFind, &findData));
        FindClose(hFind);
    }
    
    for (const auto& dir : targetDirs) {
        if (matchPrefix.empty()) {
            state->matches.push_back(dir.name);
            state->matchAttribs.push_back(dir.attrib);
            state->matchIsDir.push_back(true);
        } else if (dir.name.size() >= matchPrefix.size() &&
            _wcsnicmp(dir.name.c_str(), matchPrefix.c_str(), matchPrefix.size()) == 0) {
            state->matches.push_back(dir.name);
            state->matchAttribs.push_back(dir.attrib);
            state->matchIsDir.push_back(true);
        }
    }
    
    for (const auto& file : targetFiles) {
        if (matchPrefix.empty()) {
            state->matches.push_back(file.name);
            state->matchAttribs.push_back(file.attrib);
            state->matchIsDir.push_back(false);
        } else if (file.name.size() >= matchPrefix.size() &&
            _wcsnicmp(file.name.c_str(), matchPrefix.c_str(), matchPrefix.size()) == 0) {
            state->matches.push_back(file.name);
            state->matchAttribs.push_back(file.attrib);
            state->matchIsDir.push_back(false);
        }
    }
    
    if (!state->matches.empty()) {
        state->showMatches = true;
    }
}

void applyMatch(AppState* state) {
    if (state->matches.empty()) return;
    
    wstring selected = state->matches[state->matchIndex];
    
    if (state->isEnvVarMatch) {
        int varStart = state->envVarStartPos + 1;
        int varEnd = state->cursorPos;
        int varLen = varEnd - varStart;
        
        wstring fullVar = L"%" + selected + L"%";
        int insertLen = fullVar.length();
        int suffixLen = state->cmdLength - state->cursorPos;
        int newCmdLen = state->envVarStartPos + insertLen + suffixLen;
        
        if (newCmdLen >= MAX_CMD_LEN - 1) return;
        
        wmemmove(state->cmdBuffer + state->envVarStartPos + insertLen,
                state->cmdBuffer + varEnd,
                suffixLen + 1);
        
        wmemcpy(state->cmdBuffer + state->envVarStartPos, fullVar.c_str(), insertLen);
        
        state->cmdLength = newCmdLen;
        state->cursorPos = state->envVarStartPos + insertLen;
        state->showMatches = false;
        state->isEnvVarMatch = false;
        return;
    }
    
    int searchEnd = state->cursorPos;
    int lastSpacePos = -1;
    for (int i = searchEnd - 1; i >= 0; i--) {
        if (state->cmdBuffer[i] == L' ') {
            lastSpacePos = i;
            break;
        }
    }
    
    int argStart = lastSpacePos + 1;
    int argLen = searchEnd - argStart;
    
    wstring argStr;
    if (argLen > 0) {
        argStr = wstring(state->cmdBuffer + argStart, argLen);
    }
    
    wstring expandedArg = expandEnvVars(argStr);
    
    size_t lastSlash = expandedArg.rfind(L'\\');
    wstring basePath;
    
    if (lastSlash != wstring::npos) {
        basePath = expandedArg.substr(0, lastSlash + 1);
    }
    
    bool hasSpace = selected.find(L' ') != wstring::npos;
    if (hasSpace) {
        selected = L"\"" + selected + L"\"";
    }
    
    wstring fullArg = basePath + selected;
    
    int insertLen = fullArg.length();
    int suffixLen = state->cmdLength - state->cursorPos;
    int newCmdLen = argStart + insertLen + suffixLen;
    
    if (newCmdLen >= MAX_CMD_LEN - 1) return;
    
    wmemmove(state->cmdBuffer + argStart + insertLen, 
            state->cmdBuffer + argStart + argLen, 
            suffixLen + 1);
    
    wmemcpy(state->cmdBuffer + argStart, fullArg.c_str(), insertLen);
    
    state->cmdLength = newCmdLen;
    state->cursorPos = argStart + insertLen;
    state->showMatches = false;
}

void setStatus(AppState* state, bool success, const string& cmd, const string& arg, const string& error) {
    string msg;
    if (success) {
        msg = "SUCCESS command:\"" + cmd + "\"";
        if (!arg.empty()) {
            msg += " arg:\"" + arg + "\"";
        }
    } else {
        msg = "ERROR   command:\"" + cmd + "\"";
        if (!arg.empty()) {
            msg += " arg:\"" + arg + "\"";
        }
        msg += " " + error;
    }
    state->statusMsg = StringToWString(msg);
}

void setStatus(AppState* state, bool success, const string& cmd, const string& error) {
    setStatus(state, success, cmd, "", error);
}

void setStatus(AppState* state, bool success, const string& cmd) {
    setStatus(state, success, cmd, "", "");
}

void cmdIpconfig() {
    clearScreen();
    setCursor(0, 0);
    printf("=== IP Configuration ===\n\n");
    
    PIP_ADAPTER_INFO adapterInfo;
    PIP_ADAPTER_INFO adapter = NULL;
    DWORD dwRetVal = 0;
    ULONG ulOutBufLen = sizeof(IP_ADAPTER_INFO);
    
    adapterInfo = (IP_ADAPTER_INFO*)malloc(sizeof(IP_ADAPTER_INFO));
    if (adapterInfo == NULL) {
        printf("Error allocating memory for adapter info\n");
        return;
    }
    
    if (GetAdaptersInfo(adapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW) {
        free(adapterInfo);
        adapterInfo = (IP_ADAPTER_INFO*)malloc(ulOutBufLen);
        if (adapterInfo == NULL) {
            printf("Error allocating memory for adapter info\n");
            return;
        }
    }
    
    dwRetVal = GetAdaptersInfo(adapterInfo, &ulOutBufLen);
    
    if (dwRetVal == NO_ERROR) {
        adapter = adapterInfo;
        while (adapter) {
            printf("Adapter: %s\n", adapter->Description);
            printf("  IP Address: %s\n", adapter->IpAddressList.IpAddress.String);
            printf("  Subnet Mask: %s\n", adapter->IpAddressList.IpMask.String);
            printf("  Default Gateway: %s\n", adapter->GatewayList.IpAddress.String);
            printf("  MAC Address: %02X-%02X-%02X-%02X-%02X-%02X\n",
                adapter->Address[0], adapter->Address[1], adapter->Address[2],
                adapter->Address[3], adapter->Address[4], adapter->Address[5]);
            printf("  DHCP Enabled: %s\n", (adapter->DhcpEnabled ? "Yes" : "No"));
            if (adapter->DhcpEnabled) {
                printf("  DHCP Server: %s\n", adapter->DhcpServer.IpAddress.String);
            }
            printf("\n");
            adapter = adapter->Next;
        }
    } else {
        printf("GetAdaptersInfo failed with error: %d\n", dwRetVal);
    }
    
    free(adapterInfo);
    
    char hostname[256] = {0};
    gethostname(hostname, sizeof(hostname));
    printf("Host Name: %s\n", hostname);
    
    printf("\nPress any key to continue...\n");
    getch();
}

void cmdPing(const string& target) {
    clearScreen();
    setCursor(0, 0);
    printf("=== Ping %s ===\n\n", target.c_str());
    
    HANDLE hIcmp = IcmpCreateFile();
    if (hIcmp == INVALID_HANDLE_VALUE) {
        printf("Error creating ICMP handle: %d\n", GetLastError());
        printf("\nPress any key to continue...\n");
        getch();
        return;
    }
    
    IPAddr ipAddress = inet_addr(target.c_str());
    if (ipAddress == INADDR_NONE) {
        struct hostent* host = gethostbyname(target.c_str());
        if (host) {
            ipAddress = *(IPAddr*)host->h_addr_list[0];
            printf("Pinging %s [%s]\n\n", target.c_str(), inet_ntoa(*(struct in_addr*)host->h_addr_list[0]));
        } else {
            printf("Could not resolve host: %s\n", target.c_str());
            IcmpCloseHandle(hIcmp);
            printf("\nPress any key to continue...\n");
            getch();
            return;
        }
    }
    
    char sendData[32] = "PingTest";
    char replyBuffer[sizeof(ICMP_ECHO_REPLY) + 32];
    
    int sent = 0, received = 0;
    DWORD totalTime = 0;
    
    for (int i = 0; i < 4; i++) {
        DWORD replySize = sizeof(replyBuffer);
        DWORD dwRetVal = IcmpSendEcho(hIcmp, ipAddress, sendData, sizeof(sendData),
            NULL, replyBuffer, replySize, 3000);
        
        if (dwRetVal > 0) {
            PICMP_ECHO_REPLY pEchoReply = (PICMP_ECHO_REPLY)replyBuffer;
            if (pEchoReply->Status == IP_SUCCESS) {
                printf("Reply from %s: bytes=%d time=%dms TTL=%d\n",
                    inet_ntoa(*(struct in_addr*)&pEchoReply->Address),
                    pEchoReply->DataSize,
                    pEchoReply->RoundTripTime,
                    pEchoReply->Options.Ttl);
                received++;
                totalTime += pEchoReply->RoundTripTime;
            } else {
                printf("Reply from %s: Error code %d\n", target.c_str(), pEchoReply->Status);
            }
        } else {
            printf("Request timed out.\n");
        }
        
        if (i < 3) Sleep(1000);
    }
    
    printf("\n--- Ping statistics ---\n");
    printf("Packets: Sent = 4, Received = %d, Lost = %d (%.0f%% loss)\n",
        received, 4 - received, (4 - received) * 25.0);
    if (received > 0) {
        printf("Approximate round trip times: Average = %dms\n", totalTime / received);
    }
    
    IcmpCloseHandle(hIcmp);
    printf("\nPress any key to continue...\n");
    getch();
}

void cmdNetstat(const string& option) {
    clearScreen();
    setCursor(0, 0);
    printf("=== Network Statistics ===\n\n");
    
    PMIB_TCPTABLE pTcpTable = NULL;
    ULONG size = 0;
    
    GetTcpTable(NULL, &size, TRUE);
    pTcpTable = (PMIB_TCPTABLE)malloc(size);
    
    if (GetTcpTable(pTcpTable, &size, TRUE) == NO_ERROR) {
        printf("Active TCP Connections:\n\n");
        printf("  Proto  Local Address          Foreign Address        State\n");
        
        for (DWORD i = 0; i < pTcpTable->dwNumEntries; i++) {
            MIB_TCPROW row = pTcpTable->table[i];
            
            char localAddr[32], foreignAddr[32];
            sprintf(localAddr, "%s:%d", 
                inet_ntoa(*(struct in_addr*)&row.dwLocalAddr),
                ntohs((u_short)row.dwLocalPort));
            sprintf(foreignAddr, "%s:%d",
                inet_ntoa(*(struct in_addr*)&row.dwRemoteAddr),
                ntohs((u_short)row.dwRemotePort));
            
            const char* stateStr;
            switch (row.dwState) {
                case MIB_TCP_STATE_CLOSED: stateStr = "CLOSED"; break;
                case MIB_TCP_STATE_LISTEN: stateStr = "LISTEN"; break;
                case MIB_TCP_STATE_SYN_SENT: stateStr = "SYN_SENT"; break;
                case MIB_TCP_STATE_SYN_RCVD: stateStr = "SYN_RCVD"; break;
                case MIB_TCP_STATE_ESTAB: stateStr = "ESTABLISHED"; break;
                case MIB_TCP_STATE_FIN_WAIT1: stateStr = "FIN_WAIT1"; break;
                case MIB_TCP_STATE_FIN_WAIT2: stateStr = "FIN_WAIT2"; break;
                case MIB_TCP_STATE_CLOSE_WAIT: stateStr = "CLOSE_WAIT"; break;
                case MIB_TCP_STATE_CLOSING: stateStr = "CLOSING"; break;
                case MIB_TCP_STATE_LAST_ACK: stateStr = "LAST_ACK"; break;
                case MIB_TCP_STATE_TIME_WAIT: stateStr = "TIME_WAIT"; break;
                case MIB_TCP_STATE_DELETE_TCB: stateStr = "DELETE_TCB"; break;
                default: stateStr = "UNKNOWN"; break;
            }
            
            printf("  TCP    %-22s %-22s %s\n", localAddr, foreignAddr, stateStr);
        }
    }
    free(pTcpTable);
    
    PMIB_UDPTABLE pUdpTable = NULL;
    size = 0;
    GetUdpTable(NULL, &size, TRUE);
    pUdpTable = (PMIB_UDPTABLE)malloc(size);
    
    if (GetUdpTable(pUdpTable, &size, TRUE) == NO_ERROR) {
        printf("\nActive UDP Connections:\n\n");
        printf("  Proto  Local Address\n");
        
        for (DWORD i = 0; i < pUdpTable->dwNumEntries; i++) {
            MIB_UDPROW row = pUdpTable->table[i];
            
            char localAddr[32];
            sprintf(localAddr, "%s:%d",
                inet_ntoa(*(struct in_addr*)&row.dwLocalAddr),
                ntohs((u_short)row.dwLocalPort));
            
            printf("  UDP    %-22s\n", localAddr);
        }
    }
    free(pUdpTable);
    
    printf("\nPress any key to continue...\n");
    getch();
}

void cmdDownloadBlocking(AppState* state, const string& url, const string& fileName) {
    clearScreen();
    setCursor(0, 0);
    printf("=== Blocking Download ===\n\n");
    printf("URL: %s\n", url.c_str());
    
    string outputName = fileName;
    if (outputName.empty()) {
        size_t lastSlash = url.rfind('/');
        if (lastSlash != string::npos && lastSlash < url.size() - 1) {
            outputName = url.substr(lastSlash + 1);
            size_t queryPos = outputName.find('?');
            if (queryPos != string::npos) {
                outputName = outputName.substr(0, queryPos);
            }
        }
        if (outputName.empty()) {
            outputName = "download_" + to_string(time(NULL));
        }
    }
    
    string savePath = WStringToString(mergePath(state->currentPath));
    printf("File: %s\n", outputName.c_str());
    printf("Save to: %s\n\n", savePath.c_str());
    
    if (!state->downloadManager) {
        state->downloadManager = DownloadManager::create();
        Config config;
        config.defaultThreadCount = 4;
        config.maxConcurrentDownloads = 1;
        config.defaultSavePath = savePath;
        state->downloadManager->setConfig(config);
        state->downloadManager->start();
    }
    
    string taskId = state->downloadManager->addTask(url, savePath, 4);
    
    if (taskId.empty()) {
        printf("Failed to add download task\n");
        printf("\nPress any key to continue...\n");
        getch();
        return;
    }
    
    printf("Downloading...\n\n");
    
    bool completed = false;
    bool error = false;
    string errorMsg;
    string finalPath;
    
    while (!completed && !error) {
        TaskInfo info = state->downloadManager->getTaskInfo(taskId);
        
        setCursor(0, 8);
        
        int progressWidth = 40;
        int filled = (info.progress * progressWidth) / 100;
        
        printf("[");
        for (int i = 0; i < progressWidth; i++) {
            if (i < filled) printf("=");
            else if (i == filled) printf(">");
            else printf(" ");
        }
        printf("] %d%%\n", info.progress);
        
        string speedStr;
        if (info.speed > 1024 * 1024) {
            speedStr = to_string(info.speed / (1024 * 1024)) + " MB/s";
        } else if (info.speed > 1024) {
            speedStr = to_string(info.speed / 1024) + " KB/s";
        } else {
            speedStr = to_string(info.speed) + " B/s";
        }
        
        string sizeStr;
        if (info.totalSize > 0) {
            sizeStr = to_string(info.downloadedSize / 1024) + "/" + to_string(info.totalSize / 1024) + " KB";
        } else {
            sizeStr = to_string(info.downloadedSize / 1024) + " KB";
        }
        
        printf("Speed: %s | %s\n", speedStr.c_str(), sizeStr.c_str());
        
        if (info.status == Status::Completed) {
            completed = true;
            finalPath = info.savePath + "\\" + info.fileName;
        } else if (info.status == Status::Error) {
            error = true;
            errorMsg = info.errorMessage;
        }
        
        Sleep(100);
    }
    
    printf("\n");
    if (completed) {
        printf("Download completed: %s\n", finalPath.c_str());
        getFiles(state);
    } else {
        printf("Download failed: %s\n", errorMsg.c_str());
    }
    
    printf("\nPress any key to continue...\n");
    getch();
}

void executeCommand(AppState* state) {
    wstring wcmd(state->cmdBuffer);
    string cmd = WStringToString(wcmd);
    vector<string> args;
    wstring wargStr(state->cmdBuffer);
    vector<wstring> wargs = splitString(wargStr);
    for (const auto& wa : wargs) {
        args.push_back(WStringToString(wa));
    }
    
    if (args.empty()) return;
    
    state->statusMsg.clear();
    
    if (args[0] == "exit" || args[0] == "quit") {
        if (state->downloadManager) {
            state->downloadManager->stop();
            DownloadManager::destroy(state->downloadManager);
        }
        if (state->sevenZipArchive) {
            delete state->sevenZipArchive;
            state->sevenZipArchive = nullptr;
        }
        Cleanup();
        exit(0);
    } else if (args[0] == "cd") {
        if (args.size() < 2) {
            setStatus(state, false, "cd", "ARG_COUNT_ERROR");
        } else {
            vector<wstring> newPath;
            wstring targetArg = expandEnvVars(StringToWString(args[1]));
            
            if (isAbsolutePath(targetArg)) {
                newPath = parseAbsolutePath(targetArg);
                if (newPath.empty()) {
                    setStatus(state, false, "cd", args[1], "INVALID_PATH");
                } else {
                    if (pathExists(newPath)) {
                        state->currentPath = newPath;
                        getFiles(state);
                        setStatus(state, true, "cd", args[1]);
                    } else {
                        vector<wstring> validPath = findValidParentPath(newPath);
                        if (validPath.size() < newPath.size()) {
                            state->currentPath = validPath;
                            getFiles(state);
                            setStatus(state, false, "cd", args[1], "PARTIAL_PATH_REDIRECTED");
                        } else if (!isValidDrive(validPath[0])) {
                            state->currentPath = validPath;
                            getFiles(state);
                            setStatus(state, false, "cd", args[1], "DRIVE_REDIRECTED");
                        } else {
                            setStatus(state, false, "cd", args[1], "PATH_NOT_EXIST");
                        }
                    }
                }
            } else {
                newPath = parseRelativePath(state->currentPath, targetArg);
                if (pathExists(newPath)) {
                    state->currentPath = newPath;
                    getFiles(state);
                    setStatus(state, true, "cd", args[1]);
                } else {
                    vector<wstring> validPath = findValidParentPath(newPath);
                    if (validPath.size() < newPath.size()) {
                        state->currentPath = validPath;
                        getFiles(state);
                        setStatus(state, false, "cd", args[1], "PARTIAL_PATH_REDIRECTED");
                    } else if (!isValidDrive(validPath[0])) {
                        state->currentPath = validPath;
                        getFiles(state);
                        setStatus(state, false, "cd", args[1], "DRIVE_REDIRECTED");
                    } else {
                        setStatus(state, false, "cd", args[1], "PATH_NOT_EXIST");
                    }
                }
            }
        }
    } else if (args[0] == "run") {
        if (args.size() < 2) {
            setStatus(state, false, "run", "ARG_COUNT_ERROR");
        } else {
            wstring realName = findFileCaseInsensitive(state->files, StringToWString(args[1]));
            bool isDir = false;
            
            if (realName.empty()) {
                realName = findDirCaseInsensitive(state->dirs, StringToWString(args[1]));
                isDir = !realName.empty();
            }
            
            if (!realName.empty()) {
                string path = WStringToString(mergePath(state->currentPath));
                string fullPath = path + "\\" + WStringToString(realName);
                ShellExecuteA(NULL, "open", fullPath.c_str(), NULL, path.c_str(), SW_SHOWNORMAL);
                setStatus(state, true, "run", WStringToString(realName));
            } else {
                setStatus(state, false, "run", args[1], "FILE_NOT_EXIST");
            }
        }
    } else if (args[0] == "cls" || args[0] == "clear") {
        clearScreen();
        setStatus(state, true, args[0]);
    } else if (args[0] == "pwd") {
        setStatus(state, true, "pwd", WStringToString(mergePath(state->currentPath)));
    } else if (args[0] == "set") {
        if (args.size() < 2) {
            char* env = getenv("");
            setStatus(state, false, "set", "ARG_COUNT_ERROR usage: set VAR=value");
        } else {
            string arg = args[1];
            size_t eqPos = arg.find('=');
            if (eqPos == string::npos) {
                char* envValue = getenv(arg.c_str());
                if (envValue) {
                    setStatus(state, true, "set", arg + "=" + string(envValue));
                } else {
                    setStatus(state, false, "set", arg, "VAR_NOT_DEFINED");
                }
            } else {
                string varName = arg.substr(0, eqPos);
                string varValue = arg.substr(eqPos + 1);
                varValue = WStringToString(expandEnvVars(StringToWString(varValue)));
                if (_putenv_s(varName.c_str(), varValue.c_str()) == 0) {
                    setStatus(state, true, "set", varName + "=" + varValue);
                } else {
                    setStatus(state, false, "set", varName, "SET_FAILED");
                }
            }
        }
    } else if (args[0] == "get") {
        if (args.size() < 2) {
            setStatus(state, false, "get", "ARG_COUNT_ERROR usage: get VAR");
        } else {
            string varName = args[1];
            char* envValue = getenv(varName.c_str());
            if (envValue) {
                setStatus(state, true, "get", varName + "=" + string(envValue));
            } else {
                setStatus(state, false, "get", varName, "VAR_NOT_DEFINED");
            }
        }
    } else if (args[0] == "env") {
        extern char **environ;
        char** env = environ;
        int count = 0;
        string msg = "ENV_VARS:";
        while (*env && count < 20) {
            msg += " ";
            msg += *env;
            env++;
            count++;
        }
        if (*env) {
            msg += " ...";
        }
        setStatus(state, true, "env", msg);
    } else if (args[0] == "copy") {
        if (args.size() < 3) {
            setStatus(state, false, "copy", "ARG_COUNT_ERROR usage: copy <src> <dst>");
        } else {
            string srcPath = WStringToString(resolveFullPath(state, StringToWString(args[1])));
            string dstPath = WStringToString(resolveFullPath(state, StringToWString(args[2])));
            
            DWORD srcAttr = GetFileAttributesA(srcPath.c_str());
            if (srcAttr == INVALID_FILE_ATTRIBUTES) {
                setStatus(state, false, "copy", args[1], "SOURCE_NOT_FOUND");
            } else if (srcAttr & FILE_ATTRIBUTE_DIRECTORY) {
                setStatus(state, false, "copy", args[1], "SOURCE_IS_DIR_USE_XCOPY");
            } else {
                if (CopyFileA(srcPath.c_str(), dstPath.c_str(), FALSE)) {
                    setStatus(state, true, "copy", args[1] + " -> " + args[2]);
                    getFiles(state);
                } else {
                    DWORD err = GetLastError();
                    setStatus(state, false, "copy", args[1], "COPY_FAILED_ERR=" + to_string(err));
                }
            }
        }
    } else if (args[0] == "move") {
        if (args.size() < 3) {
            setStatus(state, false, "move", "ARG_COUNT_ERROR usage: move <src> <dst>");
        } else {
            string srcPath = WStringToString(resolveFullPath(state, StringToWString(args[1])));
            string dstPath = WStringToString(resolveFullPath(state, StringToWString(args[2])));
            
            DWORD srcAttr = GetFileAttributesA(srcPath.c_str());
            if (srcAttr == INVALID_FILE_ATTRIBUTES) {
                setStatus(state, false, "move", args[1], "SOURCE_NOT_FOUND");
            } else {
                if (MoveFileA(srcPath.c_str(), dstPath.c_str())) {
                    setStatus(state, true, "move", args[1] + " -> " + args[2]);
                    getFiles(state);
                } else {
                    DWORD err = GetLastError();
                    setStatus(state, false, "move", args[1], "MOVE_FAILED_ERR=" + to_string(err));
                }
            }
        }
    } else if (args[0] == "mkdir") {
        if (args.size() < 2) {
            setStatus(state, false, "mkdir", "ARG_COUNT_ERROR usage: mkdir <dir>");
        } else {
            string dirPath = WStringToString(resolveFullPath(state, StringToWString(args[1])));
            
            DWORD attr = GetFileAttributesA(dirPath.c_str());
            if (attr != INVALID_FILE_ATTRIBUTES) {
                setStatus(state, false, "mkdir", args[1], "ALREADY_EXISTS");
            } else {
                if (CreateDirectoryA(dirPath.c_str(), NULL)) {
                    setStatus(state, true, "mkdir", args[1]);
                    getFiles(state);
                } else {
                    DWORD err = GetLastError();
                    setStatus(state, false, "mkdir", args[1], "MKDIR_FAILED_ERR=" + to_string(err));
                }
            }
        }
    } else if (args[0] == "rm") {
        if (args.size() < 2) {
            setStatus(state, false, "rm", "ARG_COUNT_ERROR usage: rm <file|dir>");
        } else {
            string targetPath = WStringToString(resolveFullPath(state, StringToWString(args[1])));
            
            DWORD attr = GetFileAttributesA(targetPath.c_str());
            if (attr == INVALID_FILE_ATTRIBUTES) {
                setStatus(state, false, "rm", args[1], "NOT_FOUND");
            } else if (attr & FILE_ATTRIBUTE_DIRECTORY) {
                if (RemoveDirectoryA(targetPath.c_str())) {
                    setStatus(state, true, "rm", args[1]);
                    getFiles(state);
                } else {
                    DWORD err = GetLastError();
                    if (err == ERROR_DIR_NOT_EMPTY) {
                        setStatus(state, false, "rm", args[1], "DIR_NOT_EMPTY");
                    } else {
                        setStatus(state, false, "rm", args[1], "RMDIR_FAILED_ERR=" + to_string(err));
                    }
                }
            } else {
                if (DeleteFileA(targetPath.c_str())) {
                    setStatus(state, true, "rm", args[1]);
                    getFiles(state);
                } else {
                    DWORD err = GetLastError();
                    setStatus(state, false, "rm", args[1], "DELETE_FAILED_ERR=" + to_string(err));
                }
            }
        }
    } else if (args[0] == "attrib") {
        if (args.size() < 2) {
            setStatus(state, false, "attrib", "ARG_COUNT_ERROR usage: attrib [+r|-r] [+h|-h] [+s|-s] [+a|-a] <file>");
        } else {
            string filePath;
            DWORD setAttrs = 0;
            DWORD clearAttrs = 0;
            bool hasFlags = false;
            
            for (size_t i = 1; i < args.size(); i++) {
                if (args[i].size() >= 2 && (args[i][0] == '+' || args[i][0] == '-')) {
                    hasFlags = true;
                    char op = args[i][0];
                    for (size_t j = 1; j < args[i].size(); j++) {
                        char c = tolower(args[i][j]);
                        DWORD attr = 0;
                        if (c == 'r') attr = FILE_ATTRIBUTE_READONLY;
                        else if (c == 'h') attr = FILE_ATTRIBUTE_HIDDEN;
                        else if (c == 's') attr = FILE_ATTRIBUTE_SYSTEM;
                        else if (c == 'a') attr = FILE_ATTRIBUTE_ARCHIVE;
                        
                        if (attr != 0) {
                            if (op == '+') setAttrs |= attr;
                            else clearAttrs |= attr;
                        }
                    }
                } else {
                    filePath = args[i];
                }
            }
            
            if (filePath.empty()) {
                setStatus(state, false, "attrib", "ARG_COUNT_ERROR usage: attrib [flags] <file>");
            } else {
                string fullPath = WStringToString(resolveFullPath(state, StringToWString(filePath)));
                DWORD currentAttr = GetFileAttributesA(fullPath.c_str());
                
                if (currentAttr == INVALID_FILE_ATTRIBUTES) {
                    setStatus(state, false, "attrib", filePath, "NOT_FOUND");
                } else {
                    string attrStr;
                    attrStr += (currentAttr & FILE_ATTRIBUTE_READONLY) ? "R" : "-";
                    attrStr += (currentAttr & FILE_ATTRIBUTE_HIDDEN) ? "H" : "-";
                    attrStr += (currentAttr & FILE_ATTRIBUTE_SYSTEM) ? "S" : "-";
                    attrStr += (currentAttr & FILE_ATTRIBUTE_ARCHIVE) ? "A" : "-";
                    
                    if (hasFlags) {
                        DWORD newAttr = currentAttr;
                        newAttr |= setAttrs;
                        newAttr &= ~clearAttrs;
                        
                        if (SetFileAttributesA(fullPath.c_str(), newAttr)) {
                            string newAttrStr;
                            newAttrStr += (newAttr & FILE_ATTRIBUTE_READONLY) ? "R" : "-";
                            newAttrStr += (newAttr & FILE_ATTRIBUTE_HIDDEN) ? "H" : "-";
                            newAttrStr += (newAttr & FILE_ATTRIBUTE_SYSTEM) ? "S" : "-";
                            newAttrStr += (newAttr & FILE_ATTRIBUTE_ARCHIVE) ? "A" : "-";
                            setStatus(state, true, "attrib", filePath + " " + attrStr + " -> " + newAttrStr);
                            getFiles(state);
                        } else {
                            DWORD err = GetLastError();
                            setStatus(state, false, "attrib", filePath, "SET_ATTR_FAILED_ERR=" + to_string(err));
                        }
                    } else {
                        setStatus(state, true, "attrib", filePath + " " + attrStr);
                    }
                }
            }
        }
    } else if (args[0] == "download") {
        if (args.size() < 2) {
            setStatus(state, false, "download", "ARG_COUNT_ERROR usage: download <url> [filename]");
        } else {
            string url = args[1];
            string fileName;
            
            if (args.size() >= 3) {
                fileName = args[2];
            } else {
                size_t lastSlash = url.rfind('/');
                if (lastSlash != string::npos && lastSlash < url.size() - 1) {
                    fileName = url.substr(lastSlash + 1);
                    size_t queryPos = fileName.find('?');
                    if (queryPos != string::npos) {
                        fileName = fileName.substr(0, queryPos);
                    }
                }
                if (fileName.empty()) {
                    fileName = "download_" + to_string(time(NULL));
                }
            }
            
            string savePath = WStringToString(mergePath(state->currentPath));
            
            if (!state->downloadManager) {
                state->downloadManager = DownloadManager::create();
                Config config;
                config.defaultThreadCount = 4;
                config.maxConcurrentDownloads = 3;
                config.defaultSavePath = savePath;
                state->downloadManager->setConfig(config);
                
                state->downloadManager->setProgressCallback([state](const string& taskId, int progress, 
                    uint64_t downloaded, uint64_t total, uint64_t speed) mutable {
                    lock_guard<mutex> lock(state->downloadMutex);
                    state->downloadProgress = progress;
                    state->downloadDownloaded = downloaded;
                    state->downloadTotal = total;
                    state->downloadSpeed = speed;
                    state->currentDownloadId = taskId;
                });
                
                state->downloadManager->setCompleteCallback([state](const string& taskId, const string& filePath) mutable {
                    lock_guard<mutex> lock(state->downloadMutex);
                    state->showDownloadProgress = false;
                    state->statusMsg = StringToWString("SUCCESS download: " + filePath);
                    
                    auto info = state->downloadManager->getTaskInfo(taskId);
                    state->downloadHistory.push_back(info);
                    
                    getFiles(&*state);
                });
                
                state->downloadManager->setErrorCallback([state](const string& taskId, const string& errorMsg, bool willRetry) mutable {
                    lock_guard<mutex> lock(state->downloadMutex);
                    if (!willRetry) {
                        state->showDownloadProgress = false;
                        state->statusMsg = StringToWString("ERROR download: " + errorMsg);
                        
                        auto info = state->downloadManager->getTaskInfo(taskId);
                        state->downloadHistory.push_back(info);
                    }
                });
                
                state->downloadManager->start();
            }
            
            string taskId = state->downloadManager->addTask(url, savePath, 4);
            
            if (!taskId.empty()) {
                lock_guard<mutex> lock(state->downloadMutex);
                state->downloadTaskIds.push_back(taskId);
                state->currentDownloadId = taskId;
                state->downloadFileName = fileName;
                state->downloadProgress = 0;
                state->downloadSpeed = 0;
                state->downloadTotal = 0;
                state->downloadDownloaded = 0;
                state->showDownloadProgress = true;
                setStatus(state, true, "download", url + " -> " + fileName);
            } else {
                setStatus(state, false, "download", url, "FAILED_TO_ADD_TASK");
            }
        }
    } else if (args[0] == "dl") {
        if (args.size() < 2) {
            setStatus(state, false, "dl", "ARG_COUNT_ERROR usage: dl <url> [filename]");
        } else {
            string url = args[1];
            string fileName = (args.size() >= 3) ? args[2] : "";
            cmdDownloadBlocking(state, url, fileName);
        }
    } else if (args[0] == "ipconfig") {
        cmdIpconfig();
    } else if (args[0] == "ping") {
        if (args.size() < 2) {
            setStatus(state, false, "ping", "ARG_COUNT_ERROR usage: ping <host>");
        } else {
            cmdPing(args[1]);
        }
    } else if (args[0] == "netstat") {
        cmdNetstat(args.size() > 1 ? args[1] : "");
    } else if (args[0] == "unlock") {
        if (args.size() < 2) {
            setStatus(state, false, "unlock", "ARG_COUNT_ERROR usage: unlock <file|dir>");
        } else {
            string filePath = WStringToString(resolveFullPath(state, StringToWString(args[1])));
            wstring wPath(filePath.begin(), filePath.end());
            
            clearScreen();
            setCursor(0, 0);
            
            if (is_directory(wPath.c_str())) {
                printf("=== Unlock Directory ===\n\n");
                printf("Directory: %s\n\n", args[1].c_str());
            } else {
                printf("=== Unlock File ===\n\n");
                printf("File: %s\n\n", args[1].c_str());
            }
            
            if (unlock(wPath.c_str())) {
                printf("Unlock completed successfully.\n");
                setStatus(state, true, "unlock", args[1]);
                getFiles(state);
            } else {
                printf("Failed to unlock.\n");
                setStatus(state, false, "unlock", args[1], "FAILED");
            }
            
            printf("\nPress any key to continue...\n");
            getch();
        }
    } else if (args[0] == "smash") {
        if (args.size() < 2) {
            setStatus(state, false, "smash", "ARG_COUNT_ERROR usage: smash <file|dir>");
        } else {
            string filePath = WStringToString(resolveFullPath(state, StringToWString(args[1])));
            wstring wPath(filePath.begin(), filePath.end());
            
            clearScreen();
            setCursor(0, 0);
            
            if (is_directory(wPath.c_str())) {
                printf("=== Force Delete Directory ===\n\n");
                printf("Directory: %s\n\n", args[1].c_str());
            } else {
                printf("=== Force Delete File ===\n\n");
                printf("File: %s\n\n", args[1].c_str());
            }
            
            if (smash(wPath.c_str())) {
                printf("Delete completed successfully.\n");
                setStatus(state, true, "smash", args[1]);
                getFiles(state);
            } else {
                printf("Failed to delete.\n");
                setStatus(state, false, "smash", args[1], "FAILED");
            }
            
            printf("\nPress any key to continue...\n");
            getch();
        }
    } else if (args[0] == "7z") {
        if (args.size() < 3) {
            setStatus(state, false, "7z", "Usage: 7z <a|x> <archive> [files...]");
        } else {
            string action = args[1];
            string archivePath = WStringToString(resolveFullPath(state, StringToWString(args[2])));
            
            if (!state->sevenZipArchive) {
                char dllPath[MAX_PATH];
                GetModuleFileNameA(NULL, dllPath, MAX_PATH);
                string exePath(dllPath);
                size_t lastSlash = exePath.rfind('\\');
                if (lastSlash != string::npos) {
                    exePath = exePath.substr(0, lastSlash);
                }
                string sevenZipDll = exePath + "\\7z.dll";
                state->sevenZipArchive = new SevenZip::SevenZipArchive(sevenZipDll);
                if (!state->sevenZipArchive->Initialize()) {
                    setStatus(state, false, "7z", "Failed to load 7z.dll");
                    delete state->sevenZipArchive;
                    state->sevenZipArchive = nullptr;
                    return;
                }
            }
            
            if (action == "a" || action == "add") {
                vector<string> filesToCompress;
                for (size_t i = 3; i < args.size(); i++) {
                    filesToCompress.push_back(WStringToString(resolveFullPath(state, StringToWString(args[i]))));
                }
                
                if (filesToCompress.empty()) {
                    setStatus(state, false, "7z", "No files specified");
                } else {
                    SevenZip::CompressionOptions options;
                    options.level = SevenZip::CompressionLevel::Normal;
                    
                    clearScreen();
                    setCursor(0, 0);
                    printf("=== Compressing ===\n\n");
                    printf("Archive: %s\n", archivePath.c_str());
                    printf("Files: %zu\n\n", filesToCompress.size());
                    
                    bool success = state->sevenZipArchive->CompressFiles(archivePath, filesToCompress, options);
                    
                    if (success) {
                        printf("Compression completed successfully.\n");
                        setStatus(state, true, "7z", archivePath);
                        getFiles(state);
                    } else {
                        printf("Compression failed.\n");
                        setStatus(state, false, "7z", "COMPRESSION_FAILED");
                    }
                    
                    printf("\nPress any key to continue...\n");
                    getch();
                }
            } else if (action == "x" || action == "extract") {
                string outputDir = args.size() > 3 ? WStringToString(resolveFullPath(state, StringToWString(args[3]))) : WStringToString(mergePath(state->currentPath));
                
                clearScreen();
                setCursor(0, 0);
                printf("=== Extracting ===\n\n");
                printf("Archive: %s\n", archivePath.c_str());
                printf("Output: %s\n\n", outputDir.c_str());
                
                SevenZip::ExtractOptions options;
                options.outputDir = outputDir;
                
                bool success = state->sevenZipArchive->ExtractArchive(archivePath, options);
                
                if (success) {
                    printf("Extraction completed successfully.\n");
                    setStatus(state, true, "7z extract", archivePath);
                    getFiles(state);
                } else {
                    printf("Extraction failed.\n");
                    setStatus(state, false, "7z", "EXTRACTION_FAILED");
                }
                
                printf("\nPress any key to continue...\n");
                getch();
            } else {
                setStatus(state, false, "7z", "Unknown action: " + action + " (use 'a' or 'x')");
            }
        }
    } else if (args[0] == "7zlist") {
        if (args.size() < 2) {
            setStatus(state, false, "7zlist", "Usage: 7zlist <archive>");
        } else {
            string archivePath = WStringToString(resolveFullPath(state, StringToWString(args[1])));
            
            if (!state->sevenZipArchive) {
                char dllPath[MAX_PATH];
                GetModuleFileNameA(NULL, dllPath, MAX_PATH);
                string exePath(dllPath);
                size_t lastSlash = exePath.rfind('\\');
                if (lastSlash != string::npos) {
                    exePath = exePath.substr(0, lastSlash);
                }
                string sevenZipDll = exePath + "\\7z.dll";
                state->sevenZipArchive = new SevenZip::SevenZipArchive(sevenZipDll);
                if (!state->sevenZipArchive->Initialize()) {
                    setStatus(state, false, "7zlist", "Failed to load 7z.dll");
                    delete state->sevenZipArchive;
                    state->sevenZipArchive = nullptr;
                    return;
                }
            }
            
            clearScreen();
            setCursor(0, 0);
            printf("=== Archive Contents ===\n\n");
            printf("Archive: %s\n\n", archivePath.c_str());
            
            SevenZip::ArchiveInfo info;
            if (state->sevenZipArchive->ListArchive(archivePath, info)) {
                printf("Files: %u, Directories: %u\n", info.fileCount, info.directoryCount);
                printf("Uncompressed: %llu bytes\n", info.uncompressedSize);
                printf("Compressed: %llu bytes\n\n", info.compressedSize);
                
                printf("Contents:\n");
                for (const auto& file : info.files) {
                    if (file.isDirectory) {
                        printf("  [DIR]  %s\n", file.path.c_str());
                    } else {
                        printf("  [%llu] %s\n", file.size, file.path.c_str());
                    }
                }
                setStatus(state, true, "7zlist", archivePath);
            } else {
                printf("Failed to list archive.\n");
                setStatus(state, false, "7zlist", "LIST_FAILED");
            }
            
            printf("\nPress any key to continue...\n");
            getch();
        }
    } else if (args[0] == "7zextract") {
        if (args.size() < 2) {
            setStatus(state, false, "7zextract", "Usage: 7zextract <archive> [output_dir]");
        } else {
            string archivePath = WStringToString(resolveFullPath(state, StringToWString(args[1])));
            string outputDir = args.size() > 2 ? WStringToString(resolveFullPath(state, StringToWString(args[2]))) : WStringToString(mergePath(state->currentPath));
            
            if (!state->sevenZipArchive) {
                char dllPath[MAX_PATH];
                GetModuleFileNameA(NULL, dllPath, MAX_PATH);
                string exePath(dllPath);
                size_t lastSlash = exePath.rfind('\\');
                if (lastSlash != string::npos) {
                    exePath = exePath.substr(0, lastSlash);
                }
                string sevenZipDll = exePath + "\\7z.dll";
                state->sevenZipArchive = new SevenZip::SevenZipArchive(sevenZipDll);
                if (!state->sevenZipArchive->Initialize()) {
                    setStatus(state, false, "7zextract", "Failed to load 7z.dll");
                    delete state->sevenZipArchive;
                    state->sevenZipArchive = nullptr;
                    return;
                }
            }
            
            clearScreen();
            setCursor(0, 0);
            printf("=== Extracting Archive ===\n\n");
            printf("Archive: %s\n", archivePath.c_str());
            printf("Output: %s\n\n", outputDir.c_str());
            
            SevenZip::ExtractOptions options;
            options.outputDir = outputDir;
            
            bool success = state->sevenZipArchive->ExtractArchive(archivePath, options);
            
            if (success) {
                printf("Extraction completed successfully.\n");
                setStatus(state, true, "7zextract", archivePath);
                getFiles(state);
            } else {
                printf("Extraction failed.\n");
                setStatus(state, false, "7zextract", "EXTRACTION_FAILED");
            }
            
            printf("\nPress any key to continue...\n");
            getch();
        }
    } else if (args[0] == "dlstatus") {
        lock_guard<mutex> lock(state->downloadMutex);
        if (!state->downloadManager || state->downloadTaskIds.empty()) {
            setStatus(state, true, "dlstatus", "No active downloads");
        } else {
            string statusStr;
            auto tasks = state->downloadManager->getAllTasks();
            for (const auto& info : tasks) {
                string statusName;
                switch (info.status) {
                    case Status::Pending: statusName = "Pending"; break;
                    case Status::Downloading: statusName = "Downloading"; break;
                    case Status::Paused: statusName = "Paused"; break;
                    case Status::Completed: statusName = "Completed"; break;
                    case Status::Error: statusName = "Error"; break;
                    case Status::Cancelled: statusName = "Cancelled"; break;
                }
                statusStr += info.fileName + " [" + statusName + "] " + to_string(info.progress) + "% ";
                if (info.speed > 0) {
                    statusStr += to_string(info.speed / 1024) + "KB/s";
                }
                statusStr += "; ";
            }
            setStatus(state, true, "dlstatus", statusStr);
        }
    } else if (args[0] == "dlhistory") {
        state->showDownloadHistory = true;
        state->downloadHistoryIndex = 0;
    } else if (args[0] == "help" || args[0] == "?") {
        state->showHelp = true;
        state->helpScrollOffset = 0;
    } else if (args[0] == "history") {
        state->showHistory = true;
        state->historyScrollOffset = 0;
    } else if (args[0] == "grep" || args[0] == "findstr") {
        if (args.size() < 3) {
            setStatus(state, false, "grep", "Usage: grep <pattern> <file> [-i]");
        } else {
            string pattern = args[1];
            string filePath = WStringToString(resolveFullPath(state, StringToWString(args[2])));
            bool ignoreCase = false;
            for (size_t i = 3; i < args.size(); i++) {
                if (args[i] == "-i" || args[i] == "/i") ignoreCase = true;
            }
            
            clearScreen();
            setCursor(0, 0);
            printf("=== Grep: %s in %s ===\n\n", pattern.c_str(), args[2].c_str());
            
            ifstream file(filePath);
            if (!file.is_open()) {
                printf("Cannot open file: %s\n", filePath.c_str());
                setStatus(state, false, "grep", args[2], "FILE_NOT_FOUND");
            } else {
                string line;
                int lineNum = 0;
                int matchCount = 0;
                string patternLower = pattern;
                if (ignoreCase) {
                    transform(patternLower.begin(), patternLower.end(), patternLower.begin(), ::tolower);
                }
                
                while (getline(file, line)) {
                    lineNum++;
                    string searchLine = ignoreCase ? line : "";
                    if (ignoreCase) {
                        transform(searchLine.begin(), searchLine.end(), searchLine.begin(), ::tolower);
                    }
                    
                    bool found = ignoreCase ? 
                        (searchLine.find(patternLower) != string::npos) :
                        (line.find(pattern) != string::npos);
                    
                    if (found) {
                        matchCount++;
                        printf("%d: %s\n", lineNum, line.c_str());
                        if (matchCount >= 100) {
                            printf("\n... (truncated, max 100 matches)\n");
                            break;
                        }
                    }
                }
                
                printf("\nFound %d match(es)\n", matchCount);
                setStatus(state, true, "grep", pattern + " in " + args[2] + " (" + to_string(matchCount) + " matches)");
            }
            
            printf("\nPress any key to continue...\n");
            getch();
        }
    } else if (args[0] == "ps" || args[0] == "tasklist") {
        clearScreen();
        setCursor(0, 0);
        printf("=== Process List ===\n\n");
        printf("%-8s %-40s %-10s\n", "PID", "Name", "Memory(MB)");
        printf("%s\n", string(60, '-').c_str());
        
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE) {
            printf("Failed to create process snapshot\n");
        } else {
            PROCESSENTRY32 pe32;
            pe32.dwSize = sizeof(PROCESSENTRY32);
            
            if (Process32First(hSnapshot, &pe32)) {
                int count = 0;
                do {
                    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe32.th32ProcessID);
                    DWORD memMB = 0;
                    if (hProcess) {
                        PROCESS_MEMORY_COUNTERS pmc;
                        if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
                            memMB = pmc.WorkingSetSize / (1024 * 1024);
                        }
                        CloseHandle(hProcess);
                    }
                    
                    printf("%-8lu %-40s %-10lu\n", pe32.th32ProcessID, pe32.szExeFile, memMB);
                    count++;
                    if (count >= 50) {
                        printf("... (showing first 50 processes)\n");
                        break;
                    }
                } while (Process32Next(hSnapshot, &pe32));
            }
            CloseHandle(hSnapshot);
        }
        
        setStatus(state, true, "ps");
        printf("\nPress any key to continue...\n");
        getch();
    } else if (args[0] == "kill" || args[0] == "taskkill") {
        if (args.size() < 2) {
            setStatus(state, false, "kill", "Usage: kill <pid|name> [-f]");
        } else {
            bool force = false;
            string target;
            for (size_t i = 1; i < args.size(); i++) {
                if (args[i] == "-f" || args[i] == "/f") force = true;
                else target = args[i];
            }
            
            if (target.empty()) {
                setStatus(state, false, "kill", "No target specified");
            } else {
                bool success = false;
                bool isPid = true;
                for (char c : target) {
                    if (!isdigit(c)) { isPid = false; break; }
                }
                
                if (isPid) {
                    DWORD pid = stoi(target);
                    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
                    if (hProcess) {
                        success = TerminateProcess(hProcess, 0);
                        CloseHandle(hProcess);
                    }
                } else {
                    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
                    if (hSnapshot != INVALID_HANDLE_VALUE) {
                        PROCESSENTRY32 pe32;
                        pe32.dwSize = sizeof(PROCESSENTRY32);
                        if (Process32First(hSnapshot, &pe32)) {
                            do {
                                if (_stricmp(pe32.szExeFile, target.c_str()) == 0) {
                                    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);
                                    if (hProcess) {
                                        if (TerminateProcess(hProcess, 0)) success = true;
                                        CloseHandle(hProcess);
                                    }
                                }
                            } while (Process32Next(hSnapshot, &pe32));
                        }
                        CloseHandle(hSnapshot);
                    }
                }
                
                if (success) {
                    setStatus(state, true, "kill", target);
                } else {
                    setStatus(state, false, "kill", target, "FAILED");
                }
            }
        }
    } else if (args[0] == "clip") {
        if (args.size() < 2) {
            setStatus(state, false, "clip", "Usage: clip <text> or clip <file> -f");
        } else {
            string text;
            bool fromFile = false;
            if (args.size() >= 3 && (args[1] == "-f" || args[1] == "/f")) {
                fromFile = true;
                string filePath = WStringToString(resolveFullPath(state, StringToWString(args[2])));
                ifstream file(filePath);
                if (file.is_open()) {
                    string line;
                    while (getline(file, line)) {
                        if (!text.empty()) text += "\n";
                        text += line;
                    }
                } else {
                    setStatus(state, false, "clip", args[2], "FILE_NOT_FOUND");
                    return;
                }
            } else {
                for (size_t i = 1; i < args.size(); i++) {
                    if (i > 1) text += " ";
                    text += args[i];
                }
            }
            
            if (OpenClipboard(NULL)) {
                EmptyClipboard();
                HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
                if (hMem) {
                    memcpy(GlobalLock(hMem), text.c_str(), text.size() + 1);
                    GlobalUnlock(hMem);
                    SetClipboardData(CF_TEXT, hMem);
                    setStatus(state, true, "clip", fromFile ? "file content" : text.substr(0, 50));
                } else {
                    setStatus(state, false, "clip", "ALLOC_FAILED");
                }
                CloseClipboard();
            } else {
                setStatus(state, false, "clip", "CLIPBOARD_OPEN_FAILED");
            }
        }
    } else if (args[0] == "paste") {
        if (OpenClipboard(NULL)) {
            HANDLE hData = GetClipboardData(CF_TEXT);
            if (hData) {
                char* text = (char*)GlobalLock(hData);
                if (text) {
                    string clipText(text);
                    GlobalUnlock(hData);
                    
                    if (args.size() >= 2) {
                        string filePath = WStringToString(resolveFullPath(state, StringToWString(args[1])));
                        ofstream file(filePath);
                        if (file.is_open()) {
                            file << clipText;
                            setStatus(state, true, "paste", "saved to " + args[1]);
                            getFiles(state);
                        } else {
                            setStatus(state, false, "paste", args[1], "FILE_WRITE_FAILED");
                        }
                    } else {
                        setStatus(state, true, "paste", clipText.substr(0, 100) + (clipText.size() > 100 ? "..." : ""));
                    }
                }
            } else {
                setStatus(state, false, "paste", "NO_TEXT_IN_CLIPBOARD");
            }
            CloseClipboard();
        } else {
            setStatus(state, false, "paste", "CLIPBOARD_OPEN_FAILED");
        }
    } else if (args[0] == "hash" || args[0] == "md5" || args[0] == "sha1" || args[0] == "sha256") {
        if (args.size() < 2) {
            setStatus(state, false, "hash", "Usage: hash <file> [md5|sha1|sha256]");
        } else {
            string filePath = WStringToString(resolveFullPath(state, StringToWString(args[1])));
            string algo = "md5";
            if (args.size() >= 3) algo = args[2];
            if (args[0] == "sha1") algo = "sha1";
            if (args[0] == "sha256") algo = "sha256";
            
            transform(algo.begin(), algo.end(), algo.begin(), ::tolower);
            
            clearScreen();
            setCursor(0, 0);
            printf("=== Hash Calculator ===\n\n");
            printf("File: %s\n", args[1].c_str());
            printf("Algorithm: %s\n\n", algo.c_str());
            
            ALG_ID algId = CALG_MD5;
            if (algo == "sha1") algId = CALG_SHA1;
            else if (algo == "sha256") algId = CALG_SHA_256;
            
            HCRYPTPROV hProv = 0;
            HCRYPTHASH hHash = 0;
            
            if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
                printf("CryptAcquireContext failed\n");
                setStatus(state, false, "hash", "CRYPT_INIT_FAILED");
            } else {
                if (!CryptCreateHash(hProv, algId, 0, 0, &hHash)) {
                    printf("Algorithm not supported: %s\n", algo.c_str());
                    setStatus(state, false, "hash", "ALGO_NOT_SUPPORTED");
                } else {
                    HANDLE hFile = CreateFileA(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
                    if (hFile == INVALID_HANDLE_VALUE) {
                        printf("Cannot open file\n");
                        setStatus(state, false, "hash", args[1], "FILE_NOT_FOUND");
                    } else {
                        BYTE buffer[65536];
                        DWORD bytesRead;
                        LARGE_INTEGER fileSize;
                        GetFileSizeEx(hFile, &fileSize);
                        uint64_t totalRead = 0;
                        
                        while (ReadFile(hFile, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
                            CryptHashData(hHash, buffer, bytesRead, 0);
                            totalRead += bytesRead;
                            printf("\rHashing: %llu / %llu bytes", totalRead, fileSize.QuadPart);
                        }
                        CloseHandle(hFile);
                        
                        DWORD hashLen = 0;
                        DWORD hashLenSize = sizeof(DWORD);
                        CryptGetHashParam(hHash, HP_HASHSIZE, (BYTE*)&hashLen, &hashLenSize, 0);
                        
                        vector<BYTE> hashData(hashLen);
                        CryptGetHashParam(hHash, HP_HASHVAL, hashData.data(), &hashLen, 0);
                        
                        printf("\n\n%s: ", algo.c_str());
                        for (DWORD i = 0; i < hashLen; i++) {
                            printf("%02x", hashData[i]);
                        }
                        printf("\n");
                        
                        setStatus(state, true, "hash", args[1] + " " + algo);
                    }
                    CryptDestroyHash(hHash);
                }
                CryptReleaseContext(hProv, 0);
            }
            
            printf("\nPress any key to continue...\n");
            getch();
        }
    } else if (args[0] == "sysinfo" || args[0] == "systeminfo") {
        clearScreen();
        setCursor(0, 0);
        printf("=== System Information ===\n\n");
        
        char computerName[MAX_PATH];
        DWORD size = MAX_PATH;
        GetComputerNameA(computerName, &size);
        printf("Computer Name: %s\n", computerName);
        
        char userName[MAX_PATH];
        size = MAX_PATH;
        GetUserNameA(userName, &size);
        printf("User Name: %s\n", userName);
        
        OSVERSIONINFOEXA osvi;
        ZeroMemory(&osvi, sizeof(osvi));
        osvi.dwOSVersionInfoSize = sizeof(osvi);
        
        MEMORYSTATUSEX memStatus;
        memStatus.dwLength = sizeof(memStatus);
        GlobalMemoryStatusEx(&memStatus);
        printf("\nMemory:\n");
        printf("  Total Physical: %llu MB\n", memStatus.ullTotalPhys / (1024 * 1024));
        printf("  Available: %llu MB\n", memStatus.ullAvailPhys / (1024 * 1024));
        printf("  Memory Load: %lu%%\n", memStatus.dwMemoryLoad);
        
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        printf("\nCPU:\n");
        printf("  Processors: %lu\n", sysInfo.dwNumberOfProcessors);
        printf("  Architecture: ");
        switch (sysInfo.wProcessorArchitecture) {
            case PROCESSOR_ARCHITECTURE_AMD64: printf("x64\n"); break;
            case PROCESSOR_ARCHITECTURE_INTEL: printf("x86\n"); break;
            case PROCESSOR_ARCHITECTURE_ARM64: printf("ARM64\n"); break;
            case PROCESSOR_ARCHITECTURE_ARM: printf("ARM\n"); break;
            default: printf("Unknown\n");
        }
        
        printf("\nDrives:\n");
        DWORD drives = GetLogicalDrives();
        for (char d = 'A'; d <= 'Z'; d++) {
            if (drives & (1 << (d - 'A'))) {
                string root = string(1, d) + ":\\";
                ULARGE_INTEGER freeBytes, totalBytes, freeBytesAvailable;
                if (GetDiskFreeSpaceExA(root.c_str(), &freeBytesAvailable, &totalBytes, &freeBytes)) {
                    printf("  %s Total: %llu GB, Free: %llu GB\n", 
                        root.c_str(),
                        totalBytes.QuadPart / (1024 * 1024 * 1024),
                        freeBytes.QuadPart / (1024 * 1024 * 1024));
                }
            }
        }
        
        setStatus(state, true, "sysinfo");
        printf("\nPress any key to continue...\n");
        getch();
    } else if (args[0] == "du" || args[0] == "dirsize") {
        string targetPath = WStringToString(mergePath(state->currentPath));
        if (args.size() >= 2) {
            targetPath = WStringToString(resolveFullPath(state, StringToWString(args[1])));
        }
        
        clearScreen();
        setCursor(0, 0);
        printf("=== Directory Size Analysis ===\n\n");
        printf("Path: %s\n\n", targetPath.c_str());
        
        uint64_t totalSize = 0;
        int fileCount = 0;
        int dirCount = 0;
        
        vector<string> dirs;
        dirs.push_back(targetPath);
        
        while (!dirs.empty()) {
            string currentDir = dirs.back();
            dirs.pop_back();
            
            WIN32_FIND_DATAA findData;
            string searchPath = currentDir + "\\*";
            
            HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
            if (hFind != INVALID_HANDLE_VALUE) {
                do {
                    if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) continue;
                    
                    string fullPath = currentDir + "\\" + findData.cFileName;
                    
                    if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                        dirs.push_back(fullPath);
                        dirCount++;
                    } else {
                        ULARGE_INTEGER fileSize;
                        fileSize.LowPart = findData.nFileSizeLow;
                        fileSize.HighPart = findData.nFileSizeHigh;
                        totalSize += fileSize.QuadPart;
                        fileCount++;
                    }
                    
                    if (fileCount % 1000 == 0) {
                        printf("\rScanning... Files: %d, Dirs: %d", fileCount, dirCount);
                    }
                } while (FindNextFileA(hFind, &findData));
                FindClose(hFind);
            }
        }
        
        printf("\rTotal: %llu bytes (%.2f MB / %.2f GB)\n", totalSize, 
            totalSize / (1024.0 * 1024), totalSize / (1024.0 * 1024 * 1024));
        printf("Files: %d, Directories: %d\n", fileCount, dirCount);
        
        setStatus(state, true, "du", to_string(totalSize / 1024) + " KB");
        printf("\nPress any key to continue...\n");
        getch();
    } else if (args[0] == "df" || args[0] == "diskfree") {
        clearScreen();
        setCursor(0, 0);
        printf("=== Disk Space ===\n\n");
        printf("%-5s %-15s %-15s %-10s %-20s\n", "Drive", "Total(GB)", "Free(GB)", "Used%", "Type");
        printf("%s\n", string(70, '-').c_str());
        
        DWORD drives = GetLogicalDrives();
        for (char d = 'A'; d <= 'Z'; d++) {
            if (drives & (1 << (d - 'A'))) {
                string root = string(1, d) + ":\\";
                ULARGE_INTEGER freeBytes, totalBytes, freeBytesAvailable;
                if (GetDiskFreeSpaceExA(root.c_str(), &freeBytesAvailable, &totalBytes, &freeBytes)) {
                    double totalGB = totalBytes.QuadPart / (1024.0 * 1024 * 1024);
                    double freeGB = freeBytes.QuadPart / (1024.0 * 1024 * 1024);
                    double usedPercent = (1.0 - (double)freeBytes.QuadPart / totalBytes.QuadPart) * 100;
                    
                    string typeStr;
                    switch (GetDriveTypeA(root.c_str())) {
                        case DRIVE_FIXED: typeStr = "Fixed"; break;
                        case DRIVE_REMOVABLE: typeStr = "Removable"; break;
                        case DRIVE_CDROM: typeStr = "CD-ROM"; break;
                        case DRIVE_REMOTE: typeStr = "Network"; break;
                        default: typeStr = "Unknown";
                    }
                    
                    printf("%-5s %-15.2f %-15.2f %-9.1f%% %-20s\n", 
                        root.c_str(), totalGB, freeGB, usedPercent, typeStr.c_str());
                }
            }
        }
        
        setStatus(state, true, "df");
        printf("\nPress any key to continue...\n");
        getch();
    } else if (args[0] == "curl" || args[0] == "http" || args[0] == "wget") {
        if (args.size() < 2) {
            setStatus(state, false, "curl", "Usage: curl <url> [-o output] [-X GET|POST] [-d data]");
        } else {
            string url = args[1];
            string outputFile;
            string method = "GET";
            string data;
            bool showHeaders = false;
            
            for (size_t i = 2; i < args.size(); i++) {
                if (args[i] == "-o" && i + 1 < args.size()) {
                    outputFile = args[++i];
                } else if (args[i] == "-X" && i + 1 < args.size()) {
                    method = args[++i];
                } else if (args[i] == "-d" && i + 1 < args.size()) {
                    data = args[++i];
                } else if (args[i] == "-i") {
                    showHeaders = true;
                }
            }
            
            clearScreen();
            setCursor(0, 0);
            printf("=== HTTP Request ===\n\n");
            printf("URL: %s\n", url.c_str());
            printf("Method: %s\n\n", method.c_str());
            
            wstring urlW(url.begin(), url.end());
            URL_COMPONENTS urlComp;
            ZeroMemory(&urlComp, sizeof(urlComp));
            urlComp.dwStructSize = sizeof(urlComp);
            
            wchar_t hostName[256] = {0};
            wchar_t urlPath[2048] = {0};
            urlComp.lpszHostName = hostName;
            urlComp.dwHostNameLength = 256;
            urlComp.lpszUrlPath = urlPath;
            urlComp.dwUrlPathLength = 2048;
            
            WinHttpCrackUrl(urlW.c_str(), 0, 0, &urlComp);
            
            bool isHttps = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);
            int port = urlComp.nPort;
            
            HINTERNET hSession = WinHttpOpen(L"DLCore/1.1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
            if (!hSession) {
                printf("WinHttpOpen failed: %lu\n", GetLastError());
                setStatus(state, false, "curl", "SESSION_FAILED");
            } else {
                HINTERNET hConnect = WinHttpConnect(hSession, hostName, port, 0);
                if (!hConnect) {
                    printf("WinHttpConnect failed: %lu\n", GetLastError());
                    setStatus(state, false, "curl", "CONNECT_FAILED");
                } else {
                    wstring methodW(method.begin(), method.end());
                    DWORD flags = isHttps ? WINHTTP_FLAG_SECURE : 0;
                    HINTERNET hRequest = WinHttpOpenRequest(hConnect, methodW.c_str(), urlPath, NULL, NULL, NULL, flags);
                    
                    if (!hRequest) {
                        printf("WinHttpOpenRequest failed: %lu\n", GetLastError());
                        setStatus(state, false, "curl", "REQUEST_FAILED");
                    } else {
                        DWORD timeout = 30000;
                        WinHttpSetOption(hRequest, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
                        WinHttpSetOption(hRequest, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
                        
                        if (isHttps) {
                            DWORD secFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
                            WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));
                        }
                        
                        const char* pData = data.empty() ? NULL : data.c_str();
                        DWORD dataLen = data.empty() ? 0 : (DWORD)data.length();
                        
                        if (!WinHttpSendRequest(hRequest, NULL, 0, (LPVOID)pData, dataLen, dataLen, 0)) {
                            printf("WinHttpSendRequest failed: %lu\n", GetLastError());
                            setStatus(state, false, "curl", "SEND_FAILED");
                        } else {
                            if (!WinHttpReceiveResponse(hRequest, NULL)) {
                                printf("WinHttpReceiveResponse failed: %lu\n", GetLastError());
                                setStatus(state, false, "curl", "RECEIVE_FAILED");
                            } else {
                                DWORD statusCode = 0;
                                DWORD statusSize = sizeof(statusCode);
                                WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &statusCode, &statusSize, NULL);
                                
                                printf("Status: %lu\n", statusCode);
                                
                                if (showHeaders) {
                                    wchar_t headers[4096];
                                    DWORD headersSize = sizeof(headers);
                                    if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF, NULL, headers, &headersSize, NULL)) {
                                        wprintf(L"Headers:\n%s\n", headers);
                                    }
                                }
                                
                                DWORD contentLength = 0;
                                DWORD clSize = sizeof(contentLength);
                                WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, NULL, &contentLength, &clSize, NULL);
                                
                                printf("Content-Length: %lu bytes\n\n", contentLength);
                                
                                FILE* outFile = NULL;
                                if (!outputFile.empty()) {
                                    string outPath = WStringToString(resolveFullPath(state, StringToWString(outputFile)));
                                    outFile = fopen(outPath.c_str(), "wb");
                                    if (!outFile) {
                                        printf("Cannot create output file: %s\n", outPath.c_str());
                                    }
                                }
                                
                                BYTE buffer[8192];
                                DWORD bytesRead;
                                uint64_t totalRead = 0;
                                
                                while (WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
                                    if (outFile) {
                                        fwrite(buffer, 1, bytesRead, outFile);
                                    } else {
                                        fwrite(buffer, 1, bytesRead, stdout);
                                    }
                                    totalRead += bytesRead;
                                }
                                
                                if (outFile) {
                                    fclose(outFile);
                                    printf("\n\nSaved to: %s (%llu bytes)\n", outputFile.c_str(), totalRead);
                                    getFiles(state);
                                }
                                
                                setStatus(state, true, "curl", url + " HTTP " + to_string(statusCode));
                            }
                        }
                        WinHttpCloseHandle(hRequest);
                    }
                    WinHttpCloseHandle(hConnect);
                }
                WinHttpCloseHandle(hSession);
            }
            
            printf("\nPress any key to continue...\n");
            getch();
        }
    } else if (args[0] == "dig" || args[0] == "nslookup") {
        if (args.size() < 2) {
            setStatus(state, false, "dig", "Usage: dig <hostname> [type]");
        } else {
            string hostname = args[1];
            string type = "A";
            if (args.size() >= 3) type = args[2];
            
            transform(type.begin(), type.end(), type.begin(), ::toupper);
            
            clearScreen();
            setCursor(0, 0);
            printf("=== DNS Lookup ===\n\n");
            printf("Query: %s %s\n\n", hostname.c_str(), type.c_str());
            
            WSADATA wsaData;
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
                printf("WSAStartup failed\n");
                setStatus(state, false, "dig", "WSA_FAILED");
            } else {
                struct addrinfo hints, *result = NULL;
                ZeroMemory(&hints, sizeof(hints));
                hints.ai_family = AF_UNSPEC;
                hints.ai_socktype = SOCK_STREAM;
                
                int ret = getaddrinfo(hostname.c_str(), NULL, &hints, &result);
                if (ret != 0) {
                    printf("DNS lookup failed: %s\n", gai_strerror(ret));
                    setStatus(state, false, "dig", "LOOKUP_FAILED");
                } else {
                    printf("Results:\n");
                    struct addrinfo* ptr = result;
                    int count = 0;
                    while (ptr != NULL) {
                        char ipstr[INET6_ADDRSTRLEN];
                        void* addr;
                        string family;
                        
                        if (ptr->ai_family == AF_INET) {
                            struct sockaddr_in* ipv4 = (struct sockaddr_in*)ptr->ai_addr;
                            addr = &(ipv4->sin_addr);
                            family = "IPv4";
                        } else {
                            struct sockaddr_in6* ipv6 = (struct sockaddr_in6*)ptr->ai_addr;
                            addr = &(ipv6->sin6_addr);
                            family = "IPv6";
                        }
                        
                        inet_ntop(ptr->ai_family, addr, ipstr, sizeof(ipstr));
                        printf("  %s: %s\n", family.c_str(), ipstr);
                        ptr = ptr->ai_next;
                        count++;
                    }
                    
                    freeaddrinfo(result);
                    setStatus(state, true, "dig", hostname + " (" + to_string(count) + " results)");
                }
                
                hostent* he = gethostbyname(hostname.c_str());
                if (he) {
                    printf("\nAliases:\n");
                    for (int i = 0; he->h_aliases[i] != NULL; i++) {
                        printf("  %s\n", he->h_aliases[i]);
                    }
                }
                
                WSACleanup();
            }
            
            printf("\nPress any key to continue...\n");
            getch();
        }
    } else if (args[0] == "ren" || args[0] == "rename") {
        if (args.size() < 3) {
            setStatus(state, false, "ren", "Usage: ren <old> <new> or ren <pattern> <replacement> -b");
        } else {
            bool batch = false;
            for (size_t i = 3; i < args.size(); i++) {
                if (args[i] == "-b" || args[i] == "/b") batch = true;
            }
            
            if (batch) {
                string pattern = args[1];
                string replacement = args[2];
                int renamed = 0;
                
                for (const auto& file : state->files) {
                    string oldName = WStringToString(file.name);
                    size_t pos = oldName.find(pattern);
                    if (pos != string::npos) {
                        string newName = oldName;
                        newName.replace(pos, pattern.length(), replacement);
                        string oldPath = WStringToString(mergePath(state->currentPath)) + "\\" + oldName;
                        string newPath = WStringToString(mergePath(state->currentPath)) + "\\" + newName;
                        if (MoveFileA(oldPath.c_str(), newPath.c_str())) {
                            renamed++;
                        }
                    }
                }
                
                for (const auto& dir : state->dirs) {
                    string oldName = WStringToString(dir.name);
                    size_t pos = oldName.find(pattern);
                    if (pos != string::npos) {
                        string newName = oldName;
                        newName.replace(pos, pattern.length(), replacement);
                        string oldPath = WStringToString(mergePath(state->currentPath)) + "\\" + oldName;
                        string newPath = WStringToString(mergePath(state->currentPath)) + "\\" + newName;
                        if (MoveFileA(oldPath.c_str(), newPath.c_str())) {
                            renamed++;
                        }
                    }
                }
                
                getFiles(state);
                setStatus(state, true, "ren", "Batch renamed " + to_string(renamed) + " items");
            } else {
                string oldPath = WStringToString(resolveFullPath(state, StringToWString(args[1])));
                string newPath = WStringToString(resolveFullPath(state, StringToWString(args[2])));
                
                if (MoveFileA(oldPath.c_str(), newPath.c_str())) {
                    setStatus(state, true, "ren", args[1] + " -> " + args[2]);
                    getFiles(state);
                } else {
                    DWORD err = GetLastError();
                    setStatus(state, false, "ren", args[1], "RENAME_FAILED_ERR=" + to_string(err));
                }
            }
        }
    } else if (args[0] == "cat" || args[0] == "type") {
        if (args.size() < 2) {
            setStatus(state, false, "cat", "Usage: cat <file>");
        } else {
            string filePath = WStringToString(resolveFullPath(state, StringToWString(args[1])));
            
            clearScreen();
            setCursor(0, 0);
            printf("=== %s ===\n\n", args[1].c_str());
            
            ifstream file(filePath);
            if (!file.is_open()) {
                printf("Cannot open file: %s\n", filePath.c_str());
                setStatus(state, false, "cat", args[1], "FILE_NOT_FOUND");
            } else {
                string line;
                int lineNum = 0;
                while (getline(file, line) && lineNum < 500) {
                    printf("%s\n", line.c_str());
                    lineNum++;
                }
                if (!file.eof()) {
                    printf("\n... (truncated, max 500 lines)");
                }
                setStatus(state, true, "cat", args[1]);
            }
            
            printf("\n\nPress any key to continue...\n");
            getch();
        }
    } else if (args[0] == "head") {
        if (args.size() < 2) {
            setStatus(state, false, "head", "Usage: head <file> [lines]");
        } else {
            string filePath = WStringToString(resolveFullPath(state, StringToWString(args[1])));
            int lines = 10;
            if (args.size() >= 3) lines = stoi(args[2]);
            
            clearScreen();
            setCursor(0, 0);
            printf("=== Head: %s (%d lines) ===\n\n", args[1].c_str(), lines);
            
            ifstream file(filePath);
            if (!file.is_open()) {
                printf("Cannot open file\n");
                setStatus(state, false, "head", args[1], "FILE_NOT_FOUND");
            } else {
                string line;
                int count = 0;
                while (getline(file, line) && count < lines) {
                    printf("%s\n", line.c_str());
                    count++;
                }
                setStatus(state, true, "head", args[1]);
            }
            
            printf("\nPress any key to continue...\n");
            getch();
        }
    } else if (args[0] == "tail") {
        if (args.size() < 2) {
            setStatus(state, false, "tail", "Usage: tail <file> [lines]");
        } else {
            string filePath = WStringToString(resolveFullPath(state, StringToWString(args[1])));
            int lines = 10;
            if (args.size() >= 3) lines = stoi(args[2]);
            
            clearScreen();
            setCursor(0, 0);
            printf("=== Tail: %s (%d lines) ===\n\n", args[1].c_str(), lines);
            
            ifstream file(filePath);
            if (!file.is_open()) {
                printf("Cannot open file\n");
                setStatus(state, false, "tail", args[1], "FILE_NOT_FOUND");
            } else {
                vector<string> buffer;
                string line;
                while (getline(file, line)) {
                    buffer.push_back(line);
                    if ((int)buffer.size() > lines) {
                        buffer.erase(buffer.begin());
                    }
                }
                
                for (const auto& l : buffer) {
                    printf("%s\n", l.c_str());
                }
                setStatus(state, true, "tail", args[1]);
            }
            
            printf("\nPress any key to continue...\n");
            getch();
        }
    } else if (args[0] == "touch") {
        if (args.size() < 2) {
            setStatus(state, false, "touch", "Usage: touch <file>");
        } else {
            string filePath = WStringToString(resolveFullPath(state, StringToWString(args[1])));
            
            HANDLE hFile = CreateFileA(filePath.c_str(), GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile != INVALID_HANDLE_VALUE) {
                FILETIME ft;
                SYSTEMTIME st;
                GetSystemTime(&st);
                SystemTimeToFileTime(&st, &ft);
                SetFileTime(hFile, NULL, NULL, &ft);
                CloseHandle(hFile);
                setStatus(state, true, "touch", args[1]);
                getFiles(state);
            } else {
                setStatus(state, false, "touch", args[1], "CREATE_FAILED");
            }
        }
    } else if (args[0] == "find" || args[0] == "where") {
        if (args.size() < 2) {
            setStatus(state, false, "find", "Usage: find <pattern> [path]");
        } else {
            string pattern = args[1];
            string searchPath = WStringToString(mergePath(state->currentPath));
            if (args.size() >= 3) {
                searchPath = WStringToString(resolveFullPath(state, StringToWString(args[2])));
            }
            
            clearScreen();
            setCursor(0, 0);
            printf("=== Find: %s in %s ===\n\n", pattern.c_str(), searchPath.c_str());
            
            vector<string> foundFiles;
            vector<string> dirs;
            dirs.push_back(searchPath);
            int scanned = 0;
            
            while (!dirs.empty() && foundFiles.size() < 100) {
                string currentDir = dirs.back();
                dirs.pop_back();
                
                WIN32_FIND_DATAA findData;
                string searchStr = currentDir + "\\*";
                
                HANDLE hFind = FindFirstFileA(searchStr.c_str(), &findData);
                if (hFind != INVALID_HANDLE_VALUE) {
                    do {
                        if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) continue;
                        
                        string name = findData.cFileName;
                        string fullPath = currentDir + "\\" + name;
                        
                        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                            dirs.push_back(fullPath);
                        }
                        
                        if (name.find(pattern) != string::npos) {
                            foundFiles.push_back(fullPath);
                            printf("%s\n", fullPath.c_str());
                        }
                        
                        scanned++;
                        if (scanned % 500 == 0) {
                            printf("\rScanned: %d items...", scanned);
                        }
                    } while (FindNextFileA(hFind, &findData) && foundFiles.size() < 100);
                    FindClose(hFind);
                }
            }
            
            printf("\n\nFound: %zu item(s)\n", foundFiles.size());
            setStatus(state, true, "find", pattern + " (" + to_string(foundFiles.size()) + " found)");
            
            printf("\nPress any key to continue...\n");
            getch();
        }
    } else if (args[0] == "tree") {
        string targetPath = WStringToString(mergePath(state->currentPath));
        if (args.size() >= 2) {
            targetPath = WStringToString(resolveFullPath(state, StringToWString(args[1])));
        }
        
        clearScreen();
        setCursor(0, 0);
        printf("=== Directory Tree ===\n\n");
        
        function<void(const string&, int)> printTree = [&](const string& path, int depth) {
            if (depth > 10) return;
            
            WIN32_FIND_DATAA findData;
            string searchStr = path + "\\*";
            
            HANDLE hFind = FindFirstFileA(searchStr.c_str(), &findData);
            if (hFind == INVALID_HANDLE_VALUE) return;
            
            do {
                if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) continue;
                
                for (int i = 0; i < depth; i++) printf("  ");
                printf("%s%s\n", findData.cFileName, 
                    (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? "\\" : "");
                
                if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    printTree(path + "\\" + findData.cFileName, depth + 1);
                }
            } while (FindNextFileA(hFind, &findData));
            
            FindClose(hFind);
        };
        
        printf("%s\n", targetPath.c_str());
        printTree(targetPath, 0);
        
        setStatus(state, true, "tree", targetPath);
        printf("\nPress any key to continue...\n");
        getch();
    } else if (args[0] == "wc") {
        if (args.size() < 2) {
            setStatus(state, false, "wc", "Usage: wc <file>");
        } else {
            string filePath = WStringToString(resolveFullPath(state, StringToWString(args[1])));
            
            ifstream file(filePath);
            if (!file.is_open()) {
                setStatus(state, false, "wc", args[1], "FILE_NOT_FOUND");
            } else {
                int lines = 0, words = 0, chars = 0;
                string line;
                while (getline(file, line)) {
                    lines++;
                    chars += line.length() + 1;
                    
                    bool inWord = false;
                    for (char c : line) {
                        if (isspace(c)) {
                            inWord = false;
                        } else if (!inWord) {
                            inWord = true;
                            words++;
                        }
                    }
                }
                
                setStatus(state, true, "wc", to_string(lines) + " lines, " + to_string(words) + " words, " + to_string(chars) + " chars");
            }
        }
    } else if (args[0] == "time") {
        auto now = chrono::system_clock::now();
        time_t t = chrono::system_clock::to_time_t(now);
        char buffer[64];
        ctime_s(buffer, sizeof(buffer), &t);
        setStatus(state, true, "time", string(buffer).substr(0, 24));
    } else if (args[0] == "date") {
        SYSTEMTIME st;
        GetLocalTime(&st);
        char buffer[64];
        sprintf(buffer, "%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
        setStatus(state, true, "date", string(buffer));
    } else if (args[0] == "uptime") {
        DWORD ticks = GetTickCount64() / 1000;
        int days = ticks / 86400;
        int hours = (ticks % 86400) / 3600;
        int mins = (ticks % 3600) / 60;
        int secs = ticks % 60;
        
        string uptime = to_string(days) + "d " + to_string(hours) + "h " + to_string(mins) + "m " + to_string(secs) + "s";
        setStatus(state, true, "uptime", uptime);
    } else if (args[0] == "whoami") {
        char username[256];
        DWORD size = 256;
        GetUserNameA(username, &size);
        char computer[256];
        size = 256;
        GetComputerNameA(computer, &size);
        setStatus(state, true, "whoami", string(computer) + "\\" + string(username));
    } else if (args[0] == "hostname") {
        char computer[256];
        DWORD size = 256;
        GetComputerNameA(computer, &size);
        setStatus(state, true, "hostname", string(computer));
    } else if (args[0] == "sleep") {
        if (args.size() < 2) {
            setStatus(state, false, "sleep", "Usage: sleep <seconds>");
        } else {
            int seconds = stoi(args[1]);
            Sleep(seconds * 1000);
            setStatus(state, true, "sleep", args[1] + " seconds");
        }
    } else if (args[0] == "echo") {
        string text;
        for (size_t i = 1; i < args.size(); i++) {
            if (i > 1) text += " ";
            text += args[i];
        }
        setStatus(state, true, "echo", text);
    } else if (args[0] == "edit") {
        if (args.size() < 2) {
            setStatus(state, false, "edit", "Usage: edit <file>");
        } else {
            string filePath = WStringToString(resolveFullPath(state, StringToWString(args[1])));
            ShellExecuteA(NULL, "open", "notepad.exe", filePath.c_str(), NULL, SW_SHOWNORMAL);
            setStatus(state, true, "edit", args[1]);
        }
    } else if (args[0] == "open") {
        string path = WStringToString(mergePath(state->currentPath));
        if (args.size() >= 2) {
            path = WStringToString(resolveFullPath(state, StringToWString(args[1])));
        }
        ShellExecuteA(NULL, "open", path.c_str(), NULL, NULL, SW_SHOWNORMAL);
        setStatus(state, true, "open", path);
    } else if (args[0] == "xdir" || args[0] == "ls") {
        clearScreen();
        setCursor(0, 0);
        printf("=== Directory Listing ===\n\n");
        
        string path = WStringToString(mergePath(state->currentPath));
        printf("Path: %s\n\n", path.c_str());
        
        printf("Directories:\n");
        for (const auto& dir : state->dirs) {
            wstring attrStr = getAttribStr(dir.attrib, true);
            wprintf(L"  %s %s\\\n", attrStr.c_str(), dir.name.c_str());
        }
        
        printf("\nFiles:\n");
        for (const auto& file : state->files) {
            wstring attrStr = getAttribStr(file.attrib, false);
            
            string filePath = path + "\\" + WStringToString(file.name);
            WIN32_FILE_ATTRIBUTE_DATA fileInfo;
            ULARGE_INTEGER fileSize;
            if (GetFileAttributesExA(filePath.c_str(), GetFileExInfoStandard, &fileInfo)) {
                fileSize.LowPart = fileInfo.nFileSizeLow;
                fileSize.HighPart = fileInfo.nFileSizeHigh;
            } else {
                fileSize.QuadPart = 0;
            }
            
            wprintf(L"  %s %-12llu %s\n", attrStr.c_str(), fileSize.QuadPart, file.name.c_str());
        }
        
        printf("\nTotal: %zu dirs, %zu files\n", state->dirs.size(), state->files.size());
        setStatus(state, true, "ls");
        printf("\nPress any key to continue...\n");
        getch();
    } else if (args[0] == "sort") {
        if (args.size() < 2) {
            setStatus(state, false, "sort", "Usage: sort <file>");
        } else {
            string filePath = WStringToString(resolveFullPath(state, StringToWString(args[1])));
            
            clearScreen();
            setCursor(0, 0);
            printf("=== Sorted: %s ===\n\n", args[1].c_str());
            
            ifstream file(filePath);
            if (!file.is_open()) {
                printf("Cannot open file\n");
                setStatus(state, false, "sort", args[1], "FILE_NOT_FOUND");
            } else {
                vector<string> lines;
                string line;
                while (getline(file, line)) {
                    lines.push_back(line);
                }
                
                sort(lines.begin(), lines.end());
                
                for (const auto& l : lines) {
                    printf("%s\n", l.c_str());
                }
                
                setStatus(state, true, "sort", args[1]);
            }
            
            printf("\nPress any key to continue...\n");
            getch();
        }
    } else if (args[0] == "uniq") {
        if (args.size() < 2) {
            setStatus(state, false, "uniq", "Usage: uniq <file>");
        } else {
            string filePath = WStringToString(resolveFullPath(state, StringToWString(args[1])));
            
            ifstream file(filePath);
            if (!file.is_open()) {
                setStatus(state, false, "uniq", args[1], "FILE_NOT_FOUND");
            } else {
                string prevLine, line;
                int uniqueCount = 0;
                while (getline(file, line)) {
                    if (line != prevLine) {
                        uniqueCount++;
                        prevLine = line;
                    }
                }
                setStatus(state, true, "uniq", to_string(uniqueCount) + " unique lines");
            }
        }
    } else if (args[0] == "diff") {
        if (args.size() < 3) {
            setStatus(state, false, "diff", "Usage: diff <file1> <file2>");
        } else {
            string file1 = WStringToString(resolveFullPath(state, StringToWString(args[1])));
            string file2 = WStringToString(resolveFullPath(state, StringToWString(args[2])));
            
            clearScreen();
            setCursor(0, 0);
            printf("=== Diff: %s vs %s ===\n\n", args[1].c_str(), args[2].c_str());
            
            ifstream f1(file1), f2(file2);
            if (!f1.is_open() || !f2.is_open()) {
                printf("Cannot open files\n");
                setStatus(state, false, "diff", "FILE_NOT_FOUND");
            } else {
                vector<string> lines1, lines2;
                string line;
                while (getline(f1, line)) lines1.push_back(line);
                while (getline(f2, line)) lines2.push_back(line);
                
                int maxLines = max(lines1.size(), lines2.size());
                int diffs = 0;
                
                for (int i = 0; i < maxLines && diffs < 50; i++) {
                    string l1 = (i < (int)lines1.size()) ? lines1[i] : "(missing)";
                    string l2 = (i < (int)lines2.size()) ? lines2[i] : "(missing)";
                    
                    if (l1 != l2) {
                        diffs++;
                        printf("Line %d:\n", i + 1);
                        printf("  < %s\n", l1.c_str());
                        printf("  > %s\n", l2.c_str());
                    }
                }
                
                if (diffs == 0) {
                    printf("Files are identical\n");
                } else {
                    printf("\n%d difference(s) found\n", diffs);
                }
                
                setStatus(state, true, "diff", to_string(diffs) + " differences");
            }
            
            printf("\nPress any key to continue...\n");
            getch();
        }
    } else if (args[0] == "which") {
        if (args.size() < 2) {
            setStatus(state, false, "which", "Usage: which <command>");
        } else {
            string cmd = args[1];
            char path[MAX_PATH];
            
            string extensions[] = {".exe", ".cmd", ".bat", ".com"};
            bool found = false;
            
            for (const auto& ext : extensions) {
                string search = cmd + ext;
                if (SearchPathA(NULL, search.c_str(), NULL, MAX_PATH, path, NULL)) {
                    setStatus(state, true, "which", string(path));
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                setStatus(state, false, "which", cmd, "NOT_FOUND");
            }
        }
    } else if (args[0] == "reg" || args[0] == "registry") {
        if (args.size() < 3) {
            setStatus(state, false, "reg", "Usage: reg <get|set|del|list> <path> [value|data]");
        } else {
            string action = args[1];
            string regPath = args[2];
            
            HKEY hRootKey = HKEY_CURRENT_USER;
            string subPath = regPath;
            
            if (regPath.find("HKLM\\") == 0 || regPath.find("HKEY_LOCAL_MACHINE\\") == 0) {
                hRootKey = HKEY_LOCAL_MACHINE;
                subPath = regPath.substr(regPath.find('\\') + 1);
            } else if (regPath.find("HKCU\\") == 0 || regPath.find("HKEY_CURRENT_USER\\") == 0) {
                hRootKey = HKEY_CURRENT_USER;
                subPath = regPath.substr(regPath.find('\\') + 1);
            } else if (regPath.find("HKCR\\") == 0 || regPath.find("HKEY_CLASSES_ROOT\\") == 0) {
                hRootKey = HKEY_CLASSES_ROOT;
                subPath = regPath.substr(regPath.find('\\') + 1);
            } else if (regPath.find("HKU\\") == 0 || regPath.find("HKEY_USERS\\") == 0) {
                hRootKey = HKEY_USERS;
                subPath = regPath.substr(regPath.find('\\') + 1);
            }
            
            if (action == "get" || action == "query") {
                string valueName = args.size() >= 4 ? args[3] : "";
                
                HKEY hKey;
                if (RegOpenKeyExA(hRootKey, subPath.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
                    setStatus(state, false, "reg", regPath, "KEY_NOT_FOUND");
                } else {
                    DWORD type, size = 0;
                    if (RegQueryValueExA(hKey, valueName.empty() ? NULL : valueName.c_str(), 0, &type, NULL, &size) != ERROR_SUCCESS) {
                        setStatus(state, false, "reg", valueName.empty() ? "(Default)" : valueName, "VALUE_NOT_FOUND");
                    } else {
                        vector<BYTE> data(size);
                        RegQueryValueExA(hKey, valueName.empty() ? NULL : valueName.c_str(), 0, &type, data.data(), &size);
                        
                        string typeStr;
                        string valueStr;
                        
                        switch (type) {
                            case REG_SZ:
                            case REG_EXPAND_SZ:
                                typeStr = "REG_SZ";
                                valueStr = string((char*)data.data(), size > 0 ? size - 1 : 0);
                                break;
                            case REG_DWORD:
                                typeStr = "REG_DWORD";
                                valueStr = to_string(*(DWORD*)data.data());
                                break;
                            case REG_QWORD:
                                typeStr = "REG_QWORD";
                                valueStr = to_string(*(ULONGLONG*)data.data());
                                break;
                            case REG_BINARY:
                                typeStr = "REG_BINARY";
                                {
                                    stringstream ss;
                                    for (DWORD i = 0; i < size && i < 64; i++) {
                                        ss << hex << setfill('0') << setw(2) << (int)data[i] << " ";
                                    }
                                    valueStr = ss.str();
                                }
                                break;
                            case REG_MULTI_SZ:
                                typeStr = "REG_MULTI_SZ";
                                for (DWORD i = 0; i < size && data[i]; i++) {
                                    if (data[i] >= 32 && data[i] < 127) valueStr += (char)data[i];
                                    else if (data[i] == 0 && i + 1 < size) valueStr += " | ";
                                }
                                break;
                            default:
                                typeStr = "REG_" + to_string(type);
                                valueStr = "(binary data)";
                        }
                        
                        setStatus(state, true, "reg get", (valueName.empty() ? "(Default)" : valueName) + " [" + typeStr + "]: " + valueStr);
                    }
                    RegCloseKey(hKey);
                }
            } else if (action == "set") {
                if (args.size() < 5) {
                    setStatus(state, false, "reg", "Usage: reg set <path> <value> <data> [type]");
                } else {
                    string valueName = args[3];
                    string data = args[4];
                    string typeStr = args.size() >= 6 ? args[5] : "sz";
                    
                    DWORD type = REG_SZ;
                    if (typeStr == "dword") type = REG_DWORD;
                    else if (typeStr == "qword") type = REG_QWORD;
                    else if (typeStr == "expand_sz") type = REG_EXPAND_SZ;
                    else if (typeStr == "binary") type = REG_BINARY;
                    else if (typeStr == "multi_sz") type = REG_MULTI_SZ;
                    
                    HKEY hKey;
                    DWORD disp;
                    if (RegCreateKeyExA(hRootKey, subPath.c_str(), 0, NULL, 0, KEY_WRITE, NULL, &hKey, &disp) != ERROR_SUCCESS) {
                        setStatus(state, false, "reg", regPath, "CREATE_FAILED");
                    } else {
                        LONG result = ERROR_SUCCESS;
                        
                        if (type == REG_SZ || type == REG_EXPAND_SZ) {
                            result = RegSetValueExA(hKey, valueName.c_str(), 0, type, (const BYTE*)data.c_str(), data.length() + 1);
                        } else if (type == REG_DWORD) {
                            DWORD val = stoul(data);
                            result = RegSetValueExA(hKey, valueName.c_str(), 0, type, (const BYTE*)&val, sizeof(val));
                        } else if (type == REG_QWORD) {
                            ULONGLONG val = stoull(data);
                            result = RegSetValueExA(hKey, valueName.c_str(), 0, type, (const BYTE*)&val, sizeof(val));
                        } else if (type == REG_BINARY) {
                            vector<BYTE> binData;
                            for (size_t i = 0; i < data.length(); i += 2) {
                                if (i + 1 < data.length()) {
                                    binData.push_back((BYTE)stoul(data.substr(i, 2), nullptr, 16));
                                }
                            }
                            result = RegSetValueExA(hKey, valueName.c_str(), 0, type, binData.data(), binData.size());
                        }
                        
                        RegCloseKey(hKey);
                        
                        if (result == ERROR_SUCCESS) {
                            setStatus(state, true, "reg set", valueName + " = " + data);
                        } else {
                            setStatus(state, false, "reg set", "ERROR " + to_string(result));
                        }
                    }
                }
            } else if (action == "del" || action == "delete") {
                string valueName = args.size() >= 4 ? args[3] : "";
                
                HKEY hKey;
                if (RegOpenKeyExA(hRootKey, subPath.c_str(), 0, KEY_WRITE, &hKey) != ERROR_SUCCESS) {
                    setStatus(state, false, "reg", regPath, "KEY_NOT_FOUND");
                } else {
                    LONG result;
                    if (valueName.empty()) {
                        RegCloseKey(hKey);
                        result = RegDeleteKeyA(hRootKey, subPath.c_str());
                    } else {
                        result = RegDeleteValueA(hKey, valueName.c_str());
                        RegCloseKey(hKey);
                    }
                    
                    if (result == ERROR_SUCCESS) {
                        setStatus(state, true, "reg del", valueName.empty() ? regPath : valueName);
                    } else {
                        setStatus(state, false, "reg del", "ERROR " + to_string(result));
                    }
                }
            } else if (action == "list") {
                HKEY hKey;
                if (RegOpenKeyExA(hRootKey, subPath.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
                    setStatus(state, false, "reg", regPath, "KEY_NOT_FOUND");
                } else {
                    clearScreen();
                    setCursor(0, 0);
                    printf("=== Registry: %s ===\n\n", regPath.c_str());
                    
                    char name[256];
                    DWORD nameSize;
                    int index = 0;
                    
                    printf("Subkeys:\n");
                    while (true) {
                        nameSize = sizeof(name);
                        if (RegEnumKeyExA(hKey, index, name, &nameSize, NULL, NULL, NULL, NULL) != ERROR_SUCCESS) break;
                        printf("  %s\n", name);
                        index++;
                    }
                    
                    printf("\nValues:\n");
                    index = 0;
                    while (true) {
                        nameSize = sizeof(name);
                        DWORD type;
                        if (RegEnumValueA(hKey, index, name, &nameSize, NULL, &type, NULL, NULL) != ERROR_SUCCESS) break;
                        string typeStr;
                        switch (type) {
                            case REG_SZ: typeStr = "SZ"; break;
                            case REG_DWORD: typeStr = "DWORD"; break;
                            case REG_QWORD: typeStr = "QWORD"; break;
                            case REG_BINARY: typeStr = "BIN"; break;
                            default: typeStr = "?";
                        }
                        printf("  [%s] %s\n", typeStr.c_str(), name[0] ? name : "(Default)");
                        index++;
                    }
                    
                    RegCloseKey(hKey);
                    setStatus(state, true, "reg list", regPath);
                    printf("\nPress any key to continue...\n");
                    getch();
                }
            } else {
                setStatus(state, false, "reg", "Unknown action: " + action);
            }
        }
    } else if (args[0] == "service" || args[0] == "sc") {
        if (args.size() < 2) {
            setStatus(state, false, "service", "Usage: service <list|start|stop|restart|query> [name]");
        } else {
            string action = args[1];
            
            if (action == "list") {
                clearScreen();
                setCursor(0, 0);
                printf("=== Services ===\n\n");
                
                SC_HANDLE hSCManager = OpenSCManagerA(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
                if (!hSCManager) {
                    setStatus(state, false, "service", "OPEN_SC_MANAGER_FAILED");
                } else {
                    DWORD bytesNeeded, servicesReturned, resumeHandle = 0;
                    EnumServicesStatusA(hSCManager, SERVICE_WIN32, SERVICE_STATE_ALL, NULL, 0, &bytesNeeded, &servicesReturned, &resumeHandle);
                    
                    vector<BYTE> buffer(bytesNeeded);
                    LPENUM_SERVICE_STATUSA services = (LPENUM_SERVICE_STATUSA)buffer.data();
                    
                    if (EnumServicesStatusA(hSCManager, SERVICE_WIN32, SERVICE_STATE_ALL, services, bytesNeeded, &bytesNeeded, &servicesReturned, &resumeHandle)) {
                        printf("%-40s %-12s %s\n", "Name", "Status", "Display Name");
                        printf("%s\n", string(80, '-').c_str());
                        
                        for (DWORD i = 0; i < servicesReturned; i++) {
                            string statusStr;
                            switch (services[i].ServiceStatus.dwCurrentState) {
                                case SERVICE_RUNNING: statusStr = "Running"; break;
                                case SERVICE_STOPPED: statusStr = "Stopped"; break;
                                case SERVICE_PAUSED: statusStr = "Paused"; break;
                                case SERVICE_START_PENDING: statusStr = "Starting"; break;
                                case SERVICE_STOP_PENDING: statusStr = "Stopping"; break;
                                default: statusStr = "Unknown";
                            }
                            printf("%-40s %-12s %s\n", services[i].lpServiceName, statusStr.c_str(), services[i].lpDisplayName);
                        }
                        setStatus(state, true, "service list", to_string(servicesReturned) + " services");
                    } else {
                        setStatus(state, false, "service", "ENUM_FAILED");
                    }
                    CloseServiceHandle(hSCManager);
                }
                printf("\nPress any key to continue...\n");
                getch();
            } else if (action == "start" || action == "stop" || action == "restart" || action == "query") {
                if (args.size() < 3) {
                    setStatus(state, false, "service", "Service name required");
                } else {
                    string svcName = args[2];
                    
                    SC_HANDLE hSCManager = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT);
                    if (!hSCManager) {
                        setStatus(state, false, "service", "OPEN_SC_MANAGER_FAILED");
                    } else {
                        SC_HANDLE hService = OpenServiceA(hSCManager, svcName.c_str(), SERVICE_ALL_ACCESS);
                        if (!hService) {
                            setStatus(state, false, "service", svcName, "NOT_FOUND");
                            CloseServiceHandle(hSCManager);
                        } else {
                            if (action == "query") {
                                SERVICE_STATUS status;
                                if (QueryServiceStatus(hService, &status)) {
                                    string statusStr;
                                    switch (status.dwCurrentState) {
                                        case SERVICE_RUNNING: statusStr = "Running"; break;
                                        case SERVICE_STOPPED: statusStr = "Stopped"; break;
                                        case SERVICE_PAUSED: statusStr = "Paused"; break;
                                        default: statusStr = "State " + to_string(status.dwCurrentState);
                                    }
                                    setStatus(state, true, "service query", svcName + ": " + statusStr);
                                } else {
                                    setStatus(state, false, "service query", "QUERY_FAILED");
                                }
                            } else if (action == "start") {
                                SERVICE_STATUS status;
                                if (StartServiceA(hService, 0, NULL)) {
                                    setStatus(state, true, "service start", svcName);
                                } else {
                                    DWORD err = GetLastError();
                                    if (err == ERROR_SERVICE_ALREADY_RUNNING) {
                                        setStatus(state, false, "service start", svcName, "ALREADY_RUNNING");
                                    } else {
                                        setStatus(state, false, "service start", svcName, "ERROR " + to_string(err));
                                    }
                                }
                            } else if (action == "stop") {
                                SERVICE_STATUS status;
                                if (ControlService(hService, SERVICE_CONTROL_STOP, &status)) {
                                    setStatus(state, true, "service stop", svcName);
                                } else {
                                    DWORD err = GetLastError();
                                    setStatus(state, false, "service stop", svcName, "ERROR " + to_string(err));
                                }
                            } else if (action == "restart") {
                                SERVICE_STATUS status;
                                ControlService(hService, SERVICE_CONTROL_STOP, &status);
                                Sleep(1000);
                                if (StartServiceA(hService, 0, NULL)) {
                                    setStatus(state, true, "service restart", svcName);
                                } else {
                                    setStatus(state, false, "service restart", svcName, "START_FAILED");
                                }
                            }
                            CloseServiceHandle(hService);
                            CloseServiceHandle(hSCManager);
                        }
                    }
                }
            } else {
                setStatus(state, false, "service", "Unknown action: " + action);
            }
        }
    } else if (args[0] == "window" || args[0] == "win") {
        if (args.size() < 2) {
            setStatus(state, false, "window", "Usage: window <list|close|minimize|maximize|focus> [title]");
        } else {
            string action = args[1];
            
            if (action == "list") {
                clearScreen();
                setCursor(0, 0);
                printf("=== Windows ===\n\n");
                printf("%-10s %-40s\n", "Handle", "Title");
                printf("%s\n", string(52, '-').c_str());
                
                struct EnumData { int count; };
                EnumData data = {0};
                
                EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
                    EnumData* d = (EnumData*)lParam;
                    if (IsWindowVisible(hwnd)) {
                        char title[256];
                        GetWindowTextA(hwnd, title, sizeof(title));
                        if (title[0]) {
                            printf("%-10lu %-40s\n", (DWORD)(uintptr_t)hwnd, title);
                            d->count++;
                            if (d->count >= 30) return FALSE;
                        }
                    }
                    return TRUE;
                }, (LPARAM)&data);
                
                setStatus(state, true, "window list", to_string(data.count) + " windows");
                printf("\nPress any key to continue...\n");
                getch();
            } else {
                if (args.size() < 3) {
                    setStatus(state, false, "window", "Window title or handle required");
                } else {
                    HWND hwnd = NULL;
                    string target = args[2];
                    
                    bool isHandle = true;
                    for (char c : target) {
                        if (!isdigit(c)) { isHandle = false; break; }
                    }
                    
                    if (isHandle) {
                        hwnd = (HWND)(uintptr_t)stoull(target);
                    } else {
                        hwnd = FindWindowA(NULL, target.c_str());
                    }
                    
                    if (!hwnd || !IsWindow(hwnd)) {
                        setStatus(state, false, "window", target, "NOT_FOUND");
                    } else {
                        if (action == "close") {
                            PostMessage(hwnd, WM_CLOSE, 0, 0);
                            setStatus(state, true, "window close", target);
                        } else if (action == "minimize") {
                            ShowWindow(hwnd, SW_MINIMIZE);
                            setStatus(state, true, "window minimize", target);
                        } else if (action == "maximize") {
                            ShowWindow(hwnd, SW_MAXIMIZE);
                            setStatus(state, true, "window maximize", target);
                        } else if (action == "restore") {
                            ShowWindow(hwnd, SW_RESTORE);
                            setStatus(state, true, "window restore", target);
                        } else if (action == "focus" || action == "activate") {
                            SetForegroundWindow(hwnd);
                            setStatus(state, true, "window focus", target);
                        } else {
                            setStatus(state, false, "window", "Unknown action: " + action);
                        }
                    }
                }
            }
        }
    } else if (args[0] == "screenshot" || args[0] == "scr") {
        string outputFile = "screenshot.png";
        bool fullscreen = true;
        int x = 0, y = 0, w = 0, h = 0;
        
        for (size_t i = 1; i < args.size(); i++) {
            if (args[i] == "-o" && i + 1 < args.size()) {
                outputFile = args[++i];
            } else if (args[i] == "-r" && i + 4 < args.size()) {
                fullscreen = false;
                x = stoi(args[++i]);
                y = stoi(args[++i]);
                w = stoi(args[++i]);
                h = stoi(args[++i]);
            }
        }
        
        string outPath = WStringToString(resolveFullPath(state, StringToWString(outputFile)));
        
        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);
        
        if (fullscreen) {
            w = screenW;
            h = screenH;
        }
        
        HDC hdcScreen = GetDC(NULL);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, w, h);
        HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);
        
        BitBlt(hdcMem, 0, 0, w, h, hdcScreen, x, y, SRCCOPY);
        
        BITMAPINFOHEADER bi = {0};
        bi.biSize = sizeof(bi);
        bi.biWidth = w;
        bi.biHeight = -h;
        bi.biPlanes = 1;
        bi.biBitCount = 24;
        bi.biCompression = BI_RGB;
        
        int rowSize = ((w * 3 + 3) / 4) * 4;
        int imageSize = rowSize * h;
        vector<BYTE> bits(imageSize);
        
        GetDIBits(hdcMem, hBitmap, 0, h, bits.data(), (BITMAPINFO*)&bi, DIB_RGB_COLORS);
        
        FILE* fp = fopen(outPath.c_str(), "wb");
        if (fp) {
            int fileSize = 54 + imageSize;
            BYTE header[54] = {
                'B', 'M',
                (BYTE)(fileSize), (BYTE)(fileSize >> 8), (BYTE)(fileSize >> 16), (BYTE)(fileSize >> 24),
                0, 0, 0, 0,
                54, 0, 0, 0,
                40, 0, 0, 0,
                (BYTE)w, (BYTE)(w >> 8), (BYTE)(w >> 16), (BYTE)(w >> 24),
                (BYTE)h, (BYTE)(h >> 8), (BYTE)(h >> 16), (BYTE)(h >> 24),
                1, 0,
                24, 0,
                0, 0, 0, 0,
                (BYTE)imageSize, (BYTE)(imageSize >> 8), (BYTE)(imageSize >> 16), (BYTE)(imageSize >> 24),
                0, 0, 0, 0,
                0, 0, 0, 0,
                0, 0, 0, 0,
                0, 0, 0, 0
            };
            
            fwrite(header, 1, 54, fp);
            fwrite(bits.data(), 1, imageSize, fp);
            fclose(fp);
            
            setStatus(state, true, "screenshot", outputFile + " (" + to_string(w) + "x" + to_string(h) + ")");
            getFiles(state);
        } else {
            setStatus(state, false, "screenshot", outputFile, "WRITE_FAILED");
        }
        
        SelectObject(hdcMem, hOldBitmap);
        DeleteObject(hBitmap);
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
    } else if (args[0] == "perf" || args[0] == "performance") {
        clearScreen();
        setCursor(0, 0);
        printf("=== Performance Monitor ===\n\n");
        
        MEMORYSTATUSEX memStatus;
        memStatus.dwLength = sizeof(memStatus);
        GlobalMemoryStatusEx(&memStatus);
        
        printf("Memory:\n");
        printf("  Load: %lu%%\n", memStatus.dwMemoryLoad);
        printf("  Total: %.2f GB\n", memStatus.ullTotalPhys / (1024.0 * 1024 * 1024));
        printf("  Available: %.2f GB\n", memStatus.ullAvailPhys / (1024.0 * 1024 * 1024));
        printf("  Committed: %.2f / %.2f GB\n", 
            memStatus.ullTotalPageFile / (1024.0 * 1024 * 1024),
            memStatus.ullAvailPageFile / (1024.0 * 1024 * 1024));
        
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        printf("\nCPU:\n");
        printf("  Processors: %lu\n", sysInfo.dwNumberOfProcessors);
        
        static ULONGLONG prevIdle = 0, prevKernel = 0, prevUser = 0;
        FILETIME idleTime, kernelTime, userTime;
        if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
            ULONGLONG idle = ((ULONGLONG)idleTime.dwHighDateTime << 32) | idleTime.dwLowDateTime;
            ULONGLONG kernel = ((ULONGLONG)kernelTime.dwHighDateTime << 32) | kernelTime.dwLowDateTime;
            ULONGLONG user = ((ULONGLONG)userTime.dwHighDateTime << 32) | userTime.dwLowDateTime;
            
            if (prevIdle > 0) {
                ULONGLONG idleDiff = idle - prevIdle;
                ULONGLONG kernelDiff = kernel - prevKernel;
                ULONGLONG userDiff = user - prevUser;
                ULONGLONG total = idleDiff + kernelDiff + userDiff;
                if (total > 0) {
                    double cpuUsage = 100.0 * (1.0 - (double)idleDiff / total);
                    printf("  Usage: %.1f%%\n", cpuUsage);
                }
            }
            
            prevIdle = idle;
            prevKernel = kernel;
            prevUser = user;
        }
        
        printf("\nDisk:\n");
        DWORD drives = GetLogicalDrives();
        for (char d = 'C'; d <= 'Z'; d++) {
            if (drives & (1 << (d - 'A'))) {
                string root = string(1, d) + ":\\";
                ULARGE_INTEGER freeBytes, totalBytes;
                if (GetDiskFreeSpaceExA(root.c_str(), NULL, &totalBytes, &freeBytes)) {
                    double usedPercent = 100.0 * (1.0 - (double)freeBytes.QuadPart / totalBytes.QuadPart);
                    printf("  %s %.1f%% used (%.1f GB free)\n", root.c_str(), usedPercent, 
                        freeBytes.QuadPart / (1024.0 * 1024 * 1024));
                }
            }
        }
        
        printf("\nNetwork:\n");
        DWORD bps;
        if (GetIfEntry2) {
            MIB_IF_ROW2 ifRow;
            PMIB_IF_TABLE2 ifTable;
            if (GetIfTable2(&ifTable) == NO_ERROR) {
                for (ULONG i = 0; i < ifTable->NumEntries; i++) {
                    ifRow = ifTable->Table[i];
                    if (ifRow.OperStatus == IfOperStatusUp && ifRow.Type != MIB_IF_TYPE_LOOPBACK) {
                        wstring name(ifRow.Description);
                        printf("  %S: RX=%llu MB, TX=%llu MB\n", 
                            ifRow.Description,
                            ifRow.InOctets / (1024 * 1024),
                            ifRow.OutOctets / (1024 * 1024));
                    }
                }
                FreeMibTable(ifTable);
            }
        }
        
        setStatus(state, true, "perf");
        printf("\nPress any key to continue...\n");
        getch();
    } else if (args[0] == "tts" || args[0] == "speak") {
        if (args.size() < 2) {
            setStatus(state, false, "tts", "Usage: tts <text>");
        } else {
            string text;
            for (size_t i = 1; i < args.size(); i++) {
                if (i > 1) text += " ";
                text += args[i];
            }
            
            string psCmd = "powershell -Command \"Add-Type -AssemblyName System.Speech; $s = New-Object System.Speech.Synthesis.SpeechSynthesizer; $s.Speak('" + text + "');\"";
            int result = system(psCmd.c_str());
            if (result == 0) {
                setStatus(state, true, "tts", text.substr(0, 50));
            } else {
                setStatus(state, false, "tts", "SPEAK_FAILED");
            }
        }
    } else if (args[0] == "user") {
        if (args.size() < 2) {
            setStatus(state, false, "user", "Usage: user <list|info|add|del> [name]");
        } else {
            string action = args[1];
            
            if (action == "list") {
                clearScreen();
                setCursor(0, 0);
                printf("=== Users ===\n\n");
                
                NET_API_STATUS status;
                LPUSER_INFO_0 pBuf = NULL;
                DWORD entriesRead, totalEntries;
                
                status = NetUserEnum(NULL, 0, FILTER_NORMAL_ACCOUNT, (LPBYTE*)&pBuf, MAX_PREFERRED_LENGTH, &entriesRead, &totalEntries, NULL);
                
                if (status == NERR_Success) {
                    printf("%-30s\n", string(30, '-').c_str());
                    for (DWORD i = 0; i < entriesRead; i++) {
                        wprintf(L"  %s\n", pBuf[i].usri0_name);
                    }
                    NetApiBufferFree(pBuf);
                    setStatus(state, true, "user list", to_string(entriesRead) + " users");
                } else {
                    setStatus(state, false, "user list", "ENUM_FAILED");
                }
                
                printf("\nPress any key to continue...\n");
                getch();
            } else if (action == "info") {
                if (args.size() < 3) {
                    setStatus(state, false, "user info", "Username required");
                } else {
                    wstring username(args[2].begin(), args[2].end());
                    LPUSER_INFO_1 pBuf = NULL;
                    
                    if (NetUserGetInfo(NULL, (LPCWSTR)username.c_str(), 1, (LPBYTE*)&pBuf) == NERR_Success) {
                        string info = "Name: " + args[2];
                        info += ", Priv: ";
                        switch (pBuf->usri1_priv) {
                            case USER_PRIV_GUEST: info += "Guest"; break;
                            case USER_PRIV_USER: info += "User"; break;
                            case USER_PRIV_ADMIN: info += "Admin"; break;
                            default: info += to_string(pBuf->usri1_priv);
                        }
                        info += ", Flags: " + to_string(pBuf->usri1_flags);
                        NetApiBufferFree(pBuf);
                        setStatus(state, true, "user info", info);
                    } else {
                        setStatus(state, false, "user info", args[2], "NOT_FOUND");
                    }
                }
            } else {
                setStatus(state, false, "user", "Unknown action (add/del require admin)");
            }
        }
    } else if (args[0] == "share") {
        if (args.size() < 2) {
            setStatus(state, false, "share", "Usage: share <list|add|del> [path] [name]");
        } else {
            string action = args[1];
            
            if (action == "list") {
                clearScreen();
                setCursor(0, 0);
                printf("=== Network Shares ===\n\n");
                printf("%-20s %-40s\n", "Name", "Path");
                printf("%s\n", string(62, '-').c_str());
                
                PSHARE_INFO_1 pBuf = NULL;
                DWORD entriesRead, totalEntries;
                
                if (NetShareEnum(NULL, 1, (LPBYTE*)&pBuf, MAX_PREFERRED_LENGTH, &entriesRead, &totalEntries, NULL) == NERR_Success) {
                    for (DWORD i = 0; i < entriesRead; i++) {
                        wstring path;
                        if (pBuf[i].shi1_type == STYPE_DISKTREE) {
                            PSHARE_INFO_2 pBuf2 = NULL;
                            if (NetShareGetInfo(NULL, pBuf[i].shi1_netname, 2, (LPBYTE*)&pBuf2) == NERR_Success) {
                                path = pBuf2->shi2_path;
                                NetApiBufferFree(pBuf2);
                            }
                        }
                        wprintf(L"%-20s %-40s\n", pBuf[i].shi1_netname, path.c_str());
                    }
                    NetApiBufferFree(pBuf);
                    setStatus(state, true, "share list", to_string(entriesRead) + " shares");
                } else {
                    setStatus(state, false, "share list", "ENUM_FAILED");
                }
                
                printf("\nPress any key to continue...\n");
                getch();
            } else {
                setStatus(state, false, "share", "Unknown action (add/del require admin)");
            }
        }
    } else if (args[0] == "firewall" || args[0] == "fw") {
        if (args.size() < 2) {
            setStatus(state, false, "firewall", "Usage: firewall <status|profiles>");
        } else {
            string action = args[1];
            
            if (action == "status" || action == "profiles") {
                clearScreen();
                setCursor(0, 0);
                printf("=== Firewall Status ===\n\n");
                
                HKEY hKey;
                const char* profiles[] = {
                    "Domain",
                    "Public", 
                    "Standard"  // StandardProfile
                };
                const char* profileKeys[] = {
                    "SYSTEM\\CurrentControlSet\\Services\\SharedAccess\\Parameters\\FirewallPolicy\\DomainProfile",
                    "SYSTEM\\CurrentControlSet\\Services\\SharedAccess\\Parameters\\FirewallPolicy\\PublicProfile",
                    "SYSTEM\\CurrentControlSet\\Services\\SharedAccess\\Parameters\\FirewallPolicy\\StandardProfile"
                };
                
                for (int i = 0; i < 3; i++) {
                    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, profileKeys[i], 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
                        DWORD enabled = 0, size = sizeof(DWORD);
                        if (RegQueryValueExA(hKey, "EnableFirewall", NULL, NULL, (LPBYTE)&enabled, &size) == ERROR_SUCCESS) {
                            printf("%-10s: %s\n", profiles[i], enabled ? "Enabled" : "Disabled");
                        }
                        RegCloseKey(hKey);
                    }
                }
                
                setStatus(state, true, "firewall status");
                printf("\nPress any key to continue...\n");
                getch();
            } else {
                setStatus(state, false, "firewall", "Unknown action (use: status, profiles)");
            }
        }
    } else if (args[0] == "eventlog" || args[0] == "elog") {
        if (args.size() < 2) {
            setStatus(state, false, "eventlog", "Usage: eventlog <list|read> [logname] [count]");
        } else {
            string action = args[1];
            
            if (action == "list") {
                clearScreen();
                setCursor(0, 0);
                printf("=== Event Logs ===\n\n");
                
                HANDLE hEventLog = OpenEventLogA(NULL, NULL);
                if (hEventLog) {
                    char logName[256];
                    DWORD bytesRead, bytesNeeded;
                    
                    while (ReadEventLogA(hEventLog, EVENTLOG_SEQUENTIAL_READ | EVENTLOG_FORWARDS_READ,
                        0, NULL, 0, &bytesRead, &bytesNeeded)) {
                    }
                    
                    CloseEventLog(hEventLog);
                }
                
                const char* commonLogs[] = {"Application", "System", "Security"};
                for (int i = 0; i < 3; i++) {
                    HANDLE hLog = OpenEventLogA(NULL, commonLogs[i]);
                    if (hLog) {
                        DWORD oldest, newest;
                        GetOldestEventLogRecord(hLog, &oldest);
                        GetNumberOfEventLogRecords(hLog, &newest);
                        printf("%-15s %lu records\n", commonLogs[i], newest);
                        CloseEventLog(hLog);
                    }
                }
                
                setStatus(state, true, "eventlog list");
                printf("\nPress any key to continue...\n");
                getch();
            } else if (action == "read") {
                string logName = args.size() >= 3 ? args[2] : "Application";
                int count = args.size() >= 4 ? stoi(args[3]) : 20;
                
                clearScreen();
                setCursor(0, 0);
                printf("=== Event Log: %s (last %d) ===\n\n", logName.c_str(), count);
                
                HANDLE hLog = OpenEventLogA(NULL, logName.c_str());
                if (!hLog) {
                    setStatus(state, false, "eventlog", logName, "OPEN_FAILED");
                } else {
                    DWORD oldest, total;
                    GetOldestEventLogRecord(hLog, &oldest);
                    GetNumberOfEventLogRecords(hLog, &total);
                    
                    DWORD bufferSize = 0;
                    EVENTLOGRECORD* pRecord = NULL;
                    DWORD bytesRead, bytesNeeded;
                    int readCount = 0;
                    
                    __int64 startRecord = oldest + total - count;
                    if (startRecord < oldest) startRecord = oldest;
                    
                    while (readCount < count) {
                        if (!ReadEventLogA(hLog, EVENTLOG_SEQUENTIAL_READ | EVENTLOG_FORWARDS_READ,
                            0, pRecord, bufferSize, &bytesRead, &bytesNeeded)) {
                            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                                bufferSize = bytesNeeded;
                                pRecord = (EVENTLOGRECORD*)realloc(pRecord, bufferSize);
                                continue;
                            }
                            break;
                        }
                        
                        BYTE* ptr = (BYTE*)pRecord;
                        while (bytesRead > 0 && readCount < count) {
                            EVENTLOGRECORD* pRec = (EVENTLOGRECORD*)ptr;
                            
                            char* sourceName = (char*)(ptr + sizeof(EVENTLOGRECORD));
                            DWORD eventId = pRec->EventID & 0xFFFF;
                            
                            string typeStr;
                            switch (pRec->EventType) {
                                case EVENTLOG_ERROR_TYPE: typeStr = "ERROR"; break;
                                case EVENTLOG_WARNING_TYPE: typeStr = "WARN"; break;
                                case EVENTLOG_INFORMATION_TYPE: typeStr = "INFO"; break;
                                case EVENTLOG_SUCCESS: typeStr = "SUCCESS"; break;
                                default: typeStr = "OTHER";
                            }
                            
                            printf("[%s] %s: EventID=%lu\n", typeStr.c_str(), sourceName, eventId);
                            
                            ptr += pRec->Length;
                            bytesRead -= pRec->Length;
                            readCount++;
                        }
                    }
                    
                    free(pRecord);
                    CloseEventLog(hLog);
                    setStatus(state, true, "eventlog read", logName);
                }
                
                printf("\nPress any key to continue...\n");
                getch();
            } else {
                setStatus(state, false, "eventlog", "Unknown action");
            }
        }
    } else if (args[0] == "power" || args[0] == "battery") {
        clearScreen();
        setCursor(0, 0);
        printf("=== Power Status ===\n\n");
        
        SYSTEM_POWER_STATUS status;
        if (GetSystemPowerStatus(&status)) {
            printf("AC Line Status: ");
            switch (status.ACLineStatus) {
                case 0: printf("Offline\n"); break;
                case 1: printf("Online\n"); break;
                default: printf("Unknown\n");
            }
            
            printf("Battery Flag: ");
            if (status.BatteryFlag & 128) printf("No battery\n");
            else {
                if (status.BatteryFlag & 1) printf("High ");
                if (status.BatteryFlag & 2) printf("Low ");
                if (status.BatteryFlag & 4) printf("Critical ");
                if (status.BatteryFlag & 8) printf("Charging ");
                printf("\n");
            }
            
            printf("Battery Life: %d%%\n", status.BatteryLifePercent);
            
            if (status.BatteryLifeTime != 0xFFFFFFFF) {
                int hours = status.BatteryLifeTime / 3600;
                int mins = (status.BatteryLifeTime % 3600) / 60;
                printf("Time Remaining: %dh %dm\n", hours, mins);
            }
            
            if (status.BatteryFullLifeTime != 0xFFFFFFFF) {
                int hours = status.BatteryFullLifeTime / 3600;
                int mins = (status.BatteryFullLifeTime % 3600) / 60;
                printf("Full Life: %dh %dm\n", hours, mins);
            }
            
            printf("\nPower Saving: %s\n", (status.SystemStatusFlag & 1) ? "Active" : "Inactive");
            
            setStatus(state, true, "power");
        } else {
            setStatus(state, false, "power", "GET_STATUS_FAILED");
        }
        
        printf("\nPress any key to continue...\n");
        getch();
    } else if (args[0] == "shutdown" || args[0] == "reboot") {
        bool reboot = (args[0] == "reboot");
        int timeout = 0;
        string reason;
        
        for (size_t i = 1; i < args.size(); i++) {
            if (args[i] == "-t" && i + 1 < args.size()) {
                timeout = stoi(args[++i]);
            } else if (args[i] == "-r") {
                reboot = true;
            } else if (args[i] != "-t") {
                if (!reason.empty()) reason += " ";
                reason += args[i];
            }
        }
        
        if (reboot) {
            if (InitiateSystemShutdownA(NULL, (LPSTR)(reason.empty() ? "System reboot" : reason.c_str()), timeout, FALSE, TRUE)) {
                setStatus(state, true, "reboot", "Initiated (timeout: " + to_string(timeout) + "s)");
            } else {
                DWORD err = GetLastError();
                if (err == ERROR_NOT_ALL_ASSIGNED) {
                    setStatus(state, false, "reboot", "PRIVILEGE_REQUIRED (run as admin)");
                } else {
                    setStatus(state, false, "reboot", "ERROR " + to_string(err));
                }
            }
        } else {
            if (InitiateSystemShutdownA(NULL, (LPSTR)(reason.empty() ? "System shutdown" : reason.c_str()), timeout, FALSE, FALSE)) {
                setStatus(state, true, "shutdown", "Initiated (timeout: " + to_string(timeout) + "s)");
            } else {
                DWORD err = GetLastError();
                if (err == ERROR_NOT_ALL_ASSIGNED) {
                    setStatus(state, false, "shutdown", "PRIVILEGE_REQUIRED (run as admin)");
                } else {
                    setStatus(state, false, "shutdown", "ERROR " + to_string(err));
                }
            }
        }
    } else if (args[0] == "abortshutdown" || args[0] == "cancelshutdown") {
        if (AbortSystemShutdownA(NULL)) {
            setStatus(state, true, "abortshutdown", "Cancelled");
        } else {
            setStatus(state, false, "abortshutdown", "FAILED (no shutdown pending or admin required)");
        }
    } else if (args[0] == "logoff") {
        if (ExitWindowsEx(EWX_LOGOFF, 0)) {
            setStatus(state, true, "logoff", "Initiated");
        } else {
            setStatus(state, false, "logoff", "FAILED (admin may be required)");
        }
    } else if (args[0] == "lock") {
        if (LockWorkStation()) {
            setStatus(state, true, "lock", "Workstation locked");
        } else {
            setStatus(state, false, "lock", "FAILED");
        }
    } else if (args[0] == "monitor") {
        if (args.size() < 2) {
            setStatus(state, false, "monitor", "Usage: monitor <on|off|low>");
        } else {
            if (args[1] == "off") {
                SendMessage(HWND_BROADCAST, WM_SYSCOMMAND, SC_MONITORPOWER, 2);
                setStatus(state, true, "monitor", "Off");
            } else if (args[1] == "low" || args[1] == "standby") {
                SendMessage(HWND_BROADCAST, WM_SYSCOMMAND, SC_MONITORPOWER, 1);
                setStatus(state, true, "monitor", "Low power");
            } else if (args[1] == "on") {
                SendMessage(HWND_BROADCAST, WM_SYSCOMMAND, SC_MONITORPOWER, -1);
                setStatus(state, true, "monitor", "On");
            } else {
                setStatus(state, false, "monitor", "Unknown state: " + args[1]);
            }
        }
    } else if (args[0] == "volume" || args[0] == "vol") {
        if (args.size() < 2) {
            setStatus(state, false, "volume", "Usage: volume <get|set|mute|unmute> [level]");
        } else {
            string action = args[1];
            
            if (action == "get") {
                DWORD volume = 0;
                waveOutGetVolume(0, &volume);
                int left = LOWORD(volume) * 100 / 0xFFFF;
                int right = HIWORD(volume) * 100 / 0xFFFF;
                setStatus(state, true, "volume get", "L:" + to_string(left) + "% R:" + to_string(right) + "%");
            } else if (action == "set") {
                if (args.size() < 3) {
                    setStatus(state, false, "volume set", "Level required (0-100)");
                } else {
                    int level = stoi(args[2]);
                    if (level >= 0 && level <= 100) {
                        DWORD vol = (level * 0xFFFF / 100) | ((level * 0xFFFF / 100) << 16);
                        waveOutSetVolume(0, vol);
                        setStatus(state, true, "volume set", args[2] + "%");
                    } else {
                        setStatus(state, false, "volume set", "Invalid level (0-100)");
                    }
                }
            } else if (action == "mute" || action == "unmute") {
                setStatus(state, false, "volume", "Mute requires Core Audio API (use Windows settings)");
            } else {
                setStatus(state, false, "volume", "Unknown action: " + action);
            }
        }
    } else if (args[0] == "brightness" || args[0] == "bright") {
        if (args.size() < 2) {
            setStatus(state, false, "brightness", "Usage: brightness <get|set> [level]");
        } else {
            string action = args[1];
            
            HMODULE hDxva2 = LoadLibraryA("dxva2.dll");
            if (!hDxva2) {
                setStatus(state, false, "brightness", "DXVA2.dll not available");
            } else {
                typedef BOOL (WINAPI *GetNumberOfPhysicalMonitorsFromHMONITORPROC)(HMONITOR, LPDWORD);
                typedef struct {
                    HANDLE hPhysicalMonitor;
                    WCHAR szPhysicalMonitorDescription[128];
                } MY_PHYSICAL_MONITOR;
                typedef BOOL (WINAPI *GetPhysicalMonitorsFromHMONITORPROC)(HMONITOR, DWORD, MY_PHYSICAL_MONITOR*);
                typedef BOOL (WINAPI *GetMonitorBrightnessPROC)(HANDLE, LPDWORD, LPDWORD, LPDWORD);
                typedef BOOL (WINAPI *SetMonitorBrightnessPROC)(HANDLE, DWORD);
                typedef BOOL (WINAPI *DestroyPhysicalMonitorsPROC)(DWORD, MY_PHYSICAL_MONITOR*);
                
                auto GetNumberOfPhysicalMonitorsFromHMONITOR = (GetNumberOfPhysicalMonitorsFromHMONITORPROC)GetProcAddress(hDxva2, "GetNumberOfPhysicalMonitorsFromHMONITOR");
                auto GetPhysicalMonitorsFromHMONITOR = (GetPhysicalMonitorsFromHMONITORPROC)GetProcAddress(hDxva2, "GetPhysicalMonitorsFromHMONITOR");
                auto GetMonitorBrightness = (GetMonitorBrightnessPROC)GetProcAddress(hDxva2, "GetMonitorBrightness");
                auto SetMonitorBrightness = (SetMonitorBrightnessPROC)GetProcAddress(hDxva2, "SetMonitorBrightness");
                auto DestroyPhysicalMonitors = (DestroyPhysicalMonitorsPROC)GetProcAddress(hDxva2, "DestroyPhysicalMonitors");
                
                if (!GetNumberOfPhysicalMonitorsFromHMONITOR || !GetPhysicalMonitorsFromHMONITOR || !GetMonitorBrightness || !SetMonitorBrightness) {
                    setStatus(state, false, "brightness", "Functions not available in DXVA2.dll");
                    FreeLibrary(hDxva2);
                } else {
                    HMONITOR hMonitor = MonitorFromWindow(GetDesktopWindow(), MONITOR_DEFAULTTOPRIMARY);
                    DWORD numMonitors = 0;
                    
                    if (GetNumberOfPhysicalMonitorsFromHMONITOR(hMonitor, &numMonitors) && numMonitors > 0) {
                        MY_PHYSICAL_MONITOR* pMonitors = new MY_PHYSICAL_MONITOR[numMonitors];
                        
                        if (GetPhysicalMonitorsFromHMONITOR(hMonitor, numMonitors, pMonitors)) {
                            DWORD min, current, max;
                            
                            if (GetMonitorBrightness(pMonitors[0].hPhysicalMonitor, &min, &current, &max)) {
                                if (action == "get") {
                                    int percent = (current - min) * 100 / (max - min);
                                    setStatus(state, true, "brightness get", to_string(percent) + "%");
                                } else if (action == "set") {
                                    if (args.size() < 3) {
                                        setStatus(state, false, "brightness set", "Level required (0-100)");
                                    } else {
                                        int percent = stoi(args[2]);
                                        DWORD level = min + (max - min) * percent / 100;
                                        if (SetMonitorBrightness(pMonitors[0].hPhysicalMonitor, level)) {
                                            setStatus(state, true, "brightness set", args[2] + "%");
                                        } else {
                                            setStatus(state, false, "brightness set", "SET_FAILED");
                                        }
                                    }
                                } else {
                                    setStatus(state, false, "brightness", "Unknown action: " + action);
                                }
                            } else {
                                setStatus(state, false, "brightness", "GET_BRIGHTNESS_FAILED (monitor may not support DDC/CI)");
                            }
                            
                            DestroyPhysicalMonitors(numMonitors, pMonitors);
                        }
                        delete[] pMonitors;
                    } else {
                        setStatus(state, false, "brightness", "No physical monitors found");
                    }
                    FreeLibrary(hDxva2);
                }
            }
        }
    } else if (args[0] == "clipboard" || args[0] == "cb") {
        if (args.size() < 2) {
            setStatus(state, false, "clipboard", "Usage: clipboard <clear|getformats>");
        } else {
            string action = args[1];
            
            if (action == "clear") {
                if (OpenClipboard(NULL)) {
                    EmptyClipboard();
                    CloseClipboard();
                    setStatus(state, true, "clipboard clear");
                } else {
                    setStatus(state, false, "clipboard clear", "OPEN_FAILED");
                }
            } else if (action == "getformats" || action == "formats") {
                if (OpenClipboard(NULL)) {
                    clearScreen();
                    setCursor(0, 0);
                    printf("=== Clipboard Formats ===\n\n");
                    
                    UINT format = 0;
                    int count = 0;
                    while ((format = EnumClipboardFormats(format)) != 0) {
                        char name[256];
                        if (GetClipboardFormatNameA(format, name, sizeof(name))) {
                            printf("%5u: %s\n", format, name);
                        } else {
                            const char* stdName = NULL;
                            switch (format) {
                                case CF_TEXT: stdName = "CF_TEXT"; break;
                                case CF_BITMAP: stdName = "CF_BITMAP"; break;
                                case CF_METAFILEPICT: stdName = "CF_METAFILEPICT"; break;
                                case CF_SYLK: stdName = "CF_SYLK"; break;
                                case CF_DIF: stdName = "CF_DIF"; break;
                                case CF_TIFF: stdName = "CF_TIFF"; break;
                                case CF_OEMTEXT: stdName = "CF_OEMTEXT"; break;
                                case CF_DIB: stdName = "CF_DIB"; break;
                                case CF_PALETTE: stdName = "CF_PALETTE"; break;
                                case CF_PENDATA: stdName = "CF_PENDATA"; break;
                                case CF_RIFF: stdName = "CF_RIFF"; break;
                                case CF_WAVE: stdName = "CF_WAVE"; break;
                                case CF_UNICODETEXT: stdName = "CF_UNICODETEXT"; break;
                                case CF_ENHMETAFILE: stdName = "CF_ENHMETAFILE"; break;
                                case CF_HDROP: stdName = "CF_HDROP"; break;
                                case CF_LOCALE: stdName = "CF_LOCALE"; break;
                                case CF_DIBV5: stdName = "CF_DIBV5"; break;
                            }
                            if (stdName) {
                                printf("%5u: %s\n", format, stdName);
                            } else {
                                printf("%5u: (unknown)\n", format);
                            }
                        }
                        count++;
                    }
                    
                    CloseClipboard();
                    setStatus(state, true, "clipboard formats", to_string(count) + " formats");
                    printf("\nPress any key to continue...\n");
                    getch();
                } else {
                    setStatus(state, false, "clipboard formats", "OPEN_FAILED");
                }
            } else {
                setStatus(state, false, "clipboard", "Unknown action: " + action);
            }
        }
    } else if (args[0] == "startup") {
        if (args.size() < 2) {
            setStatus(state, false, "startup", "Usage: startup <list|add|del> [name] [command]");
        } else {
            string action = args[1];
            
            HKEY hKey;
            string regPath = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";
            
            if (action == "list") {
                if (RegOpenKeyExA(HKEY_CURRENT_USER, regPath.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
                    clearScreen();
                    setCursor(0, 0);
                    printf("=== Startup Programs ===\n\n");
                    
                    char name[256], value[1024];
                    DWORD nameSize, valueSize;
                    int index = 0;
                    
                    while (true) {
                        nameSize = sizeof(name);
                        valueSize = sizeof(value);
                        if (RegEnumValueA(hKey, index, name, &nameSize, NULL, NULL, (LPBYTE)value, &valueSize) != ERROR_SUCCESS) break;
                        printf("%-30s %s\n", name, value);
                        index++;
                    }
                    
                    RegCloseKey(hKey);
                    setStatus(state, true, "startup list", to_string(index) + " programs");
                    printf("\nPress any key to continue...\n");
                    getch();
                } else {
                    setStatus(state, false, "startup list", "REG_OPEN_FAILED");
                }
            } else if (action == "add") {
                if (args.size() < 4) {
                    setStatus(state, false, "startup add", "Usage: startup add <name> <command>");
                } else {
                    string name = args[2];
                    string cmd = args[3];
                    for (size_t i = 4; i < args.size(); i++) cmd += " " + args[i];
                    
                    if (RegOpenKeyExA(HKEY_CURRENT_USER, regPath.c_str(), 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
                        if (RegSetValueExA(hKey, name.c_str(), 0, REG_SZ, (const BYTE*)cmd.c_str(), cmd.length() + 1) == ERROR_SUCCESS) {
                            setStatus(state, true, "startup add", name);
                        } else {
                            setStatus(state, false, "startup add", "REG_SET_FAILED");
                        }
                        RegCloseKey(hKey);
                    } else {
                        setStatus(state, false, "startup add", "REG_OPEN_FAILED");
                    }
                }
            } else if (action == "del" || action == "delete") {
                if (args.size() < 3) {
                    setStatus(state, false, "startup del", "Name required");
                } else {
                    if (RegOpenKeyExA(HKEY_CURRENT_USER, regPath.c_str(), 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
                        if (RegDeleteValueA(hKey, args[2].c_str()) == ERROR_SUCCESS) {
                            setStatus(state, true, "startup del", args[2]);
                        } else {
                            setStatus(state, false, "startup del", args[2], "NOT_FOUND");
                        }
                        RegCloseKey(hKey);
                    } else {
                        setStatus(state, false, "startup del", "REG_OPEN_FAILED");
                    }
                }
            } else {
                setStatus(state, false, "startup", "Unknown action: " + action);
            }
        }
    } else if (args[0] == "sqlite" || args[0] == "sql") {
        if (args.size() < 2) {
            setStatus(state, false, "sqlite", "Usage: sqlite <dbfile> <sql|tables|schema>");
        } else {
            string dbFile = args[1];
            string sqlCmd = args.size() >= 3 ? args[2] : "";
            
            HMODULE hSqlite = LoadLibraryA("sqlite3.dll");
            if (!hSqlite) {
                setStatus(state, false, "sqlite", "sqlite3.dll not found");
            } else {
                #define SQLITE_OK 0
                #define SQLITE_ROW 100
                
                typedef struct sqlite3 sqlite3;
                typedef struct sqlite3_stmt sqlite3_stmt;
                typedef int (*sqlite3_open_t)(const char*, sqlite3**);
                typedef int (*sqlite3_close_t)(sqlite3*);
                typedef int (*sqlite3_exec_t)(sqlite3*, const char*, int(*)(void*,int,char**,char**), void*, char**);
                typedef const char* (*sqlite3_errmsg_t)(sqlite3*);
                typedef int (*sqlite3_prepare_v2_t)(sqlite3*, const char*, int, sqlite3_stmt**, const char**);
                typedef int (*sqlite3_step_t)(sqlite3_stmt*);
                typedef int (*sqlite3_column_count_t)(sqlite3_stmt*);
                typedef const char* (*sqlite3_column_text_t)(sqlite3_stmt*, int);
                typedef int (*sqlite3_finalize_t)(sqlite3_stmt*);
                typedef int (*sqlite3_table_column_metadata_t)(sqlite3*, const char*, const char*, const char*, char**, char**, int*, int*, int*);
                
                auto sqlite3_open = (sqlite3_open_t)GetProcAddress(hSqlite, "sqlite3_open");
                auto sqlite3_close = (sqlite3_close_t)GetProcAddress(hSqlite, "sqlite3_close");
                auto sqlite3_exec = (sqlite3_exec_t)GetProcAddress(hSqlite, "sqlite3_exec");
                auto sqlite3_errmsg = (sqlite3_errmsg_t)GetProcAddress(hSqlite, "sqlite3_errmsg");
                auto sqlite3_prepare_v2 = (sqlite3_prepare_v2_t)GetProcAddress(hSqlite, "sqlite3_prepare_v2");
                auto sqlite3_step = (sqlite3_step_t)GetProcAddress(hSqlite, "sqlite3_step");
                auto sqlite3_column_count = (sqlite3_column_count_t)GetProcAddress(hSqlite, "sqlite3_column_count");
                auto sqlite3_column_text = (sqlite3_column_text_t)GetProcAddress(hSqlite, "sqlite3_column_text");
                auto sqlite3_finalize = (sqlite3_finalize_t)GetProcAddress(hSqlite, "sqlite3_finalize");
                
                if (!sqlite3_open || !sqlite3_exec) {
                    setStatus(state, false, "sqlite", "Functions not found in sqlite3.dll");
                    FreeLibrary(hSqlite);
                } else {
                    sqlite3* db = NULL;
                    int rc = sqlite3_open(dbFile.c_str(), &db);
                    
                    if (rc != 0) {
                        setStatus(state, false, "sqlite", string("OPEN_FAILED: ") + (sqlite3_errmsg ? sqlite3_errmsg(db) : "unknown"));
                        if (db) sqlite3_close(db);
                        FreeLibrary(hSqlite);
                    } else {
                        if (sqlCmd == "tables") {
                            clearScreen();
                            setCursor(0, 0);
                            printf("=== Tables in %s ===\n\n", dbFile.c_str());
                            
                            char* errMsg = NULL;
                            int count = 0;
                            auto callback = [](void* data, int argc, char** argv, char** cols) -> int {
                                int* cnt = (int*)data;
                                for (int i = 0; i < argc; i++) {
                                    if (argv[i]) printf("  %s\n", argv[i]);
                                }
                                (*cnt)++;
                                return 0;
                            };
                            
                            rc = sqlite3_exec(db, "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name;", callback, &count, &errMsg);
                            if (rc != SQLITE_OK) {
                                printf("Error: %s\n", errMsg ? errMsg : "unknown");
                            }
                            
                            sqlite3_close(db);
                            FreeLibrary(hSqlite);
                            setStatus(state, true, "sqlite tables");
                            printf("\nPress any key to continue...\n");
                            getch();
                        } else if (sqlCmd == "schema") {
                            clearScreen();
                            setCursor(0, 0);
                            printf("=== Schema for %s ===\n\n", dbFile.c_str());
                            
                            char* errMsg = NULL;
                            auto callback = [](void* data, int argc, char** argv, char** cols) -> int {
                                for (int i = 0; i < argc; i++) {
                                    if (argv[i]) printf("%s\n", argv[i]);
                                }
                                return 0;
                            };
                            
                            rc = sqlite3_exec(db, "SELECT sql FROM sqlite_master WHERE sql IS NOT NULL;", callback, NULL, &errMsg);
                            
                            sqlite3_close(db);
                            FreeLibrary(hSqlite);
                            setStatus(state, true, "sqlite schema");
                            printf("\nPress any key to continue...\n");
                            getch();
                        } else if (sqlCmd.size() > 0) {
                            clearScreen();
                            setCursor(0, 0);
                            printf("=== Query Result ===\n\n");
                            
                            if (sqlite3_prepare_v2 && sqlite3_step && sqlite3_column_count && sqlite3_column_text && sqlite3_finalize) {
                                sqlite3_stmt* stmt = NULL;
                                rc = sqlite3_prepare_v2(db, sqlCmd.c_str(), -1, &stmt, NULL);
                                
                                if (rc == SQLITE_OK) {
                                    int cols = sqlite3_column_count(stmt);
                                    bool headerPrinted = false;
                                    int rowCount = 0;
                                    
                                    while (sqlite3_step(stmt) == SQLITE_ROW && rowCount < 100) {
                                        if (!headerPrinted) {
                                            for (int i = 0; i < cols; i++) {
                                                printf("%-20s", sqlite3_column_text(stmt, i) ? sqlite3_column_text(stmt, i) : "NULL");
                                            }
                                            printf("\n%s\n", string(cols * 20, '-').c_str());
                                            headerPrinted = true;
                                        }
                                        for (int i = 0; i < cols; i++) {
                                            printf("%-20s", sqlite3_column_text(stmt, i) ? sqlite3_column_text(stmt, i) : "NULL");
                                        }
                                        printf("\n");
                                        rowCount++;
                                    }
                                    sqlite3_finalize(stmt);
                                    setStatus(state, true, "sqlite query", to_string(rowCount) + " rows");
                                } else {
                                    printf("Error: %s\n", sqlite3_errmsg(db));
                                    setStatus(state, false, "sqlite query", "PREPARE_FAILED");
                                }
                            } else {
                                char* errMsg = NULL;
                                auto callback = [](void* data, int argc, char** argv, char** cols) -> int {
                                    for (int i = 0; i < argc; i++) {
                                        printf("%s = %s\n", cols[i], argv[i] ? argv[i] : "NULL");
                                    }
                                    printf("\n");
                                    return 0;
                                };
                                
                                rc = sqlite3_exec(db, sqlCmd.c_str(), callback, NULL, &errMsg);
                                if (rc != SQLITE_OK) {
                                    printf("Error: %s\n", errMsg ? errMsg : "unknown");
                                    setStatus(state, false, "sqlite query", "EXEC_FAILED");
                                } else {
                                    setStatus(state, true, "sqlite query");
                                }
                            }
                            
                            sqlite3_close(db);
                            FreeLibrary(hSqlite);
                            printf("\nPress any key to continue...\n");
                            getch();
                        } else {
                            sqlite3_close(db);
                            FreeLibrary(hSqlite);
                            setStatus(state, false, "sqlite", "Specify: tables, schema, or SQL statement");
                        }
                    }
                }
            }
        }
    } else if (args[0] == "encrypt" || args[0] == "enc") {
        if (args.size() < 3) {
            setStatus(state, false, "encrypt", "Usage: encrypt <file> <password> [output]");
        } else {
            string inputFile = args[1];
            string password = args[2];
            string outputFile = args.size() >= 4 ? args[3] : inputFile + ".enc";
            
            ifstream inFile(inputFile, ios::binary);
            if (!inFile) {
                setStatus(state, false, "encrypt", inputFile, "FILE_NOT_FOUND");
            } else {
                string content((istreambuf_iterator<char>(inFile)), istreambuf_iterator<char>());
                inFile.close();
                
                NTSTATUS status;
                BCRYPT_ALG_HANDLE hAlg = NULL;
                BCRYPT_KEY_HANDLE hKey = NULL;
                
                status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, NULL, 0);
                if (status != 0) {
                    setStatus(state, false, "encrypt", "AES_PROVIDER_FAILED");
                } else {
                    DWORD keySize = 32;
                    vector<BYTE> key(keySize);
                    
                    BCRYPT_HASH_HANDLE hHash = NULL;
                    status = BCryptOpenAlgorithmProvider(&hHash, BCRYPT_SHA256_ALGORITHM, NULL, 0);
                    if (status == 0) {
                        BCryptHashData(hHash, (PUCHAR)password.c_str(), password.size(), 0);
                        BCryptGetProperty(hHash, BCRYPT_HASH_LENGTH, (PUCHAR)&keySize, sizeof(DWORD), &keySize, 0);
                        key.resize(keySize);
                        BCryptGetProperty(hHash, BCRYPT_HASH_LENGTH, (PUCHAR)&keySize, sizeof(DWORD), &keySize, 0);
                        BCryptFinishHash(hHash, key.data(), keySize, 0);
                        BCryptDestroyHash(hHash);
                    } else {
                        for (size_t i = 0; i < key.size(); i++) {
                            key[i] = (BYTE)(password[i % password.size()] ^ (i & 0xFF));
                        }
                    }
                    
                    DWORD blockLen = 16;
                    BCryptGetProperty(hAlg, BCRYPT_BLOCK_LENGTH, (PUCHAR)&blockLen, sizeof(DWORD), &blockLen, 0);
                    
                    vector<BYTE> iv(blockLen);
                    for (DWORD i = 0; i < blockLen; i++) iv[i] = (BYTE)rand();
                    
                    status = BCryptGenerateSymmetricKey(hAlg, &hKey, NULL, 0, key.data(), key.size(), 0);
                    if (status != 0) {
                        setStatus(state, false, "encrypt", "KEY_GENERATION_FAILED");
                    } else {
                        vector<BYTE> input(content.begin(), content.end());
                        DWORD cipherLen = 0;
                        DWORD outputLen = 0;
                        
                        status = BCryptEncrypt(hKey, input.data(), input.size(), NULL, iv.data(), iv.size(), NULL, 0, &cipherLen, BCRYPT_BLOCK_PADDING);
                        if (status == 0) {
                            vector<BYTE> cipher(cipherLen);
                            status = BCryptEncrypt(hKey, input.data(), input.size(), NULL, iv.data(), iv.size(), cipher.data(), cipherLen, &outputLen, BCRYPT_BLOCK_PADDING);
                            
                            if (status == 0) {
                                ofstream outFile(outputFile, ios::binary);
                                outFile.write((char*)iv.data(), iv.size());
                                outFile.write((char*)cipher.data(), outputLen);
                                outFile.close();
                                
                                setStatus(state, true, "encrypt", inputFile + " -> " + outputFile);
                            } else {
                                setStatus(state, false, "encrypt", "ENCRYPTION_FAILED");
                            }
                        } else {
                            setStatus(state, false, "encrypt", "ENCRYPTION_CALC_FAILED");
                        }
                        
                        BCryptDestroyKey(hKey);
                    }
                    
                    BCryptCloseAlgorithmProvider(hAlg, 0);
                }
            }
        }
    } else if (args[0] == "decrypt" || args[0] == "dec") {
        if (args.size() < 3) {
            setStatus(state, false, "decrypt", "Usage: decrypt <file> <password> [output]");
        } else {
            string inputFile = args[1];
            string password = args[2];
            string outputFile = args.size() >= 4 ? args[3] : inputFile + ".dec";
            
            ifstream inFile(inputFile, ios::binary);
            if (!inFile) {
                setStatus(state, false, "decrypt", inputFile, "FILE_NOT_FOUND");
            } else {
                inFile.seekg(0, ios::end);
                size_t fileSize = inFile.tellg();
                inFile.seekg(0, ios::beg);
                
                DWORD blockLen = 16;
                vector<BYTE> iv(blockLen);
                inFile.read((char*)iv.data(), blockLen);
                
                vector<BYTE> cipher(fileSize - blockLen);
                inFile.read((char*)cipher.data(), cipher.size());
                inFile.close();
                
                NTSTATUS status;
                BCRYPT_ALG_HANDLE hAlg = NULL;
                BCRYPT_KEY_HANDLE hKey = NULL;
                
                status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, NULL, 0);
                if (status != 0) {
                    setStatus(state, false, "decrypt", "AES_PROVIDER_FAILED");
                } else {
                    DWORD keySize = 32;
                    vector<BYTE> key(keySize);
                    
                    BCRYPT_HASH_HANDLE hHash = NULL;
                    status = BCryptOpenAlgorithmProvider(&hHash, BCRYPT_SHA256_ALGORITHM, NULL, 0);
                    if (status == 0) {
                        BCryptHashData(hHash, (PUCHAR)password.c_str(), password.size(), 0);
                        BCryptGetProperty(hHash, BCRYPT_HASH_LENGTH, (PUCHAR)&keySize, sizeof(DWORD), &keySize, 0);
                        key.resize(keySize);
                        BCryptGetProperty(hHash, BCRYPT_HASH_LENGTH, (PUCHAR)&keySize, sizeof(DWORD), &keySize, 0);
                        BCryptFinishHash(hHash, key.data(), keySize, 0);
                        BCryptDestroyHash(hHash);
                    } else {
                        for (size_t i = 0; i < key.size(); i++) {
                            key[i] = (BYTE)(password[i % password.size()] ^ (i & 0xFF));
                        }
                    }
                    
                    status = BCryptGenerateSymmetricKey(hAlg, &hKey, NULL, 0, key.data(), key.size(), 0);
                    if (status != 0) {
                        setStatus(state, false, "decrypt", "KEY_GENERATION_FAILED");
                    } else {
                        DWORD plainLen = 0;
                        DWORD outputLen = 0;
                        
                        status = BCryptDecrypt(hKey, cipher.data(), cipher.size(), NULL, iv.data(), iv.size(), NULL, 0, &plainLen, BCRYPT_BLOCK_PADDING);
                        if (status == 0) {
                            vector<BYTE> plain(plainLen);
                            status = BCryptDecrypt(hKey, cipher.data(), cipher.size(), NULL, iv.data(), iv.size(), plain.data(), plainLen, &outputLen, BCRYPT_BLOCK_PADDING);
                            
                            if (status == 0) {
                                ofstream outFile(outputFile, ios::binary);
                                outFile.write((char*)plain.data(), outputLen);
                                outFile.close();
                                
                                setStatus(state, true, "decrypt", inputFile + " -> " + outputFile);
                            } else {
                                setStatus(state, false, "decrypt", "DECRYPTION_FAILED (wrong password?)");
                            }
                        } else {
                            setStatus(state, false, "decrypt", "DECRYPTION_CALC_FAILED");
                        }
                        
                        BCryptDestroyKey(hKey);
                    }
                    
                    BCryptCloseAlgorithmProvider(hAlg, 0);
                }
            }
        }
    } else if (args[0] == "aes" || args[0] == "aes256") {
        if (args.size() < 4) {
            setStatus(state, false, "aes", "Usage: aes <enc|dec> <file> <password> [output]");
        } else {
            string action = args[1];
            string inputFile = args[2];
            string password = args[3];
            string outputFile = args.size() >= 5 ? args[4] : "";
            
            if (action == "enc" || action == "encrypt") {
                if (outputFile.empty()) outputFile = inputFile + ".enc";
                
                ifstream inFile(inputFile, ios::binary);
                if (!inFile) {
                    setStatus(state, false, "aes enc", inputFile, "FILE_NOT_FOUND");
                } else {
                    string content((istreambuf_iterator<char>(inFile)), istreambuf_iterator<char>());
                    inFile.close();
                    
                    NTSTATUS status;
                    BCRYPT_ALG_HANDLE hAlg = NULL;
                    BCRYPT_KEY_HANDLE hKey = NULL;
                    
                    status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, NULL, 0);
                    if (status == 0) {
                        DWORD keySize = 32;
                        vector<BYTE> key(keySize);
                        
                        BCRYPT_HASH_HANDLE hHash = NULL;
                        if (BCryptOpenAlgorithmProvider(&hHash, BCRYPT_SHA256_ALGORITHM, NULL, 0) == 0) {
                            BCryptHashData(hHash, (PUCHAR)password.c_str(), password.size(), 0);
                            BCryptGetProperty(hHash, BCRYPT_HASH_LENGTH, (PUCHAR)&keySize, sizeof(DWORD), &keySize, 0);
                            key.resize(keySize);
                            BCryptFinishHash(hHash, key.data(), keySize, 0);
                            BCryptDestroyHash(hHash);
                        }
                        
                        DWORD blockLen = 16;
                        vector<BYTE> iv(blockLen);
                        for (DWORD i = 0; i < blockLen; i++) iv[i] = (BYTE)rand();
                        
                        if (BCryptGenerateSymmetricKey(hAlg, &hKey, NULL, 0, key.data(), key.size(), 0) == 0) {
                            vector<BYTE> input(content.begin(), content.end());
                            DWORD cipherLen = 0;
                            
                            if (BCryptEncrypt(hKey, input.data(), input.size(), NULL, iv.data(), iv.size(), NULL, 0, &cipherLen, BCRYPT_BLOCK_PADDING) == 0) {
                                vector<BYTE> cipher(cipherLen);
                                DWORD outputLen = 0;
                                if (BCryptEncrypt(hKey, input.data(), input.size(), NULL, iv.data(), iv.size(), cipher.data(), cipherLen, &outputLen, BCRYPT_BLOCK_PADDING) == 0) {
                                    ofstream outFile(outputFile, ios::binary);
                                    outFile.write((char*)iv.data(), iv.size());
                                    outFile.write((char*)cipher.data(), outputLen);
                                    outFile.close();
                                    setStatus(state, true, "aes enc", inputFile + " -> " + outputFile);
                                }
                            }
                            BCryptDestroyKey(hKey);
                        }
                        BCryptCloseAlgorithmProvider(hAlg, 0);
                    }
                }
            } else if (action == "dec" || action == "decrypt") {
                if (outputFile.empty()) outputFile = inputFile + ".dec";
                
                ifstream inFile(inputFile, ios::binary);
                if (!inFile) {
                    setStatus(state, false, "aes dec", inputFile, "FILE_NOT_FOUND");
                } else {
                    inFile.seekg(0, ios::end);
                    size_t fileSize = inFile.tellg();
                    inFile.seekg(0, ios::beg);
                    
                    DWORD blockLen = 16;
                    vector<BYTE> iv(blockLen);
                    inFile.read((char*)iv.data(), blockLen);
                    
                    vector<BYTE> cipher(fileSize - blockLen);
                    inFile.read((char*)cipher.data(), cipher.size());
                    inFile.close();
                    
                    NTSTATUS status;
                    BCRYPT_ALG_HANDLE hAlg = NULL;
                    BCRYPT_KEY_HANDLE hKey = NULL;
                    
                    status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, NULL, 0);
                    if (status == 0) {
                        DWORD keySize = 32;
                        vector<BYTE> key(keySize);
                        
                        BCRYPT_HASH_HANDLE hHash = NULL;
                        if (BCryptOpenAlgorithmProvider(&hHash, BCRYPT_SHA256_ALGORITHM, NULL, 0) == 0) {
                            BCryptHashData(hHash, (PUCHAR)password.c_str(), password.size(), 0);
                            BCryptGetProperty(hHash, BCRYPT_HASH_LENGTH, (PUCHAR)&keySize, sizeof(DWORD), &keySize, 0);
                            key.resize(keySize);
                            BCryptFinishHash(hHash, key.data(), keySize, 0);
                            BCryptDestroyHash(hHash);
                        }
                        
                        if (BCryptGenerateSymmetricKey(hAlg, &hKey, NULL, 0, key.data(), key.size(), 0) == 0) {
                            DWORD plainLen = 0;
                            
                            if (BCryptDecrypt(hKey, cipher.data(), cipher.size(), NULL, iv.data(), iv.size(), NULL, 0, &plainLen, BCRYPT_BLOCK_PADDING) == 0) {
                                vector<BYTE> plain(plainLen);
                                DWORD outputLen = 0;
                                if (BCryptDecrypt(hKey, cipher.data(), cipher.size(), NULL, iv.data(), iv.size(), plain.data(), plainLen, &outputLen, BCRYPT_BLOCK_PADDING) == 0) {
                                    ofstream outFile(outputFile, ios::binary);
                                    outFile.write((char*)plain.data(), outputLen);
                                    outFile.close();
                                    setStatus(state, true, "aes dec", inputFile + " -> " + outputFile);
                                } else {
                                    setStatus(state, false, "aes dec", "DECRYPTION_FAILED (wrong password?)");
                                }
                            }
                            BCryptDestroyKey(hKey);
                        }
                        BCryptCloseAlgorithmProvider(hAlg, 0);
                    }
                }
            } else {
                setStatus(state, false, "aes", "Unknown action: " + action);
            }
        }
    } else if (args[0] == "base64" || args[0] == "b64") {
        if (args.size() < 2) {
            setStatus(state, false, "base64", "Usage: base64 <enc|dec> <text|file> [-f]");
        } else {
            string action = args[1];
            bool isFile = false;
            string input;
            
            for (size_t i = 2; i < args.size(); i++) {
                if (args[i] == "-f") isFile = true;
                else {
                    if (!input.empty()) input += " ";
                    input += args[i];
                }
            }
            
            if (action == "enc" || action == "encode") {
                string data = input;
                if (isFile) {
                    ifstream f(input, ios::binary);
                    if (!f) {
                        setStatus(state, false, "base64", input, "FILE_NOT_FOUND");
                    } else {
                        data = string((istreambuf_iterator<char>(f)), istreambuf_iterator<char>());
                    }
                }
                
                if (!data.empty()) {
                    DWORD encodedLen = 0;
                    CryptBinaryToStringA((BYTE*)data.data(), data.size(), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, NULL, &encodedLen);
                    string encoded(encodedLen, 0);
                    CryptBinaryToStringA((BYTE*)data.data(), data.size(), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &encoded[0], &encodedLen);
                    encoded.resize(strlen(encoded.c_str()));
                    setStatus(state, true, "base64 enc", encoded.substr(0, 100) + (encoded.size() > 100 ? "..." : ""));
                }
            } else if (action == "dec" || action == "decode") {
                string data = input;
                if (isFile) {
                    ifstream f(input, ios::binary);
                    if (!f) {
                        setStatus(state, false, "base64", input, "FILE_NOT_FOUND");
                    } else {
                        data = string((istreambuf_iterator<char>(f)), istreambuf_iterator<char>());
                    }
                }
                
                if (!data.empty()) {
                    DWORD decodedLen = 0;
                    CryptStringToBinaryA(data.c_str(), 0, CRYPT_STRING_BASE64, NULL, &decodedLen, NULL, NULL);
                    string decoded(decodedLen, 0);
                    CryptStringToBinaryA(data.c_str(), 0, CRYPT_STRING_BASE64, (BYTE*)&decoded[0], &decodedLen, NULL, NULL);
                    setStatus(state, true, "base64 dec", decoded.substr(0, 100) + (decoded.size() > 100 ? "..." : ""));
                }
            } else {
                setStatus(state, false, "base64", "Unknown action: " + action);
            }
        }
    } else if (args[0] == "ssh") {
        if (args.size() < 2) {
            setStatus(state, false, "ssh", "Usage: ssh <user@host[:port]> [-k keyfile] [-p password]");
        } else {
            string target = args[1];
            string keyFile;
            string password;
            
            for (size_t i = 2; i < args.size(); i++) {
                if (args[i] == "-k" && i + 1 < args.size()) keyFile = args[++i];
                else if (args[i] == "-p" && i + 1 < args.size()) password = args[++i];
            }
            
            size_t atPos = target.find('@');
            size_t colonPos = target.find(':', atPos == string::npos ? 0 : atPos);
            
            string user = atPos != string::npos ? target.substr(0, atPos) : "root";
            string host = atPos != string::npos ? target.substr(atPos + 1) : target;
            int port = 22;
            
            if (colonPos != string::npos) {
                port = stoi(host.substr(colonPos - (atPos != string::npos ? atPos + 1 : 0) + 1));
                host = host.substr(0, colonPos - (atPos != string::npos ? atPos + 1 : 0));
            }
            
            setStatus(state, false, "ssh", "SSH requires libssh2.dll - use: plink " + user + "@" + host + " -P " + to_string(port));
        }
    } else if (args[0] == "telnet" || args[0] == "tn") {
        if (args.size() < 2) {
            setStatus(state, false, "telnet", "Usage: telnet <host[:port]>");
        } else {
            string target = args[1];
            size_t colonPos = target.find(':');
            string host = colonPos != string::npos ? target.substr(0, colonPos) : target;
            int port = colonPos != string::npos ? stoi(target.substr(colonPos + 1)) : 23;
            
            WSADATA wsaData;
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
                setStatus(state, false, "telnet", "WSA_STARTUP_FAILED");
            } else {
                SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                if (sock == INVALID_SOCKET) {
                    setStatus(state, false, "telnet", "SOCKET_CREATE_FAILED");
                    WSACleanup();
                } else {
                    struct sockaddr_in server;
                    server.sin_family = AF_INET;
                    server.sin_port = htons(port);
                    
                    struct hostent* he = gethostbyname(host.c_str());
                    if (!he) {
                        setStatus(state, false, "telnet", host, "DNS_FAILED");
                        closesocket(sock);
                        WSACleanup();
                    } else {
                        memcpy(&server.sin_addr, he->h_addr_list[0], he->h_length);
                        
                        setStatus(state, false, "telnet", "Connecting to " + host + ":" + to_string(port) + "...");
                        
                        if (connect(sock, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
                            setStatus(state, false, "telnet", "CONNECT_FAILED: " + to_string(WSAGetLastError()));
                        } else {
                            setStatus(state, true, "telnet", "Connected to " + host + ":" + to_string(port));
                            printf("Connected to %s:%d\n", host.c_str(), port);
                            printf("Press any key to disconnect...\n");
                            getch();
                        }
                        closesocket(sock);
                        WSACleanup();
                    }
                }
            }
        }
    } else if (args[0] == "rand" || args[0] == "random") {
        if (args.size() < 2) {
            setStatus(state, false, "rand", "Usage: rand <bytes|hex|base64> [length]");
        } else {
            string format = args[1];
            int length = args.size() >= 3 ? stoi(args[2]) : 32;
            
            if (length <= 0 || length > 4096) {
                setStatus(state, false, "rand", "Length must be 1-4096");
            } else {
                vector<BYTE> buffer(length);
                NTSTATUS status = BCryptGenRandom(NULL, buffer.data(), length, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
                
                if (status != 0) {
                    HCRYPTPROV hProv = 0;
                    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
                        setStatus(state, false, "rand", "RNG_INIT_FAILED");
                    } else {
                        CryptGenRandom(hProv, length, buffer.data());
                        CryptReleaseContext(hProv, 0);
                    }
                }
                
                if (format == "hex") {
                    string hex;
                    for (BYTE b : buffer) {
                        char buf[3];
                        sprintf(buf, "%02x", b);
                        hex += buf;
                    }
                    setStatus(state, true, "rand hex", hex.substr(0, 64) + (hex.size() > 64 ? "..." : ""));
                } else if (format == "base64" || format == "b64") {
                    DWORD encodedLen = 0;
                    CryptBinaryToStringA(buffer.data(), buffer.size(), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, NULL, &encodedLen);
                    string encoded(encodedLen, 0);
                    CryptBinaryToStringA(buffer.data(), buffer.size(), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &encoded[0], &encodedLen);
                    encoded.resize(strlen(encoded.c_str()));
                    setStatus(state, true, "rand base64", encoded.substr(0, 64) + (encoded.size() > 64 ? "..." : ""));
                } else {
                    setStatus(state, true, "rand bytes", to_string(length) + " bytes generated");
                }
            }
        }
    } else if (args[0] == "genkey" || args[0] == "keygen") {
        if (args.size() < 2) {
            setStatus(state, false, "genkey", "Usage: genkey <rsa|aes> [bits] [output]");
        } else {
            string type = args[1];
            int bits = args.size() >= 3 ? stoi(args[2]) : 2048;
            string outputFile = args.size() >= 4 ? args[3] : "";
            
            if (type == "aes" || type == "symmetric") {
                int keyLen = bits / 8;
                if (keyLen <= 0 || keyLen > 64) keyLen = 32;
                
                vector<BYTE> key(keyLen);
                NTSTATUS status = BCryptGenRandom(NULL, key.data(), keyLen, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
                
                if (status != 0) {
                    setStatus(state, false, "genkey", "RNG_FAILED");
                } else {
                    string hex;
                    for (BYTE b : key) {
                        char buf[3];
                        sprintf(buf, "%02x", b);
                        hex += buf;
                    }
                    
                    if (!outputFile.empty()) {
                        ofstream f(outputFile);
                        f << hex;
                        f.close();
                        setStatus(state, true, "genkey aes", "Key saved to " + outputFile);
                    } else {
                        setStatus(state, true, "genkey aes", hex.substr(0, 64) + (hex.size() > 64 ? "..." : ""));
                    }
                }
            } else if (type == "rsa") {
                NCRYPT_KEY_HANDLE hKey = 0;
                NCRYPT_PROV_HANDLE hProv = 0;
                
                SECURITY_STATUS status = NCryptOpenStorageProvider(&hProv, MS_KEY_STORAGE_PROVIDER, 0);
                if (status != ERROR_SUCCESS) {
                    setStatus(state, false, "genkey", "KEY_STORAGE_PROVIDER_FAILED");
                } else {
                    wchar_t keyName[256];
                    swprintf(keyName, 256, L"TempRSAKey_%d", GetTickCount());
                    
                    status = NCryptCreatePersistedKey(hProv, &hKey, NCRYPT_RSA_ALGORITHM, keyName, 0, 0);
                    if (status != ERROR_SUCCESS) {
                        setStatus(state, false, "genkey", "CREATE_KEY_FAILED: " + to_string(status));
                    } else {
                        DWORD keySize = bits;
                        NCryptSetProperty(hKey, NCRYPT_LENGTH_PROPERTY, (PBYTE)&keySize, sizeof(DWORD), 0);
                        
                        status = NCryptFinalizeKey(hKey, 0);
                        if (status != ERROR_SUCCESS) {
                            setStatus(state, false, "genkey", "FINALIZE_KEY_FAILED: " + to_string(status));
                        } else {
                            setStatus(state, true, "genkey rsa", to_string(bits) + "-bit key created");
                            NCryptDeleteKey(hKey, 0);
                        }
                    }
                    NCryptFreeObject(hProv);
                }
            } else {
                setStatus(state, false, "genkey", "Unknown type: " + type + " (use: rsa, aes)");
            }
        }
    } else if (args[0] == "assoc") {
        if (args.size() < 2) {
            setStatus(state, false, "assoc", "Usage: assoc <.ext> [filetype]");
        } else {
            string ext = args[1];
            if (ext[0] != '.') ext = "." + ext;
            
            if (args.size() >= 3) {
                string fileType = args[2];
                if (RegSetValueA(HKEY_CLASSES_ROOT, ext.c_str(), REG_SZ, fileType.c_str(), 0) == ERROR_SUCCESS) {
                    setStatus(state, true, "assoc", ext + " = " + fileType);
                } else {
                    setStatus(state, false, "assoc", "SET_FAILED (admin required)");
                }
            } else {
                char fileType[256];
                DWORD size = sizeof(fileType);
                if (RegGetValueA(HKEY_CLASSES_ROOT, ext.c_str(), NULL, RRF_RT_REG_SZ, NULL, fileType, &size) == ERROR_SUCCESS) {
                    setStatus(state, true, "assoc", ext + " = " + string(fileType));
                } else {
                    setStatus(state, false, "assoc", ext, "NOT_ASSOCIATED");
                }
            }
        }
    } else if (args[0] == "ftype") {
        if (args.size() < 2) {
            setStatus(state, false, "ftype", "Usage: ftype <filetype> [command]");
        } else {
            string fileType = args[1];
            
            if (args.size() >= 3) {
                string cmd;
                for (size_t i = 2; i < args.size(); i++) {
                    if (i > 2) cmd += " ";
                    cmd += args[i];
                }
                
                string keyPath = fileType + "\\shell\\open\\command";
                if (RegSetValueA(HKEY_CLASSES_ROOT, keyPath.c_str(), REG_SZ, cmd.c_str(), 0) == ERROR_SUCCESS) {
                    setStatus(state, true, "ftype", fileType + " = " + cmd);
                } else {
                    setStatus(state, false, "ftype", "SET_FAILED (admin required)");
                }
            } else {
                char cmd[1024];
                DWORD size = sizeof(cmd);
                string keyPath = fileType + "\\shell\\open\\command";
                if (RegGetValueA(HKEY_CLASSES_ROOT, keyPath.c_str(), NULL, RRF_RT_REG_SZ, NULL, cmd, &size) == ERROR_SUCCESS) {
                    setStatus(state, true, "ftype", fileType + " = " + string(cmd));
                } else {
                    setStatus(state, false, "ftype", fileType, "NOT_FOUND");
                }
            }
        }
    } else if (args[0] == "ver" || args[0] == "version") {
        OSVERSIONINFOEXA osvi;
        ZeroMemory(&osvi, sizeof(osvi));
        osvi.dwOSVersionInfoSize = sizeof(osvi);
        
        typedef NTSTATUS (WINAPI *RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
        HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
        if (hNtdll) {
            auto pRtlGetVersion = (RtlGetVersionPtr)GetProcAddress(hNtdll, "RtlGetVersion");
            if (pRtlGetVersion) {
                pRtlGetVersion((PRTL_OSVERSIONINFOW)&osvi);
            }
        }
        
        char verStr[256];
        sprintf(verStr, "Windows %lu.%lu.%lu", osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber);
        setStatus(state, true, "ver", string(verStr));
    } else if (args[0] == "drivers") {
        clearScreen();
        setCursor(0, 0);
        printf("=== Loaded Drivers ===\n\n");
        
        LPVOID pDrivers[1024];
        DWORD cbNeeded;
        
        if (EnumDeviceDrivers(pDrivers, sizeof(pDrivers), &cbNeeded)) {
            int count = cbNeeded / sizeof(LPVOID);
            char driverName[MAX_PATH];
            
            printf("%-50s\n", string(50, '-').c_str());
            
            for (int i = 0; i < count && i < 50; i++) {
                if (GetDeviceDriverBaseNameA(pDrivers[i], driverName, sizeof(driverName))) {
                    printf("  %s\n", driverName);
                }
            }
            
            setStatus(state, true, "drivers", to_string(count) + " drivers loaded");
        } else {
            setStatus(state, false, "drivers", "ENUM_FAILED");
        }
        
        printf("\nPress any key to continue...\n");
        getch();
    } else {
        setStatus(state, false, args[0], "COMMAND_NOT_FOUND");
    }
    
    state->cmdHistory.push_back(wcmd);
    state->cmdHistory.push_back(state->statusMsg);
    while (state->cmdHistory.size() > 200) {
        state->cmdHistory.erase(state->cmdHistory.begin());
        state->cmdHistory.erase(state->cmdHistory.begin());
    }
}

void renderHelp(AppState* state) {
    COORD size = getConsoleSize();
    int width = size.X;
    int height = size.Y;
    
    clearScreen();
    
    vector<wstring> helpLines;
    helpLines.push_back(L"=== HELP - Windows Command Shell ===");
    helpLines.push_back(L"");
    helpLines.push_back(L"[Navigation]");
    helpLines.push_back(L"  cd <path>          - Change directory (supports %ENV%)");
    helpLines.push_back(L"  pwd                - Print working directory");
    helpLines.push_back(L"  ls / xdir          - List directory with details");
    helpLines.push_back(L"  tree [path]        - Show directory tree");
    helpLines.push_back(L"  run <file>         - Open file with default program");
    helpLines.push_back(L"  open [path]        - Open in Windows Explorer");
    helpLines.push_back(L"");
    helpLines.push_back(L"[File Operations]");
    helpLines.push_back(L"  copy <src> <dst>   - Copy file");
    helpLines.push_back(L"  move <src> <dst>   - Move/rename file or directory");
    helpLines.push_back(L"  ren <old> <new>    - Rename (use -b for batch)");
    helpLines.push_back(L"  mkdir <name>       - Create directory");
    helpLines.push_back(L"  rm <target>        - Delete file or empty directory");
    helpLines.push_back(L"  touch <file>       - Create or update file timestamp");
    helpLines.push_back(L"  edit <file>        - Edit file in Notepad");
    helpLines.push_back(L"  attrib [flags] <file> - View/change file attributes");
    helpLines.push_back(L"    Flags: +r/-r (readonly), +h/-h (hidden)");
    helpLines.push_back(L"           +s/-s (system), +a/-a (archive)");
    helpLines.push_back(L"");
    helpLines.push_back(L"[File Content]");
    helpLines.push_back(L"  cat <file>         - Display file content");
    helpLines.push_back(L"  head <file> [n]    - Show first n lines (default 10)");
    helpLines.push_back(L"  tail <file> [n]    - Show last n lines (default 10)");
    helpLines.push_back(L"  grep <pattern> <file> [-i] - Search in file");
    helpLines.push_back(L"  find <pattern> [path] - Find files by name");
    helpLines.push_back(L"  wc <file>          - Count lines/words/characters");
    helpLines.push_back(L"  sort <file>        - Sort file lines");
    helpLines.push_back(L"  uniq <file>        - Count unique lines");
    helpLines.push_back(L"  diff <file1> <file2> - Compare two files");
    helpLines.push_back(L"");
    helpLines.push_back(L"[File Unlock]");
    helpLines.push_back(L"  unlock <file/dir>  - Unlock locked file or directory");
    helpLines.push_back(L"  smash <file/dir>   - Force delete file or directory");
    helpLines.push_back(L"");
    helpLines.push_back(L"[7-Zip Archive]");
    helpLines.push_back(L"  7z <a|x> <archive> [files] - a: compress, x: extract");
    helpLines.push_back(L"  7zlist <archive>          - List archive contents");
    helpLines.push_back(L"  7zextract <archive> [dir] - Extract to directory");
    helpLines.push_back(L"    Supports: 7z, zip, gzip, bzip2, xz, tar, wim");
    helpLines.push_back(L"");
    helpLines.push_back(L"[Download]");
    helpLines.push_back(L"  download <url> [filename] - Background download");
    helpLines.push_back(L"  dl <url> [filename]       - Blocking download");
    helpLines.push_back(L"  dlstatus                  - Show download status");
    helpLines.push_back(L"  dlhistory                 - Show download history");
    helpLines.push_back(L"  curl <url> [-o file]      - HTTP request");
    helpLines.push_back(L"    Options: -X METHOD, -d DATA, -i (show headers)");
    helpLines.push_back(L"");
    helpLines.push_back(L"[Network]");
    helpLines.push_back(L"  ipconfig           - Show IP configuration");
    helpLines.push_back(L"  ping <host>        - Ping a host (4 packets)");
    helpLines.push_back(L"  netstat            - Show active TCP/UDP connections");
    helpLines.push_back(L"  dig <hostname>     - DNS lookup");
    helpLines.push_back(L"  share              - List network shares");
    helpLines.push_back(L"");
    helpLines.push_back(L"[Process Management]");
    helpLines.push_back(L"  ps / tasklist      - List running processes");
    helpLines.push_back(L"  kill <pid|name>    - Kill process by PID or name");
    helpLines.push_back(L"");
    helpLines.push_back(L"[Registry]");
    helpLines.push_back(L"  reg get <key> [val]    - Get registry value");
    helpLines.push_back(L"  reg set <key> <val> <data> - Set registry value");
    helpLines.push_back(L"  reg del <key> [val]    - Delete registry key/value");
    helpLines.push_back(L"  reg list <key>         - List registry subkeys");
    helpLines.push_back(L"");
    helpLines.push_back(L"[Services]");
    helpLines.push_back(L"  service list           - List all services");
    helpLines.push_back(L"  service <start|stop|restart> <name> - Control service");
    helpLines.push_back(L"  service query <name>   - Query service status");
    helpLines.push_back(L"  sc <list|start|stop|query> - Service control (short)");
    helpLines.push_back(L"");
    helpLines.push_back(L"[Windows Management]");
    helpLines.push_back(L"  window list            - List all windows");
    helpLines.push_back(L"  window <close|min|max|focus> <title> - Window control");
    helpLines.push_back(L"  screenshot [file]      - Capture screenshot");
    helpLines.push_back(L"  scr [file]             - Screenshot (short)");
    helpLines.push_back(L"");
    helpLines.push_back(L"[Performance & System]");
    helpLines.push_back(L"  perf                   - Show performance info");
    helpLines.push_back(L"  drivers                - List loaded drivers");
    helpLines.push_back(L"  startup                - List startup programs");
    helpLines.push_back(L"  ver                    - Show Windows version");
    helpLines.push_back(L"");
    helpLines.push_back(L"[User Management]");
    helpLines.push_back(L"  user list              - List local users");
    helpLines.push_back(L"  user info <name>       - Show user info");
    helpLines.push_back(L"");
    helpLines.push_back(L"[Firewall & Security]");
    helpLines.push_back(L"  firewall status        - Show firewall status");
    helpLines.push_back(L"  fw status              - Firewall (short)");
    helpLines.push_back(L"  eventlog list          - List event logs");
    helpLines.push_back(L"  eventlog read [log] [n] - Read event log entries");
    helpLines.push_back(L"  elog <list|read>       - Event log (short)");
    helpLines.push_back(L"");
    helpLines.push_back(L"[Power & System Control]");
    helpLines.push_back(L"  power                  - Show power/battery status");
    helpLines.push_back(L"  shutdown [-t sec] [reason] - Shutdown system");
    helpLines.push_back(L"  reboot [-t sec] [reason]   - Reboot system");
    helpLines.push_back(L"  abortshutdown          - Cancel pending shutdown");
    helpLines.push_back(L"  logoff                 - Log off current user");
    helpLines.push_back(L"  lock                   - Lock workstation");
    helpLines.push_back(L"  monitor <on|off|low>   - Control monitor power");
    helpLines.push_back(L"");
    helpLines.push_back(L"[Audio & Display]");
    helpLines.push_back(L"  volume get             - Get volume level");
    helpLines.push_back(L"  volume set <0-100>     - Set volume level");
    helpLines.push_back(L"  vol <get|set>          - Volume (short)");
    helpLines.push_back(L"  brightness get         - Get monitor brightness");
    helpLines.push_back(L"  brightness set <0-100> - Set monitor brightness");
    helpLines.push_back(L"");
    helpLines.push_back(L"[Text-to-Speech]");
    helpLines.push_back(L"  tts <text>             - Speak text aloud");
    helpLines.push_back(L"  speak <text>           - TTS (alternative)");
    helpLines.push_back(L"");
    helpLines.push_back(L"[File Associations]");
    helpLines.push_back(L"  assoc [.ext]           - Show file associations");
    helpLines.push_back(L"  ftype <type>           - Show file type command");
    helpLines.push_back(L"");
    helpLines.push_back(L"[Clipboard Extended]");
    helpLines.push_back(L"  clipboard clear        - Clear clipboard");
    helpLines.push_back(L"  clipboard formats      - List clipboard formats");
    helpLines.push_back(L"  cb <clear|formats>     - Clipboard (short)");
    helpLines.push_back(L"");
    helpLines.push_back(L"[System Information]");
    helpLines.push_back(L"  sysinfo            - Show system information");
    helpLines.push_back(L"  du [path]          - Directory size analysis");
    helpLines.push_back(L"  df / diskfree      - Show disk space");
    helpLines.push_back(L"  whoami             - Show current user");
    helpLines.push_back(L"  hostname           - Show computer name");
    helpLines.push_back(L"  time               - Show current time");
    helpLines.push_back(L"  date               - Show current date");
    helpLines.push_back(L"  uptime             - Show system uptime");
    helpLines.push_back(L"");
    helpLines.push_back(L"[Hash & Security]");
    helpLines.push_back(L"  hash <file> [algo] - Calculate hash (md5/sha1/sha256)");
    helpLines.push_back(L"  md5 <file>         - Calculate MD5 hash");
    helpLines.push_back(L"  sha1 <file>        - Calculate SHA1 hash");
    helpLines.push_back(L"  sha256 <file>      - Calculate SHA256 hash");
    helpLines.push_back(L"");
    helpLines.push_back(L"[Encryption (AES-256)]");
    helpLines.push_back(L"  encrypt <file> <pass> [out] - Encrypt file with AES-256");
    helpLines.push_back(L"  decrypt <file> <pass> [out] - Decrypt file");
    helpLines.push_back(L"  aes enc|dec <file> <pass>   - AES shorthand");
    helpLines.push_back(L"  base64 enc|dec <text> [-f]  - Base64 encode/decode");
    helpLines.push_back(L"  b64 enc|dec <text>          - Base64 (short)");
    helpLines.push_back(L"  rand hex|base64 [len]       - Generate random bytes");
    helpLines.push_back(L"  genkey aes|rsa [bits]       - Generate encryption key");
    helpLines.push_back(L"");
    helpLines.push_back(L"[SQLite Database]");
    helpLines.push_back(L"  sqlite <db> tables    - List tables in database");
    helpLines.push_back(L"  sqlite <db> schema    - Show database schema");
    helpLines.push_back(L"  sqlite <db> <sql>     - Execute SQL query");
    helpLines.push_back(L"  sql <db> <sql>        - SQLite (short)");
    helpLines.push_back(L"");
    helpLines.push_back(L"[Network Tools]");
    helpLines.push_back(L"  telnet <host[:port]>  - Connect to host");
    helpLines.push_back(L"  tn <host[:port]>      - Telnet (short)");
    helpLines.push_back(L"  ssh <user@host>       - SSH connection info");
    helpLines.push_back(L"");
    helpLines.push_back(L"[Clipboard]");
    helpLines.push_back(L"  clip <text>        - Copy text to clipboard");
    helpLines.push_back(L"  clip -f <file>     - Copy file content to clipboard");
    helpLines.push_back(L"  paste [file]       - Paste from clipboard");
    helpLines.push_back(L"");
    helpLines.push_back(L"[Environment Variables]");
    helpLines.push_back(L"  set <VAR>=<val>    - Set environment variable");
    helpLines.push_back(L"  set <VAR>          - Show variable value");
    helpLines.push_back(L"  get <VAR>          - Get variable value");
    helpLines.push_back(L"  env                - List all environment variables");
    helpLines.push_back(L"");
    helpLines.push_back(L"[Other]");
    helpLines.push_back(L"  echo <text>        - Print text");
    helpLines.push_back(L"  which <command>    - Find command location");
    helpLines.push_back(L"  sleep <seconds>    - Wait for specified time");
    helpLines.push_back(L"  cls / clear        - Clear screen");
    helpLines.push_back(L"  help / ?           - Show this help");
    helpLines.push_back(L"  history            - Show command history");
    helpLines.push_back(L"  exit / quit        - Exit program");
    helpLines.push_back(L"");
    helpLines.push_back(L"[Tab Completion]");
    helpLines.push_back(L"  %VAR<tab>          - Complete environment variable");
    helpLines.push_back(L"  %VAR%\\path<tab>    - Complete path after expansion");
    helpLines.push_back(L"  path<tab>          - Complete file/directory path");
    helpLines.push_back(L"");
    helpLines.push_back(L"[Keyboard Shortcuts]");
    helpLines.push_back(L"  Up/Down            - Scroll list / select match");
    helpLines.push_back(L"  Tab                - Apply completion");
    helpLines.push_back(L"  Enter              - Execute command");
    helpLines.push_back(L"  Backspace          - Delete character");
    helpLines.push_back(L"  Esc                - Close help/overlay");
    helpLines.push_back(L"");
    helpLines.push_back(L"Press ESC or any key to close help...");
    
    int listHeight = height - 2;
    int totalLines = helpLines.size();
    int startIdx = state->helpScrollOffset;
    int endIdx = min(startIdx + listHeight, totalLines);
    
    for (int i = startIdx; i < endIdx; i++) {
        setCursor(0, i - startIdx);
        wstring line = helpLines[i];
        if ((int)line.size() > width) {
            line = line.substr(0, width);
        }
        wprintf(L"%s", line.c_str());
    }
    
    setCursor(0, height - 1);
    wstring footer = L"[Line " + to_wstring(startIdx + 1) + L"-" + to_wstring(endIdx) + L"/" + to_wstring(totalLines) + L"] Use Up/Down to scroll, ESC to close";
    if ((int)footer.size() > width) {
        footer = footer.substr(0, width);
    }
    wprintf(L"%s", footer.c_str());
}

void renderHistory(AppState* state) {
    COORD size = getConsoleSize();
    int width = size.X;
    int height = size.Y;
    
    clearScreen();
    
    setCursor(0, 0);
    wprintf(L"=== Command History ===");
    
    int listHeight = height - 2;
    int totalPairs = state->cmdHistory.size() / 2;
    
    if (totalPairs == 0) {
        setCursor(0, 2);
        wprintf(L"(No commands in history)");
    } else {
        int y = 1;
        int pairIdx = state->historyScrollOffset;
        
        while (y < listHeight && pairIdx < totalPairs) {
            wstring cmd = state->cmdHistory[pairIdx * 2];
            wstring result = state->cmdHistory[pairIdx * 2 + 1];
            
            setCursor(0, y);
            wprintf(L"[%d] %s", pairIdx + 1, cmd.c_str());
            y++;
            
            if (y < listHeight) {
                setCursor(0, y);
                wprintf(L"    %s", result.c_str());
                y++;
            }
            
            if (y < listHeight) {
                y++;
            }
            
            pairIdx++;
        }
    }
    
    setCursor(0, height - 1);
    wstring footer = L"[Entry " + to_wstring(state->historyScrollOffset + 1) + L"-" + to_wstring(min(state->historyScrollOffset + (listHeight / 3), totalPairs)) + L"/" + to_wstring(totalPairs) + L"] Use Up/Down to scroll, ESC or Q to close";
    if ((int)footer.size() > width) {
        footer = footer.substr(0, width);
    }
    wprintf(L"%s", footer.c_str());
}

void renderDownloadHistory(AppState* state) {
    COORD size = getConsoleSize();
    int width = size.X;
    int height = size.Y;
    
    clearScreen();
    
    setCursor(0, 0);
    wprintf(L"=== Download History ===");
    
    int listHeight = height - 2;
    
    if (state->downloadHistory.empty()) {
        setCursor(0, 2);
        wprintf(L"(No downloads in history)");
    } else {
        int y = 1;
        int idx = 0;
        
        for (const auto& info : state->downloadHistory) {
            if (y >= listHeight) break;
            
            bool isSelected = (idx == state->downloadHistoryIndex);
            wstring marker = isSelected ? L"[X]" : L"[ ]";
            
            setCursor(0, y);
            
            wstring statusIcon;
            switch (info.status) {
                case Status::Completed: statusIcon = L"[OK]"; break;
                case Status::Error: statusIcon = L"[ERR]"; break;
                case Status::Cancelled: statusIcon = L"[CAN]"; break;
                default: statusIcon = L"[...]"; break;
            }
            
            wstring fileName = StringToWString(info.fileName);
            wstring line = marker + L" " + statusIcon + L" " + fileName;
            if (line.size() > (size_t)width) {
                line = line.substr(0, width);
            }
            wprintf(L"%s", line.c_str());
            y++;
            
            if (y < listHeight) {
                setCursor(0, y);
                wstring urlStr = StringToWString(info.url);
                wstring detailLine = L"    URL: " + urlStr;
                if (detailLine.size() > (size_t)width) {
                    detailLine = detailLine.substr(0, width - 3) + L"...";
                }
                wprintf(L"%s", detailLine.c_str());
                y++;
            }
            
            if (y < listHeight) {
                setCursor(0, y);
                wstring sizeInfo;
                if (info.totalSize > 0) {
                    sizeInfo = L"    Size: " + to_wstring(info.totalSize / 1024) + L" KB";
                } else {
                    sizeInfo = L"    Size: unknown";
                }
                if (info.status == Status::Completed) {
                    sizeInfo += L" [COMPLETED]";
                } else if (info.status == Status::Error) {
                    wstring errMsg = StringToWString(info.errorMessage);
                    sizeInfo += L" [ERROR: " + errMsg + L"]";
                }
                if (sizeInfo.size() > (size_t)width) {
                    sizeInfo = sizeInfo.substr(0, width);
                }
                wprintf(L"%s", sizeInfo.c_str());
                y++;
            }
            
            if (y < listHeight) {
                y++;
            }
            
            idx++;
        }
    }
    
    setCursor(0, height - 1);
    wstring footer = L"[Up/Down: Select] [Enter: Open folder] [O: Open] [C: Copy path] [N: Copy name] [ESC/Q: Close]";
    if ((int)footer.size() > width) {
        footer = footer.substr(0, width);
    }
    wprintf(L"%s", footer.c_str());
}

void renderDownloadProgress(AppState* state, int startY, int availableHeight) {
    lock_guard<mutex> lock(state->downloadMutex);
    
    if (!state->downloadManager) return;
    
    auto tasks = state->downloadManager->getAllTasks();
    
    int y = startY;
    int maxLines = availableHeight;
    int lineCount = 0;
    
    for (const auto& info : tasks) {
        if (lineCount >= maxLines) break;
        if (info.status != Status::Downloading && info.status != Status::Pending) continue;
        
        setCursor(0, y);
        
        wstring statusName;
        switch (info.status) {
            case Status::Pending: statusName = L"WAIT"; break;
            case Status::Downloading: statusName = L"DL  "; break;
            default: statusName = L"    "; break;
        }
        
        wstring fileName = StringToWString(info.fileName);
        if (fileName.size() > 20) {
            fileName = fileName.substr(0, 17) + L"...";
        }
        
        wprintf(L"[%s] %s", statusName.c_str(), fileName.c_str());
        y++;
        lineCount++;
        
        if (lineCount >= maxLines) break;
        
        setCursor(0, y);
        
        int progressWidth = 30;
        int filled = (info.progress * progressWidth) / 100;
        
        wprintf(L"    [");
        for (int i = 0; i < progressWidth; i++) {
            if (i < filled) wprintf(L"=");
            else if (i == filled) wprintf(L">");
            else wprintf(L" ");
        }
        wprintf(L"] %d%%", info.progress);
        y++;
        lineCount++;
        
        if (lineCount >= maxLines) break;
        
        setCursor(0, y);
        
        wstring speedStr;
        if (info.speed > 1024 * 1024) {
            speedStr = to_wstring(info.speed / (1024 * 1024)) + L" MB/s";
        } else if (info.speed > 1024) {
            speedStr = to_wstring(info.speed / 1024) + L" KB/s";
        } else {
            speedStr = to_wstring(info.speed) + L" B/s";
        }
        
        wstring sizeStr;
        if (info.totalSize > 0) {
            sizeStr = to_wstring(info.downloadedSize / 1024) + L"/" + to_wstring(info.totalSize / 1024) + L" KB";
        } else {
            sizeStr = to_wstring(info.downloadedSize / 1024) + L" KB";
        }
        
        wprintf(L"    %s | %s", speedStr.c_str(), sizeStr.c_str());
        y++;
        lineCount++;
        
        if (lineCount < maxLines) {
            y++;
            lineCount++;
        }
    }
}

void render(AppState* state) {
    COORD size = getConsoleSize();
    int width = size.X;
    int height = size.Y;
    
    clearScreen();
    
    int cmdDisplayWidth = width - 2;
    int cursorDisplayPos = state->cursorPos;
    int scrollStart = 0;
    
    if (state->cursorPos > cmdDisplayWidth / 2) {
        scrollStart = state->cursorPos - cmdDisplayWidth / 2;
        cursorDisplayPos = cmdDisplayWidth / 2;
    }
    
    wstring cmdDisplay;
    for (int i = scrollStart; i < state->cmdLength && (int)cmdDisplay.size() < cmdDisplayWidth; i++) {
        cmdDisplay += state->cmdBuffer[i];
    }
    truncateString(cmdDisplay, cmdDisplayWidth);
    
    setCursor(0, 0);
    wprintf(L"> %s", cmdDisplay.c_str());
    
    setCursor(cursorDisplayPos + 2, 0);
    
    int downloadProgressHeight = 0;
    bool hasActiveDownloads = false;
    
    if (state->downloadManager) {
        lock_guard<mutex> lock(state->downloadMutex);
        auto tasks = state->downloadManager->getAllTasks();
        for (const auto& info : tasks) {
            if (info.status == Status::Downloading || info.status == Status::Pending) {
                hasActiveDownloads = true;
                downloadProgressHeight += 4;
            }
        }
    }
    
    if (hasActiveDownloads && downloadProgressHeight > 8) {
        downloadProgressHeight = 8;
    }
    
    int listHeight = height - 3 - downloadProgressHeight;
    int listStartY = 1;
    
    if (state->showMatches && !state->matches.empty()) {
        int matchCount = state->matches.size();
        int maxDisplay = min(matchCount, listHeight);
        
        int centerIndex = state->matchIndex;
        int startIdx = max(0, centerIndex - listHeight / 2);
        int endIdx = min(matchCount, startIdx + listHeight);
        
        if (endIdx - startIdx < listHeight) {
            startIdx = max(0, endIdx - listHeight);
        }
        
        for (int i = startIdx; i < endIdx; i++) {
            int y = listStartY + (i - startIdx);
            setCursor(0, y);
            
            wstring displayName = state->matches[i];
            bool isDir = state->matchIsDir[i];
            uint32_t attrib = state->matchAttribs[i];
            
            wstring arrow = (i == state->matchIndex) ? L"->" : L"  ";
            wstring fullLine;
            
            if (state->isEnvVarMatch) {
                wstring envValue = expandEnvVars(L"%" + displayName + L"%");
                if (envValue != L"%" + displayName + L"%") {
                    if (envValue.size() > 40) {
                        envValue = envValue.substr(0, 37) + L"...";
                    }
                    fullLine = arrow + L"ENV  " + displayName + L" = " + envValue;
                } else {
                    fullLine = arrow + L"ENV  " + displayName + L" (undefined)";
                }
            } else {
                wstring attrStr = getAttribStr(attrib, isDir);
                wstring suffix = isDir ? L"\\" : L"";
                fullLine = arrow + attrStr + L" " + displayName + suffix;
            }
            
            truncateString(fullLine, width);
            wprintf(L"%s", fullLine.c_str());
        }
    } else if (state->showMatches && state->matches.empty()) {
        setCursor(0, listStartY + listHeight / 2);
        if (state->isEnvVarMatch) {
            wprintf(L"-> ERROR: ENV_VAR_NOT_FOUND");
        } else {
            wprintf(L"-> NOT FOUND");
        }
    } else {
        int totalItems = state->dirs.size() + state->files.size();
        int maxDisplay = min(totalItems, listHeight);
        
        int centerIndex = state->scrollOffset;
        int startIdx = max(0, centerIndex - listHeight / 2);
        int endIdx = min(totalItems, startIdx + listHeight);
        
        if (endIdx - startIdx < listHeight) {
            startIdx = max(0, endIdx - listHeight);
        }
        
        for (int i = startIdx; i < endIdx; i++) {
            int y = listStartY + (i - startIdx);
            setCursor(0, y);
            
            wstring displayName;
            bool isDir = false;
            uint32_t attrib = 0;
            
            if (i < (int)state->dirs.size()) {
                displayName = state->dirs[i].name;
                attrib = state->dirs[i].attrib;
                isDir = true;
            } else {
                int fileIdx = i - state->dirs.size();
                displayName = state->files[fileIdx].name;
                attrib = state->files[fileIdx].attrib;
            }
            
            wstring attrStr = getAttribStr(attrib, isDir);
            wstring suffix = isDir ? L"\\" : L"";
            wstring fullLine = L"  " + attrStr + L" " + displayName + suffix;
            
            truncateString(fullLine, width);
            wprintf(L"%s", fullLine.c_str());
        }
    }
    
    if (hasActiveDownloads) {
        int downloadStartY = listStartY + listHeight;
        
        setCursor(0, downloadStartY);
        wstring separator = L"--- Downloads ---";
        if ((int)separator.size() < width) {
            separator += wstring(width - separator.size(), L'-');
        }
        wprintf(L"%s", separator.substr(0, width).c_str());
        
        renderDownloadProgress(state, downloadStartY + 1, downloadProgressHeight - 1);
    }
    
    if (state->cmdLength > 0) {
        setCursor(0, height - 1);
        wstring cmd(state->cmdBuffer);
        vector<wstring> args = splitString(cmd);
        if (!args.empty()) {
            int argIndex = -1;
            
            size_t cmdNameLen = args[0].size();
            
            if (cmd.size() > cmdNameLen) {
                bool endsWithSpace = cmd.back() == L' ';
                int numArgs = args.size() - 1;
                
                if (endsWithSpace) {
                    argIndex = numArgs;
                } else {
                    argIndex = numArgs - 1;
                }
            }
            
            wstring hint = getCommandHint(args[0], argIndex);
            truncateString(hint, width);
            wprintf(L"%s", hint.c_str());
        }
    } else if (!state->statusMsg.empty()) {
        setCursor(0, height - 1);
        wstring statusDisplay = state->statusMsg;
        truncateString(statusDisplay, width);
        wprintf(L"%s", statusDisplay.c_str());
    }
    
    setCursor(cursorDisplayPos + 2, 0);
}

int main(int argc, char** argv) {
    if (!Initialize()) {
        printf("Failed to initialize DLCore\n");
        return 1;
    }
    
    AppState state;
    state.cmdLength = 0;
    state.cursorPos = 0;
    state.cmdBuffer[0] = '\0';
    state.matchIndex = 0;
    state.showMatches = false;
    state.isEnvVarMatch = false;
    state.envVarStartPos = -1;
    state.scrollOffset = 0;
    state.statusMsg = L"";
    state.showHelp = false;
    state.helpScrollOffset = 0;
    state.showHistory = false;
    state.historyScrollOffset = 0;
    state.downloadManager = nullptr;
    state.downloadProgress = 0;
    state.downloadSpeed = 0;
    state.downloadTotal = 0;
    state.downloadDownloaded = 0;
    state.showDownloadProgress = false;
    state.downloadRunning = false;
    state.showDownloadHistory = false;
    state.downloadHistoryIndex = 0;
    state.sevenZipArchive = nullptr;
    
    wstring initialPath = L"E:\\expl";
    state.currentPath = parseAbsolutePath(initialPath);
    
    if (state.currentPath.empty() || !isValidDrive(state.currentPath[0])) {
        wstring validDrive = findFirstValidDrive();
        state.currentPath.clear();
        state.currentPath.push_back(validDrive);
    }
    
    getFiles(&state);
    findMatches(&state);
    
    while (true) {
        if (state.showHelp) {
            renderHelp(&state);
            
            int c = fcase();
            
            if (c == 27) {
                state.showHelp = false;
            } else if (c == UP) {
                if (state.helpScrollOffset > 0) {
                    state.helpScrollOffset--;
                }
            } else if (c == DOWN) {
                state.helpScrollOffset++;
            } else if (c == 'q' || c == 'Q') {
                state.showHelp = false;
            } else {
                state.showHelp = false;
            }
            continue;
        }
        
        if (state.showHistory) {
            renderHistory(&state);
            
            int c = fcase();
            
            if (c == 27) {
                state.showHistory = false;
            } else if (c == UP) {
                if (state.historyScrollOffset > 0) {
                    state.historyScrollOffset--;
                }
            } else if (c == DOWN) {
                int totalPairs = state.cmdHistory.size() / 2;
                if (state.historyScrollOffset < totalPairs - 1) {
                    state.historyScrollOffset++;
                }
            } else if (c == 'q' || c == 'Q') {
                state.showHistory = false;
            } else {
                state.showHistory = false;
            }
            continue;
        }
        
        if (state.showDownloadHistory) {
            renderDownloadHistory(&state);
            
            int c = fcase();
            
            if (c == 27 || c == 'q' || c == 'Q') {
                state.showDownloadHistory = false;
            } else if (c == UP) {
                if (state.downloadHistoryIndex > 0) {
                    state.downloadHistoryIndex--;
                }
            } else if (c == DOWN) {
                if (state.downloadHistoryIndex < (int)state.downloadHistory.size() - 1) {
                    state.downloadHistoryIndex++;
                }
            } else if (c == ENTER || c == 'o' || c == 'O') {
                if (!state.downloadHistory.empty() && state.downloadHistoryIndex >= 0 && 
                    state.downloadHistoryIndex < (int)state.downloadHistory.size()) {
                    auto& info = state.downloadHistory[state.downloadHistoryIndex];
                    string fullPath = info.savePath + "\\" + info.fileName;
                    if (c == ENTER) {
                        string folderPath = info.savePath;
                        ShellExecuteA(NULL, "open", folderPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
                    } else {
                        ShellExecuteA(NULL, "open", fullPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
                    }
                }
            } else if (c == 'c' || c == 'C') {
                if (!state.downloadHistory.empty() && state.downloadHistoryIndex >= 0 && 
                    state.downloadHistoryIndex < (int)state.downloadHistory.size()) {
                    auto& info = state.downloadHistory[state.downloadHistoryIndex];
                    string fullPath = info.savePath + "\\" + info.fileName;
                    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, fullPath.size() + 1);
                    if (hMem) {
                        memcpy(GlobalLock(hMem), fullPath.c_str(), fullPath.size() + 1);
                        GlobalUnlock(hMem);
                        OpenClipboard(NULL);
                        EmptyClipboard();
                        SetClipboardData(CF_TEXT, hMem);
                        CloseClipboard();
                    }
                }
            } else if (c == 'n' || c == 'N') {
                if (!state.downloadHistory.empty() && state.downloadHistoryIndex >= 0 && 
                    state.downloadHistoryIndex < (int)state.downloadHistory.size()) {
                    auto& info = state.downloadHistory[state.downloadHistoryIndex];
                    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, info.fileName.size() + 1);
                    if (hMem) {
                        memcpy(GlobalLock(hMem), info.fileName.c_str(), info.fileName.size() + 1);
                        GlobalUnlock(hMem);
                        OpenClipboard(NULL);
                        EmptyClipboard();
                        SetClipboardData(CF_TEXT, hMem);
                        CloseClipboard();
                    }
                }
            }
            continue;
        }
        
        render(&state);
        
        bool hasActiveDownloads = false;
        if (state.downloadManager) {
            lock_guard<mutex> lock(state.downloadMutex);
            auto tasks = state.downloadManager->getAllTasks();
            for (const auto& info : tasks) {
                if (info.status == Status::Downloading || info.status == Status::Pending) {
                    hasActiveDownloads = true;
                    break;
                }
            }
        }
        
        int c;
        
        if (hasActiveDownloads) {
            if (_kbhit()) {
                c = fcase();
            } else {
                Sleep(100);
                continue;
            }
        } else {
            c = fcase();
        }
        
        switch (c) {
        case LEFT:
            if (state.cursorPos > 0) {
                state.cursorPos--;
                findMatches(&state);
            }
            break;
            
        case RIGHT:
            if (state.cursorPos < state.cmdLength) {
                state.cursorPos++;
                findMatches(&state);
            }
            break;
            
        case UP:
            if (state.showMatches && !state.matches.empty()) {
                if (state.matchIndex > 0) {
                    state.matchIndex--;
                }
            } else {
                if (state.scrollOffset > 0) {
                    state.scrollOffset--;
                }
            }
            break;
            
        case DOWN:
            if (state.showMatches && !state.matches.empty()) {
                if (state.matchIndex < (int)state.matches.size() - 1) {
                    state.matchIndex++;
                }
            } else {
                int totalItems = state.dirs.size() + state.files.size();
                if (state.scrollOffset < totalItems - 1) {
                    state.scrollOffset++;
                }
            }
            break;
            
        case BACKSPACE:
            if (state.cursorPos > 0) {
                for (int i = state.cursorPos - 1; i < state.cmdLength - 1; i++) {
                    state.cmdBuffer[i] = state.cmdBuffer[i + 1];
                }
                state.cmdBuffer[state.cmdLength - 1] = '\0';
                state.cmdLength--;
                state.cursorPos--;
                findMatches(&state);
            }
            break;
            
        case TAB:
            if (state.showMatches && !state.matches.empty()) {
                applyMatch(&state);
            }
            break;
            
        case ENTER:
            if (state.cmdLength > 0) {
                executeCommand(&state);
                state.cmdLength = 0;
                state.cursorPos = 0;
                state.cmdBuffer[0] = '\0';
                findMatches(&state);
            }
            break;
            
        default:
            if (c >= 32 && c <= 126) {
                if (state.cmdLength < MAX_CMD_LEN - 1) {
                    for (int i = state.cmdLength; i > state.cursorPos; i--) {
                        state.cmdBuffer[i] = state.cmdBuffer[i - 1];
                    }
                    state.cmdBuffer[state.cursorPos] = (wchar_t)c;
                    state.cmdLength++;
                    state.cursorPos++;
                    state.cmdBuffer[state.cmdLength] = '\0';
                    findMatches(&state);
                }
            }
            break;
        }
    }
    
    return 0;
}
