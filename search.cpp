#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <algorithm>
#include <atomic>
#include <thread>
#include <mutex>
#include <conio.h>
#include <shellapi.h>

#include "search.h"

struct SearchFileInfo {
    std::string name;
    std::string path;
    std::string extension;
    unsigned long long size;
    FILETIME createTime;
    FILETIME modifyTime;
    FILETIME accessTime;
    bool isDirectory;
    int pathDepth;
    
    SearchFileInfo() : size(0), isDirectory(false), pathDepth(0) {
        createTime = {0, 0};
        modifyTime = {0, 0};
        accessTime = {0, 0};
    }
};

static std::vector<SearchFileInfo> g_indexedFiles;
static std::vector<SearchFileInfo> g_searchResults;
static std::string g_currentPath;
static int g_scope = 0;
static std::atomic<bool> g_isIndexing(false);
static std::atomic<bool> g_cancelIndexing(false);
static std::atomic<int> g_indexProgress(0);
static std::mutex g_indexMutex;
static std::mutex g_resultMutex;
static std::atomic<bool> g_searchActive(false);

static std::vector<std::string> g_cachedDrives;

enum SortBy {
    SORT_SIZE,
    SORT_NAME,
    SORT_PATH,
    SORT_EXTEND_NAME,
    SORT_CHANGED_TIME,
    SORT_CREATED_TIME
};

enum CompareType {
    COMPARE_GREATER,
    COMPARE_LESS
};

static int g_sortBy = SORT_SIZE;
static int g_compareType = COMPARE_LESS;
static std::vector<std::string> g_keywords;

static HMODULE g_everythingDll = NULL;
static bool g_everythingAvailable = false;

typedef void (__stdcall *Everything_SetSearchA_t)(LPCSTR lpString);
typedef BOOL (__stdcall *Everything_QueryA_t)(BOOL bWait);
typedef DWORD (__stdcall *Everything_GetNumResults_t)(void);
typedef BOOL (__stdcall *Everything_IsFolderResult_t)(DWORD dwIndex);
typedef LPCSTR (__stdcall *Everything_GetResultFileNameA_t)(DWORD dwIndex);
typedef LPCSTR (__stdcall *Everything_GetResultPathA_t)(DWORD dwIndex);
typedef DWORD (__stdcall *Everything_GetResultFullPathNameA_t)(DWORD dwIndex, LPSTR buf, DWORD bufsize);
typedef BOOL (__stdcall *Everything_GetResultSize_t)(DWORD dwIndex, LARGE_INTEGER *lpSize);
typedef BOOL (__stdcall *Everything_GetResultDateCreated_t)(DWORD dwIndex, FILETIME *lpDateCreated);
typedef BOOL (__stdcall *Everything_GetResultDateModified_t)(DWORD dwIndex, FILETIME *lpDateModified);
typedef DWORD (__stdcall *Everything_GetLastError_t)(void);
typedef void (__stdcall *Everything_SetSort_t)(DWORD dwSort);
typedef void (__stdcall *Everything_SetRequestFlags_t)(DWORD dwRequestFlags);
typedef void (__stdcall *Everything_Reset_t)(void);

static Everything_SetSearchA_t pEverything_SetSearchA = NULL;
static Everything_QueryA_t pEverything_QueryA = NULL;
static Everything_GetNumResults_t pEverything_GetNumResults = NULL;
static Everything_IsFolderResult_t pEverything_IsFolderResult = NULL;
static Everything_GetResultFileNameA_t pEverything_GetResultFileNameA = NULL;
static Everything_GetResultPathA_t pEverything_GetResultPathA = NULL;
static Everything_GetResultFullPathNameA_t pEverything_GetResultFullPathNameA = NULL;
static Everything_GetResultSize_t pEverything_GetResultSize = NULL;
static Everything_GetResultDateCreated_t pEverything_GetResultDateCreated = NULL;
static Everything_GetResultDateModified_t pEverything_GetResultDateModified = NULL;
static Everything_GetLastError_t pEverything_GetLastError = NULL;
static Everything_SetSort_t pEverything_SetSort = NULL;
static Everything_SetRequestFlags_t pEverything_SetRequestFlags = NULL;
static Everything_Reset_t pEverything_Reset = NULL;

#define EVERYTHING_REQUEST_FILE_NAME 0x00000001
#define EVERYTHING_REQUEST_PATH 0x00000002
#define EVERYTHING_REQUEST_SIZE 0x00000010
#define EVERYTHING_REQUEST_DATE_CREATED 0x00000020
#define EVERYTHING_REQUEST_DATE_MODIFIED 0x00000040

#define EVERYTHING_SORT_NAME_ASCENDING 1
#define EVERYTHING_SORT_NAME_DESCENDING 2
#define EVERYTHING_SORT_PATH_ASCENDING 3
#define EVERYTHING_SORT_PATH_DESCENDING 4
#define EVERYTHING_SORT_SIZE_ASCENDING 5
#define EVERYTHING_SORT_SIZE_DESCENDING 6
#define EVERYTHING_SORT_EXTENSION_ASCENDING 7
#define EVERYTHING_SORT_EXTENSION_DESCENDING 8
#define EVERYTHING_SORT_DATE_CREATED_ASCENDING 11
#define EVERYTHING_SORT_DATE_CREATED_DESCENDING 12
#define EVERYTHING_SORT_DATE_MODIFIED_ASCENDING 13
#define EVERYTHING_SORT_DATE_MODIFIED_DESCENDING 14

static bool LoadEverythingDll() {
    if (g_everythingDll) return g_everythingAvailable;
    
    char dllPath[MAX_PATH];
    GetModuleFileNameA(NULL, dllPath, MAX_PATH);
    std::string exePath(dllPath);
    size_t lastSlash = exePath.rfind('\\');
    if (lastSlash != std::string::npos) {
        exePath = exePath.substr(0, lastSlash);
    }
    
    std::string dll64Path = exePath + "\\Everything64.dll";
    std::string dll32Path = exePath + "\\Everything32.dll";
    
#ifdef _WIN64
    g_everythingDll = LoadLibraryA(dll64Path.c_str());
    if (!g_everythingDll) {
        g_everythingDll = LoadLibraryA("Everything64.dll");
    }
#else
    g_everythingDll = LoadLibraryA(dll32Path.c_str());
    if (!g_everythingDll) {
        g_everythingDll = LoadLibraryA("Everything32.dll");
    }
#endif
    
    if (!g_everythingDll) return false;
    
    pEverything_SetSearchA = (Everything_SetSearchA_t)GetProcAddress(g_everythingDll, "Everything_SetSearchA");
    pEverything_QueryA = (Everything_QueryA_t)GetProcAddress(g_everythingDll, "Everything_QueryA");
    pEverything_GetNumResults = (Everything_GetNumResults_t)GetProcAddress(g_everythingDll, "Everything_GetNumResults");
    pEverything_IsFolderResult = (Everything_IsFolderResult_t)GetProcAddress(g_everythingDll, "Everything_IsFolderResult");
    pEverything_GetResultFileNameA = (Everything_GetResultFileNameA_t)GetProcAddress(g_everythingDll, "Everything_GetResultFileNameA");
    pEverything_GetResultPathA = (Everything_GetResultPathA_t)GetProcAddress(g_everythingDll, "Everything_GetResultPathA");
    pEverything_GetResultFullPathNameA = (Everything_GetResultFullPathNameA_t)GetProcAddress(g_everythingDll, "Everything_GetResultFullPathNameA");
    pEverything_GetResultSize = (Everything_GetResultSize_t)GetProcAddress(g_everythingDll, "Everything_GetResultSize");
    pEverything_GetResultDateCreated = (Everything_GetResultDateCreated_t)GetProcAddress(g_everythingDll, "Everything_GetResultDateCreated");
    pEverything_GetResultDateModified = (Everything_GetResultDateModified_t)GetProcAddress(g_everythingDll, "Everything_GetResultDateModified");
    pEverything_GetLastError = (Everything_GetLastError_t)GetProcAddress(g_everythingDll, "Everything_GetLastError");
    pEverything_SetSort = (Everything_SetSort_t)GetProcAddress(g_everythingDll, "Everything_SetSort");
    pEverything_SetRequestFlags = (Everything_SetRequestFlags_t)GetProcAddress(g_everythingDll, "Everything_SetRequestFlags");
    pEverything_Reset = (Everything_Reset_t)GetProcAddress(g_everythingDll, "Everything_Reset");
    
    if (!pEverything_SetSearchA || !pEverything_QueryA || !pEverything_GetNumResults) {
        FreeLibrary(g_everythingDll);
        g_everythingDll = NULL;
        return false;
    }
    
    g_everythingAvailable = true;
    return true;
}

static void UnloadEverythingDll() {
    if (g_everythingDll) {
        FreeLibrary(g_everythingDll);
        g_everythingDll = NULL;
    }
    g_everythingAvailable = false;
}

static bool SearchWithEverything(const std::string& keywords) {
    if (!g_everythingAvailable || !pEverything_Reset) return false;
    
    pEverything_Reset();
    
    DWORD sortType = EVERYTHING_SORT_NAME_ASCENDING;
    switch (g_sortBy) {
        case SORT_SIZE: sortType = (g_compareType == COMPARE_LESS) ? EVERYTHING_SORT_SIZE_ASCENDING : EVERYTHING_SORT_SIZE_DESCENDING; break;
        case SORT_NAME: sortType = (g_compareType == COMPARE_LESS) ? EVERYTHING_SORT_NAME_ASCENDING : EVERYTHING_SORT_NAME_DESCENDING; break;
        case SORT_PATH: sortType = (g_compareType == COMPARE_LESS) ? EVERYTHING_SORT_PATH_ASCENDING : EVERYTHING_SORT_PATH_DESCENDING; break;
        case SORT_EXTEND_NAME: sortType = (g_compareType == COMPARE_LESS) ? EVERYTHING_SORT_EXTENSION_ASCENDING : EVERYTHING_SORT_EXTENSION_DESCENDING; break;
        case SORT_CHANGED_TIME: sortType = (g_compareType == COMPARE_LESS) ? EVERYTHING_SORT_DATE_MODIFIED_ASCENDING : EVERYTHING_SORT_DATE_MODIFIED_DESCENDING; break;
        case SORT_CREATED_TIME: sortType = (g_compareType == COMPARE_LESS) ? EVERYTHING_SORT_DATE_CREATED_ASCENDING : EVERYTHING_SORT_DATE_CREATED_DESCENDING; break;
    }
    
    if (pEverything_SetSort) pEverything_SetSort(sortType);
    if (pEverything_SetRequestFlags) {
        pEverything_SetRequestFlags(EVERYTHING_REQUEST_FILE_NAME | EVERYTHING_REQUEST_PATH | 
                                     EVERYTHING_REQUEST_SIZE | EVERYTHING_REQUEST_DATE_CREATED | 
                                     EVERYTHING_REQUEST_DATE_MODIFIED);
    }
    
    std::string searchQuery = keywords;
    if (g_scope == 0 && !g_currentPath.empty()) {
        searchQuery = "\"" + g_currentPath + "\" " + keywords;
    }
    
    pEverything_SetSearchA(searchQuery.c_str());
    
    if (!pEverything_QueryA(TRUE)) {
        DWORD err = pEverything_GetLastError ? pEverything_GetLastError() : 0;
        if (err == 2) {
            return false;
        }
        return false;
    }
    
    DWORD numResults = pEverything_GetNumResults();
    
    std::lock_guard<std::mutex> lock(g_resultMutex);
    g_searchResults.clear();
    g_searchResults.reserve(numResults);
    
    for (DWORD i = 0; i < numResults; i++) {
        SearchFileInfo info;
        
        char pathBuf[MAX_PATH];
        DWORD len = pEverything_GetResultFullPathNameA(i, pathBuf, MAX_PATH);
        if (len > 0) {
            info.path = pathBuf;
        } else {
            const char* fileName = pEverything_GetResultFileNameA(i);
            const char* filePath = pEverything_GetResultPathA(i);
            if (filePath && fileName) {
                info.path = std::string(filePath) + "\\" + fileName;
            } else if (fileName) {
                info.path = fileName;
            }
        }
        
        const char* fileName = pEverything_GetResultFileNameA(i);
        if (fileName) {
            info.name = fileName;
        }
        
        info.isDirectory = pEverything_IsFolderResult(i);
        
        LARGE_INTEGER size;
        if (pEverything_GetResultSize && pEverything_GetResultSize(i, &size)) {
            info.size = size.QuadPart;
        }
        
        if (pEverything_GetResultDateCreated) {
            pEverything_GetResultDateCreated(i, &info.createTime);
        }
        if (pEverything_GetResultDateModified) {
            pEverything_GetResultDateModified(i, &info.modifyTime);
        }
        
        size_t dotPos = info.name.rfind('.');
        if (dotPos != std::string::npos && dotPos < info.name.length() - 1) {
            info.extension = info.name.substr(dotPos + 1);
        }
        
        g_searchResults.push_back(info);
    }
    
    return true;
}

static void ClearScreen() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    
    COORD coordScreen = {0, 0};
    DWORD cCharsWritten;
    DWORD dwConSize = csbi.dwSize.X * csbi.dwSize.Y;
    
    FillConsoleOutputCharacterA(hConsole, ' ', dwConSize, coordScreen, &cCharsWritten);
    FillConsoleOutputAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE, dwConSize, coordScreen, &cCharsWritten);
    SetConsoleCursorPosition(hConsole, coordScreen);
}

static void SetCursor(int x, int y) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD coord;
    coord.X = x;
    coord.Y = y;
    SetConsoleCursorPosition(hConsole, coord);
}

static COORD GetConsoleSize() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    COORD size;
    size.X = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    size.Y = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    return size;
}

static void HideCursor() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(hConsole, &cursorInfo);
    cursorInfo.bVisible = FALSE;
    SetConsoleCursorInfo(hConsole, &cursorInfo);
}

static void ShowCursor() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(hConsole, &cursorInfo);
    cursorInfo.bVisible = TRUE;
    SetConsoleCursorInfo(hConsole, &cursorInfo);
}

static std::string ToLower(const std::string& str) {
    std::string result = str;
    for (auto& c : result) {
        c = tolower((unsigned char)c);
    }
    return result;
}

static std::vector<std::string> GetDrives() {
    if (!g_cachedDrives.empty()) {
        return g_cachedDrives;
    }
    
    std::vector<std::string> drives;
    char buffer[256];
    GetLogicalDriveStringsA(256, buffer);
    
    char* ptr = buffer;
    while (*ptr) {
        std::string drive = ptr;
        if (GetDriveTypeA(drive.c_str()) != DRIVE_NO_ROOT_DIR) {
            drives.push_back(drive);
        }
        ptr += drive.length() + 1;
    }
    
    g_cachedDrives = drives;
    return drives;
}

bool search_initialize() {
    g_cachedDrives.clear();
    GetDrives();
    LoadEverythingDll();
    return true;
}

void search_cleanup() {
    g_cancelIndexing = true;
    if (g_isIndexing) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    {
        std::lock_guard<std::mutex> lock(g_indexMutex);
        g_indexedFiles.clear();
    }
    {
        std::lock_guard<std::mutex> lock(g_resultMutex);
        g_searchResults.clear();
    }
    UnloadEverythingDll();
}

void search_set_scope(int scope, const char* customPath) {
    g_scope = scope;
    if (customPath) {
        g_currentPath = customPath;
    }
}

static void IndexDirectory(const std::string& path, int depth) {
    if (g_cancelIndexing) return;
    
    WIN32_FIND_DATAA findData;
    std::string searchPath = path + "\\*";
    
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) return;
    
    std::vector<std::string> subdirs;
    
    do {
        if (g_cancelIndexing) break;
        
        std::string name = findData.cFileName;
        if (name == "." || name == "..") continue;
        
        SearchFileInfo info;
        info.name = name;
        info.path = path + "\\" + name;
        info.isDirectory = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        info.pathDepth = depth;
        
        if (info.isDirectory) {
            info.extension = "";
            info.size = 0;
            subdirs.push_back(info.path);
        } else {
            size_t dotPos = name.rfind('.');
            if (dotPos != std::string::npos && dotPos < name.length() - 1) {
                info.extension = name.substr(dotPos + 1);
            }
            ULARGE_INTEGER fileSize;
            fileSize.LowPart = findData.nFileSizeLow;
            fileSize.HighPart = findData.nFileSizeHigh;
            info.size = fileSize.QuadPart;
        }
        
        info.createTime = findData.ftCreationTime;
        info.modifyTime = findData.ftLastWriteTime;
        info.accessTime = findData.ftLastAccessTime;
        
        {
            std::lock_guard<std::mutex> lock(g_indexMutex);
            g_indexedFiles.push_back(info);
            g_indexProgress++;
        }
        
    } while (FindNextFileA(hFind, &findData));
    
    FindClose(hFind);
    
    for (const auto& subdir : subdirs) {
        if (g_cancelIndexing) break;
        IndexDirectory(subdir, depth + 1);
    }
}

void search_start_indexing() {
    if (g_isIndexing) return;
    
    g_cancelIndexing = false;
    g_isIndexing = true;
    g_indexProgress = 0;
    
    {
        std::lock_guard<std::mutex> lock(g_indexMutex);
        g_indexedFiles.clear();
    }
    
    std::thread([]() {
        if (g_scope == 1) {
            auto drives = GetDrives();
            for (const auto& drive : drives) {
                if (g_cancelIndexing) break;
                std::string path = drive;
                if (path.back() == '\\') path.pop_back();
                IndexDirectory(path, 0);
            }
        } else {
            IndexDirectory(g_currentPath, 0);
        }
        
        g_isIndexing = false;
    }).detach();
}

bool search_is_indexing() {
    return g_isIndexing;
}

int search_get_progress() {
    return g_indexProgress;
}

void search_cancel_indexing() {
    g_cancelIndexing = true;
}

void search_set_keywords(const char* keywords) {
    g_keywords.clear();
    if (!keywords) return;
    
    std::string kw = keywords;
    std::string current;
    
    for (char c : kw) {
        if (c == ' ') {
            if (!current.empty()) {
                g_keywords.push_back(ToLower(current));
                current.clear();
            }
        } else {
            current += c;
        }
    }
    
    if (!current.empty()) {
        g_keywords.push_back(ToLower(current));
    }
}

static bool MatchKeywords(const SearchFileInfo& info) {
    if (g_keywords.empty()) return true;
    
    std::string lowerName = ToLower(info.name);
    
    for (const auto& kw : g_keywords) {
        if (lowerName.find(kw) == std::string::npos) {
            return false;
        }
    }
    
    return true;
}

static int CompareFILETIME(const FILETIME& a, const FILETIME& b) {
    ULARGE_INTEGER ua, ub;
    ua.LowPart = a.dwLowDateTime;
    ua.HighPart = a.dwHighDateTime;
    ub.LowPart = b.dwLowDateTime;
    ub.HighPart = b.dwHighDateTime;
    
    if (ua.QuadPart == ub.QuadPart) return 0;
    return (ua.QuadPart < ub.QuadPart) ? -1 : 1;
}

static bool CompareResults(const SearchFileInfo& a, const SearchFileInfo& b) {
    if (a.isDirectory != b.isDirectory) {
        return a.isDirectory;
    }
    
    bool lessOrder = (g_compareType == COMPARE_LESS);
    int cmp = 0;
    
    switch (g_sortBy) {
        case SORT_SIZE:
            if (a.size != b.size) {
                cmp = (a.size < b.size) ? -1 : 1;
            }
            break;
        case SORT_NAME:
            cmp = _stricmp(a.name.c_str(), b.name.c_str());
            break;
        case SORT_PATH:
            cmp = _stricmp(a.path.c_str(), b.path.c_str());
            break;
        case SORT_EXTEND_NAME: {
            std::string extA = a.extension.empty() ? "" : "." + a.extension;
            std::string extB = b.extension.empty() ? "" : "." + b.extension;
            cmp = _stricmp(extA.c_str(), extB.c_str());
            break;
        }
        case SORT_CHANGED_TIME:
            cmp = CompareFILETIME(a.modifyTime, b.modifyTime);
            break;
        case SORT_CREATED_TIME:
            cmp = CompareFILETIME(a.createTime, b.createTime);
            break;
    }
    
    if (cmp == 0) {
        return _stricmp(a.name.c_str(), b.name.c_str()) < 0;
    }
    
    return lessOrder ? (cmp < 0) : (cmp > 0);
}

void search_execute() {
    std::string keywordStr;
    for (size_t i = 0; i < g_keywords.size(); i++) {
        if (i > 0) keywordStr += " ";
        keywordStr += g_keywords[i];
    }
    
    if (g_everythingAvailable && SearchWithEverything(keywordStr)) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(g_resultMutex);
    
    g_searchResults.clear();
    
    {
        std::lock_guard<std::mutex> indexLock(g_indexMutex);
        for (const auto& info : g_indexedFiles) {
            if (MatchKeywords(info)) {
                g_searchResults.push_back(info);
            }
        }
    }
    
    std::sort(g_searchResults.begin(), g_searchResults.end(), CompareResults);
}

int search_get_result_count() {
    std::lock_guard<std::mutex> lock(g_resultMutex);
    return (int)g_searchResults.size();
}

bool search_is_active() {
    return g_searchActive;
}

void search_close() {
    g_searchActive = false;
}

static std::string FormatSize(unsigned long long size) {
    char buffer[64];
    if (size >= 1024ULL * 1024 * 1024) {
        sprintf(buffer, "%.1fGB", (double)size / (1024 * 1024 * 1024));
    } else if (size >= 1024 * 1024) {
        sprintf(buffer, "%.1fMB", (double)size / (1024 * 1024));
    } else if (size >= 1024) {
        sprintf(buffer, "%.1fKB", (double)size / 1024);
    } else {
        sprintf(buffer, "%lluB", size);
    }
    return buffer;
}

static std::vector<std::string> GetSubFolders(const std::string& path) {
    std::vector<std::string> folders;
    
    WIN32_FIND_DATAA findData;
    std::string searchPath = path + "\\*";
    
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) return folders;
    
    do {
        std::string name = findData.cFileName;
        if (name == "." || name == "..") continue;
        
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            folders.push_back(name);
        }
    } while (FindNextFileA(hFind, &findData));
    
    FindClose(hFind);
    
    std::sort(folders.begin(), folders.end(), [](const std::string& a, const std::string& b) {
        return _stricmp(a.c_str(), b.c_str()) < 0;
    });
    
    return folders;
}

static bool MoveToRecycleBin(const char* path) {
    SHFILEOPSTRUCTA fileOp = {0};
    char fromPath[MAX_PATH + 2] = {0};
    strcpy(fromPath, path);
    fromPath[strlen(path) + 1] = '\0';
    
    fileOp.wFunc = FO_DELETE;
    fileOp.pFrom = fromPath;
    fileOp.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT;
    
    return SHFileOperationA(&fileOp) == 0;
}

static bool CopyToClipboard(const char* text) {
    if (!OpenClipboard(NULL)) return false;
    EmptyClipboard();
    
    size_t len = strlen(text) + 1;
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
    if (!hMem) {
        CloseClipboard();
        return false;
    }
    
    memcpy(GlobalLock(hMem), text, len);
    GlobalUnlock(hMem);
    
    SetClipboardData(CF_TEXT, hMem);
    CloseClipboard();
    return true;
}

static int GetKeyCode() {
    int c = getch();
    if (c == 0 || c == 224) {
        c = getch();
        switch (c) {
            case 72: return 1000;
            case 80: return 1001;
            case 75: return 1002;
            case 77: return 1003;
            case 83: return 1004;
            default: return 2000 + c;
        }
    }
    return c;
}

static bool RunScopeSelection(std::string& selectedPath, int& scope) {
    int selectedIndex = 0;
    bool inPathBrowse = false;
    std::string browsePath;
    std::vector<std::string> browseItems;
    int browseIndex = 0;
    bool browseAtDrives = false;
    
    while (true) {
        COORD size = GetConsoleSize();
        ClearScreen();
        HideCursor();
        
        if (!inPathBrowse) {
            SetCursor(0, 0);
            printf("set area (move by up/down, start by enter, quit by esc)");
            if (g_everythingAvailable) {
                printf(" [Everything OK]");
            }
            
            const char* options[] = {
                "this folder",
                "full computer",
                "input myself"
            };
            
            for (int i = 0; i < 3; i++) {
                SetCursor(0, 2 + i);
                if (i == selectedIndex) {
                    printf("[X] %s", options[i]);
                    if (i == 2) printf(" {->}");
                } else {
                    printf("[ ] %s", options[i]);
                }
            }
            
            SetCursor(0, size.Y - 1);
            printf("[Up/Down: Select] [Enter: Confirm] [Right: Enter path] [Esc: Quit]");
        } else {
            SetCursor(0, 0);
            printf("Browse path (select by enter, back by left, quit by esc)");
            
            if (browseAtDrives) {
                SetCursor(0, 2);
                printf("[Computer]");
                
                int maxDisplay = size.Y - 6;
                int displayCount = std::min((int)browseItems.size(), maxDisplay);
                
                for (int i = 0; i < displayCount; i++) {
                    SetCursor(0, 4 + i);
                    if (i == browseIndex) {
                        printf("-> %s", browseItems[i].c_str());
                    } else {
                        printf("   %s", browseItems[i].c_str());
                    }
                }
            } else {
                SetCursor(0, 2);
                printf("[%s]", browsePath.c_str());
                
                int maxDisplay = size.Y - 6;
                int displayCount = std::min((int)browseItems.size(), maxDisplay);
                
                for (int i = 0; i < displayCount; i++) {
                    SetCursor(0, 4 + i);
                    if (i == browseIndex) {
                        printf("-> %s\\", browseItems[i].c_str());
                    } else {
                        printf("   %s\\", browseItems[i].c_str());
                    }
                }
            }
            
            SetCursor(0, size.Y - 1);
            printf("[Up/Down: Select] [Right/Enter: Enter] [Left: Back] [Esc: Cancel]");
        }
        
        int c = GetKeyCode();
        
        if (!inPathBrowse) {
            if (c == 1000) {
                if (selectedIndex > 0) selectedIndex--;
            } else if (c == 1001) {
                if (selectedIndex < 2) selectedIndex++;
            } else if (c == 1003) {
                if (selectedIndex == 2) {
                    inPathBrowse = true;
                    browseAtDrives = true;
                    browseItems = GetDrives();
                    browseIndex = 0;
                }
            } else if (c == '\r') {
                if (selectedIndex == 0) {
                    scope = 0;
                    selectedPath = g_currentPath;
                } else if (selectedIndex == 1) {
                    scope = 1;
                    selectedPath = "";
                } else {
                    scope = 2;
                    selectedPath = browsePath.empty() ? g_currentPath : browsePath;
                }
                return true;
            } else if (c == 27) {
                return false;
            }
        } else {
            if (c == 1000) {
                if (browseIndex > 0) browseIndex--;
            } else if (c == 1001) {
                int maxDisplay = size.Y - 6;
                if (browseIndex < (int)browseItems.size() - 1) browseIndex++;
            } else if (c == 1003 || c == '\r') {
                if (browseAtDrives) {
                    if (!browseItems.empty()) {
                        browsePath = browseItems[browseIndex];
                        if (browsePath.back() == '\\') browsePath.pop_back();
                        browseAtDrives = false;
                        browseItems = GetSubFolders(browsePath);
                        browseIndex = 0;
                    }
                } else {
                    if (!browseItems.empty() && browseIndex >= 0 && browseIndex < (int)browseItems.size()) {
                        browsePath = browsePath + "\\" + browseItems[browseIndex];
                        browseItems = GetSubFolders(browsePath);
                        browseIndex = 0;
                    }
                }
            } else if (c == 1002) {
                if (!browseAtDrives) {
                    size_t lastSlash = browsePath.rfind('\\');
                    if (lastSlash != std::string::npos) {
                        browsePath = browsePath.substr(0, lastSlash);
                        if (browsePath.length() == 2 && browsePath[1] == ':') {
                            browseAtDrives = true;
                            browseItems = GetDrives();
                            browseIndex = 0;
                        } else {
                            browseItems = GetSubFolders(browsePath);
                            browseIndex = 0;
                        }
                    } else {
                        browseAtDrives = true;
                        browseItems = GetDrives();
                        browseIndex = 0;
                    }
                }
            } else if (c == 27) {
                inPathBrowse = false;
            }
        }
    }
}

static bool RunSortSelection() {
    int sortByIndex = g_sortBy;
    int compareIndex = g_compareType;
    int currentRow = 0;
    
    const char* sortByOptions[] = {
        "SIZE",
        "NAME",
        "PATH",
        "EXTEND_NAME",
        "CHANGED_TIME",
        "CREATED_TIME"
    };
    const int sortByCount = 6;
    
    const char* compareOptions[] = {
        "GREATER",
        "LESS"
    };
    const int compareCount = 2;
    
    while (true) {
        COORD size = GetConsoleSize();
        ClearScreen();
        HideCursor();
        
        SetCursor(0, 0);
        printf("Sort Options");
        
        SetCursor(0, 2);
        printf("SORT BY");
        
        for (int i = 0; i < sortByCount; i++) {
            SetCursor(0, 3 + i);
            bool isSelected = (currentRow == i);
            bool isChecked = (sortByIndex == i);
            
            if (isSelected) {
                printf("->[%c] %s", isChecked ? 'X' : ' ', sortByOptions[i]);
            } else {
                printf("  [%c] %s", isChecked ? 'X' : ' ', sortByOptions[i]);
            }
        }
        
        SetCursor(0, 10);
        printf("COMPARE:");
        
        for (int i = 0; i < compareCount; i++) {
            SetCursor(0, 11 + i);
            int row = sortByCount + i;
            bool isSelected = (currentRow == row);
            bool isChecked = (compareIndex == i);
            
            if (isSelected) {
                printf("->[%c] %s", isChecked ? 'X' : ' ', compareOptions[i]);
            } else {
                printf("  [%c] %s", isChecked ? 'X' : ' ', compareOptions[i]);
            }
        }
        
        SetCursor(0, size.Y - 1);
        printf("[Up/Down: Select] [Right: Choose] [Left/Esc/Enter: Back]");
        
        int c = GetKeyCode();
        
        if (c == 1000) {
            if (currentRow > 0) currentRow--;
        } else if (c == 1001) {
            if (currentRow < sortByCount + compareCount - 1) currentRow++;
        } else if (c == 1003) {
            if (currentRow < sortByCount) {
                sortByIndex = currentRow;
            } else {
                compareIndex = currentRow - sortByCount;
            }
        } else if (c == 1002 || c == '\r' || c == 27) {
            g_sortBy = sortByIndex;
            g_compareType = compareIndex;
            return true;
        }
    }
}

void search_run_ui(void* appState) {
    g_searchActive = true;
    
    std::string selectedPath = g_currentPath;
    int selectedScope = g_scope;
    
    if (!RunScopeSelection(selectedPath, selectedScope)) {
        g_searchActive = false;
        ClearScreen();
        ShowCursor();
        return;
    }
    
    g_scope = selectedScope;
    g_currentPath = selectedPath;
    
    if (!g_everythingAvailable) {
        {
            std::lock_guard<std::mutex> lock(g_indexMutex);
            g_indexedFiles.clear();
        }
        search_start_indexing();
    }
    
    std::string inputBuffer;
    int cursorPos = 0;
    
    search_set_keywords("");
    search_execute();
    
    enum FocusArea {
        FOCUS_INPUT,
        FOCUS_SORT,
        FOCUS_RESULTS
    };
    
    FocusArea focusArea = FOCUS_INPUT;
    int resultScrollOffset = 0;
    int resultSelectedIndex = 0;
    
    while (g_searchActive) {
        COORD size = GetConsoleSize();
        ClearScreen();
        HideCursor();
        
        SetCursor(0, 0);
        if (focusArea == FOCUS_INPUT) {
            printf("->");
        } else {
            printf("  ");
        }
        
        printf("%s", inputBuffer.c_str());
        
        if (focusArea == FOCUS_INPUT) {
            SetCursor(2 + cursorPos, 0);
            ShowCursor();
        } else {
            HideCursor();
        }
        
        SetCursor(0, 1);
        if (focusArea == FOCUS_SORT) {
            printf("->");
        } else {
            printf("  ");
        }
        
        const char* sortByStr = "SIZE";
        switch (g_sortBy) {
            case SORT_SIZE: sortByStr = "SIZE"; break;
            case SORT_NAME: sortByStr = "NAME"; break;
            case SORT_PATH: sortByStr = "PATH"; break;
            case SORT_EXTEND_NAME: sortByStr = "EXT"; break;
            case SORT_CHANGED_TIME: sortByStr = "MOD"; break;
            case SORT_CREATED_TIME: sortByStr = "CRE"; break;
        }
        
        const char* compareStr = (g_compareType == COMPARE_GREATER) ? "G" : "L";
        
        printf("[ ]sort: %s-%s", sortByStr, compareStr);
        
        if (g_everythingAvailable) {
            printf(" [Everything]");
        }
        
        if (g_isIndexing) {
            SetCursor(0, 2);
            printf("Indexing: %d files...", g_indexProgress.load());
        }
        
        std::vector<SearchFileInfo> results;
        {
            std::lock_guard<std::mutex> lock(g_resultMutex);
            results = g_searchResults;
        }
        
        int listStart = g_isIndexing ? 4 : 3;
        int listHeight = size.Y - listStart - 1;
        
        int startIdx = resultScrollOffset;
        int endIdx = std::min(startIdx + listHeight, (int)results.size());
        
        for (int i = startIdx; i < endIdx; i++) {
            int y = listStart + (i - startIdx);
            SetCursor(0, y);
            
            const auto& info = results[i];
            
            bool isSelected = (focusArea == FOCUS_RESULTS && i == resultSelectedIndex);
            
            if (isSelected) {
                HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
                SetConsoleTextAttribute(hConsole, BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
            }
            
            std::string displayLine;
            if (info.isDirectory) {
                displayLine = "[DIR] " + info.name + "\\";
            } else {
                displayLine = "[" + FormatSize(info.size) + "] " + info.name;
            }
            
            if ((int)displayLine.length() > size.X - 1) {
                displayLine = displayLine.substr(0, size.X - 4) + "...";
            }
            
            printf("%s", displayLine.c_str());
            
            if (isSelected) {
                HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
                SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
            }
        }
        
        SetCursor(0, size.Y - 1);
        if (focusArea == FOCUS_INPUT) {
            printf("[Type: Search] [Down: Sort] [Left/Right: Cursor] [i/o/p: Copy] [Esc: Quit]");
        } else if (focusArea == FOCUS_SORT) {
            printf("[Up: Input] [Down: Results] [Right: Sort Options] [q: Quit] [Esc: Back]");
        } else {
            printf("[Up: Sort] [Enter: Open] [Del: Recycle] [i/o/p: Copy] [q: Quit] [Esc: Back]");
        }
        
        int c = GetKeyCode();
        
        if (c == 27) {
            if (focusArea == FOCUS_INPUT) {
                g_searchActive = false;
            } else {
                focusArea = FOCUS_INPUT;
            }
        } else if (c == 'q' || c == 'Q') {
            if (focusArea != FOCUS_INPUT) {
                g_searchActive = false;
            }
        } else if (c == 1000) {
            if (focusArea == FOCUS_SORT) {
                focusArea = FOCUS_INPUT;
            } else if (focusArea == FOCUS_RESULTS) {
                if (resultSelectedIndex > 0) {
                    resultSelectedIndex--;
                    if (resultSelectedIndex < resultScrollOffset) {
                        resultScrollOffset = resultSelectedIndex;
                    }
                }
            }
        } else if (c == 1001) {
            if (focusArea == FOCUS_INPUT) {
                focusArea = FOCUS_SORT;
            } else if (focusArea == FOCUS_SORT) {
                focusArea = FOCUS_RESULTS;
                if (resultSelectedIndex >= (int)results.size()) {
                    resultSelectedIndex = (int)results.size() - 1;
                }
                if (resultSelectedIndex < 0) resultSelectedIndex = 0;
            } else if (focusArea == FOCUS_RESULTS) {
                if (resultSelectedIndex < (int)results.size() - 1) {
                    resultSelectedIndex++;
                    if (resultSelectedIndex >= resultScrollOffset + listHeight) {
                        resultScrollOffset = resultSelectedIndex - listHeight + 1;
                    }
                }
            }
        } else if (c == 1002) {
            if (focusArea == FOCUS_INPUT && cursorPos > 0) {
                cursorPos--;
            }
        } else if (c == 1003) {
            if (focusArea == FOCUS_SORT) {
                if (RunSortSelection()) {
                    search_execute();
                }
            }
        } else if (c == '\r') {
            if (focusArea == FOCUS_RESULTS && resultSelectedIndex >= 0 && resultSelectedIndex < (int)results.size()) {
                const auto& info = results[resultSelectedIndex];
                ShellExecuteA(NULL, "open", info.path.c_str(), NULL, NULL, SW_SHOWNORMAL);
            }
        } else if (c == 1004) {
            if (focusArea == FOCUS_RESULTS && resultSelectedIndex >= 0 && resultSelectedIndex < (int)results.size()) {
                const auto& info = results[resultSelectedIndex];
                MoveToRecycleBin(info.path.c_str());
                search_execute();
            }
        } else if (c == 'i' || c == 'I') {
            if (focusArea == FOCUS_RESULTS && resultSelectedIndex >= 0 && resultSelectedIndex < (int)results.size()) {
                const auto& info = results[resultSelectedIndex];
                CopyToClipboard(info.name.c_str());
            }
        } else if (c == 'o' || c == 'O') {
            if (focusArea == FOCUS_RESULTS && resultSelectedIndex >= 0 && resultSelectedIndex < (int)results.size()) {
                const auto& info = results[resultSelectedIndex];
                CopyToClipboard(info.path.c_str());
            }
        } else if (c == 'p' || c == 'P') {
            if (focusArea == FOCUS_RESULTS && resultSelectedIndex >= 0 && resultSelectedIndex < (int)results.size()) {
                const auto& info = results[resultSelectedIndex];
                std::string fullPath = info.path + "\\" + info.name;
                CopyToClipboard(fullPath.c_str());
            }
        } else if (c == '\b') {
            if (focusArea == FOCUS_INPUT && cursorPos > 0) {
                inputBuffer.erase(cursorPos - 1, 1);
                cursorPos--;
                search_set_keywords(inputBuffer.c_str());
                search_execute();
                resultSelectedIndex = 0;
                resultScrollOffset = 0;
            }
        } else if (c >= 32 && c < 127) {
            if (focusArea == FOCUS_INPUT) {
                inputBuffer.insert(cursorPos, 1, (char)c);
                cursorPos++;
                search_set_keywords(inputBuffer.c_str());
                search_execute();
                resultSelectedIndex = 0;
                resultScrollOffset = 0;
            }
        }
    }
    
    ClearScreen();
    ShowCursor();
}
