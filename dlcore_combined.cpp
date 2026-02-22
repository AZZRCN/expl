/**
 * @file dlcore_combined.cpp
 * @brief DLCore - A multi-threaded download library for Windows (Combined)
 */

#define DLCORE_EXPORTS
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <winhttp.h>
#include <wincrypt.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "advapi32.lib")
#endif

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <queue>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <condition_variable>
#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <algorithm>
#include <memory>
#include <random>
#include <stdexcept>
#include <array>
#include <iomanip>
#include <functional>
#include <cstdint>

namespace dl {

enum class Status {
    Pending,
    Downloading,
    Paused,
    Completed,
    Error,
    Cancelled
};

struct TaskInfo {
    std::string id;
    std::string url;
    std::string fileName;
    std::string savePath;
    uint64_t totalSize;
    uint64_t downloadedSize;
    uint64_t speed;
    uint64_t remainingTime;
    Status status;
    int progress;
    int threadCount;
    std::string errorMessage;
    std::string expectedMd5;
    bool verified;
    int retryCount;
    
    TaskInfo() 
        : totalSize(0), downloadedSize(0), speed(0), remainingTime(0)
        , status(Status::Pending), progress(0), threadCount(0)
        , verified(false), retryCount(0) {}
};

enum class ProxyType {
    None,
    HTTP,
    HTTPS,
    SOCKS4,
    SOCKS5
};

struct ProxyConfig {
    ProxyType type;
    std::string host;
    int port;
    std::string username;
    std::string password;
    
    ProxyConfig() : type(ProxyType::None), port(0) {}
};

enum class LogLevel {
    None = 0,
    Error = 1,
    Warning = 2,
    Info = 3,
    Debug = 4
};

struct LogConfig {
    LogLevel level;
    bool logToFile;
    bool logToConsole;
    std::string logFilePath;
    
    LogConfig() 
        : level(LogLevel::Info)
        , logToFile(false)
        , logToConsole(true) {}
};

struct Config {
    int maxConcurrentDownloads;
    int speedLimitKB;
    int defaultThreadCount;
    std::string defaultSavePath;
    ProxyConfig proxy;
    LogConfig logging;
    
    int maxRetries;
    int retryDelayMs;
    bool verifySsl;
    bool verifyChecksum;
    int connectTimeoutMs;
    int readTimeoutMs;
    
    Config() 
        : maxConcurrentDownloads(3)
        , speedLimitKB(0)
        , defaultThreadCount(4)
        , defaultSavePath(".")
        , maxRetries(3)
        , retryDelayMs(1000)
        , verifySsl(true)
        , verifyChecksum(true)
        , connectTimeoutMs(30000)
        , readTimeoutMs(30000)
    {}
};

using ProgressCallback = std::function<void(const std::string& taskId, int progress, uint64_t downloaded, uint64_t total, uint64_t speed)>;
using CompleteCallback = std::function<void(const std::string& taskId, const std::string& filePath)>;
using ErrorCallback = std::function<void(const std::string& taskId, const std::string& errorMessage, bool willRetry)>;
using StatusCallback = std::function<void(const std::string& taskId, Status status)>;
using LogCallback = std::function<void(LogLevel level, const std::string& message)>;

class DownloadManager {
public:
    static DownloadManager* create();
    static void destroy(DownloadManager* manager);
    
    virtual ~DownloadManager() = default;
    
    virtual void setConfig(const Config& config) = 0;
    virtual Config getConfig() const = 0;
    
    virtual void setProgressCallback(ProgressCallback callback) = 0;
    virtual void setCompleteCallback(CompleteCallback callback) = 0;
    virtual void setErrorCallback(ErrorCallback callback) = 0;
    virtual void setStatusCallback(StatusCallback callback) = 0;
    virtual void setLogCallback(LogCallback callback) = 0;
    
    virtual std::string addTask(const std::string& url, 
                                 const std::string& savePath, 
                                 int threads = 0) = 0;
    
    virtual std::string addTaskWithMd5(const std::string& url,
                                        const std::string& savePath,
                                        int threads,
                                        const std::string& expectedMd5) = 0;
    
    virtual bool pauseTask(const std::string& taskId) = 0;
    virtual bool resumeTask(const std::string& taskId) = 0;
    virtual bool cancelTask(const std::string& taskId) = 0;
    virtual bool removeTask(const std::string& taskId) = 0;
    virtual bool retryTask(const std::string& taskId) = 0;
    
    virtual TaskInfo getTaskInfo(const std::string& taskId) const = 0;
    virtual std::vector<TaskInfo> getAllTasks() const = 0;
    
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
    
    virtual void waitForTask(const std::string& taskId, int timeoutMs = -1) = 0;
    virtual void waitForAll(int timeoutMs = -1) = 0;

protected:
    DownloadManager() = default;
    DownloadManager(const DownloadManager&) = delete;
    DownloadManager& operator=(const DownloadManager&) = delete;
};

bool Initialize();
void Cleanup();
std::string GetVersion();
std::string CalculateFileMd5(const std::string& filePath);

constexpr int BUFFER_SIZE = 65536;
constexpr int DEFAULT_CONNECT_TIMEOUT_MS = 30000;
constexpr int DEFAULT_READ_TIMEOUT_MS = 30000;
constexpr int SPEED_UPDATE_INTERVAL_MS = 500;
constexpr int META_SAVE_INTERVAL_MS = 1000;
constexpr int MAX_THREAD_COUNT = 16;
constexpr int MIN_THREAD_COUNT = 1;
constexpr const char* VERSION = "1.1.0";

namespace Utils {

inline std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return std::wstring();
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (len <= 0) return std::wstring();
    std::wstring result(len - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], len);
    return result;
}

inline std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return std::string();
    std::string result(len - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], len, nullptr, nullptr);
    return result;
}

inline std::string GenerateUniqueId() {
    static std::atomic<uint64_t> counter{0};
    static std::mt19937 gen(std::random_device{}());
    static std::mutex mtx;
    
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    uint64_t cnt = counter.fetch_add(1, std::memory_order_relaxed);
    
    std::lock_guard<std::mutex> lock(mtx);
    std::uniform_int_distribution<> dis(1000, 9999);
    return std::to_string(ms) + "_" + std::to_string(cnt) + "_" + std::to_string(dis(gen));
}

inline std::string ExtractFileName(const std::string& url) {
    size_t pos = url.rfind('/');
    if (pos == std::string::npos || pos >= url.length() - 1) {
        return "download_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    }
    std::string name = url.substr(pos + 1);
    size_t queryPos = name.find('?');
    if (queryPos != std::string::npos) name = name.substr(0, queryPos);
    size_t fragPos = name.find('#');
    if (fragPos != std::string::npos) name = name.substr(0, fragPos);
    if (name.empty()) {
        return "download_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    }
    return name;
}

inline bool EnsureDirectoryExists(const std::string& path) {
    if (path.empty()) return false;
    std::wstring wpath = Utf8ToWide(path);
    DWORD attr = GetFileAttributesW(wpath.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        return true;
    }
    if (CreateDirectoryW(wpath.c_str(), nullptr)) {
        return true;
    }
    DWORD err = GetLastError();
    if (err == ERROR_ALREADY_EXISTS) {
        return true;
    }
    if (err == ERROR_PATH_NOT_FOUND) {
        size_t pos = path.find_last_of("\\/");
        if (pos != std::string::npos && pos > 0) {
            if (EnsureDirectoryExists(path.substr(0, pos))) {
                return CreateDirectoryW(wpath.c_str(), nullptr) != 0 || 
                       GetLastError() == ERROR_ALREADY_EXISTS;
            }
        }
    }
    return false;
}

inline std::string SafeToString(const std::string& str) {
    std::string result;
    result.reserve(str.size());
    for (char c : str) {
        if (c >= 32 && c <= 126) {
            result += c;
        }
    }
    return result;
}

inline std::string FormatTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_s(&tm, &time);
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
    return std::string(buffer);
}

}

class Logger {
private:
    LogConfig config_;
    std::mutex mutex_;
    std::ofstream file_;
    LogCallback callback_;
    
public:
    Logger() = default;
    
    void setConfig(const LogConfig& config) {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        if (config_.logToFile && !config_.logFilePath.empty()) {
            file_.open(config_.logFilePath, std::ios::app);
        }
    }
    
    void setCallback(LogCallback callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        callback_ = std::move(callback);
    }
    
    void log(LogLevel level, const std::string& message) {
        if (level > config_.level || config_.level == LogLevel::None) {
            return;
        }
        
        std::string levelStr;
        switch (level) {
            case LogLevel::Error: levelStr = "ERROR"; break;
            case LogLevel::Warning: levelStr = "WARN"; break;
            case LogLevel::Info: levelStr = "INFO"; break;
            case LogLevel::Debug: levelStr = "DEBUG"; break;
            default: levelStr = "UNKNOWN"; break;
        }
        
        std::string fullMessage = "[" + Utils::FormatTimestamp() + "] [" + levelStr + "] " + message;
        
        if (config_.logToConsole) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (level == LogLevel::Error) {
                std::cerr << fullMessage << std::endl;
            } else {
                std::cout << fullMessage << std::endl;
            }
        }
        
        if (config_.logToFile && file_.is_open()) {
            std::lock_guard<std::mutex> lock(mutex_);
            file_ << fullMessage << std::endl;
            file_.flush();
        }
        
        if (callback_) {
            callback_(level, message);
        }
    }
    
    void error(const std::string& msg) { log(LogLevel::Error, msg); }
    void warning(const std::string& msg) { log(LogLevel::Warning, msg); }
    void info(const std::string& msg) { log(LogLevel::Info, msg); }
    void debug(const std::string& msg) { log(LogLevel::Debug, msg); }
};

static Logger g_logger;

#define LOG_ERROR(msg) g_logger.error(msg)
#define LOG_WARN(msg) g_logger.warning(msg)
#define LOG_INFO(msg) g_logger.info(msg)
#define LOG_DEBUG(msg) g_logger.debug(msg)

class WinHttpHandle {
public:
    WinHttpHandle() : handle_(nullptr) {}
    explicit WinHttpHandle(HINTERNET h) : handle_(h) {}
    ~WinHttpHandle() { close(); }
    
    WinHttpHandle(const WinHttpHandle&) = delete;
    WinHttpHandle& operator=(const WinHttpHandle&) = delete;
    
    WinHttpHandle(WinHttpHandle&& other) noexcept : handle_(other.handle_) { 
        other.handle_ = nullptr; 
    }
    
    WinHttpHandle& operator=(WinHttpHandle&& other) noexcept {
        if (this != &other) { 
            close(); 
            handle_ = other.handle_; 
            other.handle_ = nullptr; 
        }
        return *this;
    }
    
    bool isValid() const { return handle_ != nullptr; }
    HINTERNET get() const { return handle_; }
    void close() { 
        if (handle_) { 
            WinHttpCloseHandle(handle_); 
            handle_ = nullptr; 
        } 
    }
    void reset(HINTERNET h = nullptr) { close(); handle_ = h; }
    
private:
    HINTERNET handle_;
};

struct DownloadSegment {
    uint64_t startByte;
    uint64_t endByte;
    std::atomic<uint64_t> downloadedBytes{0};
    std::string tempFile;
    std::atomic<bool> completed{false};
    std::atomic<bool> active{false};
    std::string errorMessage;
    
    DownloadSegment() : startByte(0), endByte(0) {}
    uint64_t totalSize() const { return endByte - startByte + 1; }
    bool isComplete() const { return downloadedBytes.load() >= totalSize(); }
};

struct DownloadTask {
    std::string id;
    std::string url;
    std::string fileName;
    std::string savePath;
    uint64_t totalSize;
    std::atomic<uint64_t> downloadedSize{0};
    std::atomic<uint64_t> speed{0};
    std::atomic<uint64_t> remainingTime{0};
    std::atomic<Status> status{Status::Pending};
    std::atomic<int> progress{0};
    int threadCount;
    std::vector<std::shared_ptr<DownloadSegment>> segments;
    std::chrono::system_clock::time_point startTime;
    std::chrono::steady_clock::time_point lastSpeedUpdate;
    std::chrono::steady_clock::time_point lastMetaSave;
    uint64_t lastDownloadedForSpeed;
    std::string errorMessage;
    bool supportsRange;
    bool isHttps;
    std::mutex segmentMutex;
    std::condition_variable segmentCV;
    std::atomic<int> activeSegmentCount{0};
    std::atomic<bool> merging{false};
    std::atomic<bool> cancelRequested{false};
    
    std::string expectedMd5;
    std::atomic<bool> verified{false};
    std::atomic<int> retryCount{0};
    
    std::condition_variable taskCV;
    std::mutex taskMutex;
    
    DownloadTask() : totalSize(0), threadCount(4), supportsRange(false), isHttps(false) {
        id = Utils::GenerateUniqueId();
        startTime = std::chrono::system_clock::now();
        lastSpeedUpdate = std::chrono::steady_clock::now();
        lastMetaSave = std::chrono::steady_clock::now();
        lastDownloadedForSpeed = 0;
    }
    
    std::string getFullPath() const {
        if (savePath.empty()) return "";
        std::string path = savePath;
        if (path.back() != '\\' && path.back() != '/') {
            path += "\\";
        }
        return path + fileName;
    }
    
    std::string getMetaPath() const { return getFullPath() + ".dlmeta"; }
    
    Status getStatus() const { return status.load(std::memory_order_acquire); }
    void setStatus(Status s) { 
        status.store(s, std::memory_order_release); 
        taskCV.notify_all();
    }
    int getProgress() const { return progress.load(std::memory_order_acquire); }
    void setProgress(int p) { progress.store(p, std::memory_order_release); }
    
    bool isDownloading() const { return getStatus() == Status::Downloading; }
    bool isPaused() const { return getStatus() == Status::Paused; }
    bool isCompleted() const { return getStatus() == Status::Completed; }
    bool isError() const { return getStatus() == Status::Error; }
    bool isCancelled() const { return getStatus() == Status::Cancelled; }
    bool canPause() const { return getStatus() == Status::Downloading; }
    bool canResume() const { return isPaused(); }
    bool canCancel() const { 
        Status s = getStatus();
        return s == Status::Downloading || s == Status::Paused || s == Status::Pending; 
    }
    bool canRetry() const {
        Status s = getStatus();
        return s == Status::Error;
    }
    
    TaskInfo getTaskInfo() const {
        TaskInfo info;
        info.id = id;
        info.url = url;
        info.fileName = fileName;
        info.savePath = savePath;
        info.totalSize = totalSize;
        info.downloadedSize = downloadedSize.load();
        info.speed = speed.load();
        info.remainingTime = remainingTime.load();
        info.status = getStatus();
        info.progress = getProgress();
        info.threadCount = threadCount;
        info.errorMessage = errorMessage;
        info.expectedMd5 = expectedMd5;
        info.verified = verified.load();
        info.retryCount = retryCount.load();
        return info;
    }
};

struct ParsedUrl {
    std::wstring scheme;
    std::wstring hostName;
    std::wstring urlPath;
    int port;
    bool isHttps;
};

class FileWrapper {
public:
    explicit FileWrapper(FILE* f = nullptr) : file_(f) {}
    ~FileWrapper() { close(); }
    
    FileWrapper(const FileWrapper&) = delete;
    FileWrapper& operator=(const FileWrapper&) = delete;
    
    FileWrapper(FileWrapper&& other) noexcept : file_(other.file_) { 
        other.file_ = nullptr; 
    }
    
    FileWrapper& operator=(FileWrapper&& other) noexcept {
        if (this != &other) {
            close();
            file_ = other.file_;
            other.file_ = nullptr;
        }
        return *this;
    }
    
    bool isOpen() const { return file_ != nullptr; }
    FILE* get() const { return file_; }
    
    void close() {
        if (file_) {
            fclose(file_);
            file_ = nullptr;
        }
    }
    
    void reset(FILE* f = nullptr) {
        close();
        file_ = f;
    }
    
private:
    FILE* file_;
};

class HttpClient {
public:
    static bool ParseUrl(const std::string& url, ParsedUrl& result) {
        if (url.empty()) return false;
        
        std::wstring urlW = Utils::Utf8ToWide(url);
        URL_COMPONENTS urlComp;
        ZeroMemory(&urlComp, sizeof(urlComp));
        urlComp.dwStructSize = sizeof(urlComp);
        
        std::array<WCHAR, 256> hostName;
        std::array<WCHAR, 2048> urlPath;
        std::array<WCHAR, 256> extraInfo;
        
        urlComp.dwHostNameLength = static_cast<DWORD>(hostName.size());
        urlComp.dwUrlPathLength = static_cast<DWORD>(urlPath.size());
        urlComp.dwExtraInfoLength = static_cast<DWORD>(extraInfo.size());
        urlComp.lpszHostName = hostName.data();
        urlComp.lpszUrlPath = urlPath.data();
        urlComp.lpszExtraInfo = extraInfo.data();
        
        if (!WinHttpCrackUrl(urlW.c_str(), 0, 0, &urlComp)) {
            return false;
        }
        
        result.scheme = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? L"https" : L"http";
        result.hostName = hostName.data();
        result.urlPath = urlPath.data();
        if (result.urlPath.empty()) {
            result.urlPath = L"/";
        }
        result.isHttps = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);
        result.port = urlComp.nPort;
        if (result.port == 0) {
            result.port = result.isHttps ? 443 : 80;
        }
        return true;
    }
    
    static WinHttpHandle CreateSession(const std::wstring& userAgent = L"DLCore/1.1") {
        return WinHttpHandle(WinHttpOpen(userAgent.c_str(),
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS, 0));
    }
    
    static WinHttpHandle CreateConnection(HINTERNET hSession, const std::wstring& hostName, int port) {
        if (!hSession) return WinHttpHandle();
        return WinHttpHandle(WinHttpConnect(hSession, hostName.c_str(),
            static_cast<INTERNET_PORT>(port), 0));
    }
    
    static WinHttpHandle CreateRequest(HINTERNET hConnect, const std::wstring& urlPath, bool isHttps,
        const std::wstring& method = L"GET") {
        if (!hConnect) return WinHttpHandle();
        DWORD flags = isHttps ? WINHTTP_FLAG_SECURE : 0;
        return WinHttpHandle(WinHttpOpenRequest(hConnect, method.c_str(), urlPath.c_str(),
            NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
    }
    
    static bool SetTimeouts(HINTERNET hRequest, int connectMs, int sendMs, int recvMs) {
        if (!hRequest) return false;
        DWORD connectTimeout = static_cast<DWORD>(connectMs);
        DWORD sendTimeout = static_cast<DWORD>(sendMs);
        DWORD recvTimeout = static_cast<DWORD>(recvMs);
        WinHttpSetOption(hRequest, WINHTTP_OPTION_CONNECT_TIMEOUT, &connectTimeout, sizeof(connectTimeout));
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SEND_TIMEOUT, &sendTimeout, sizeof(sendTimeout));
        WinHttpSetOption(hRequest, WINHTTP_OPTION_RECEIVE_TIMEOUT, &recvTimeout, sizeof(recvTimeout));
        return true;
    }
    
    static bool AddRangeHeader(HINTERNET hRequest, uint64_t start, uint64_t end) {
        if (!hRequest) return false;
        std::wstring rangeHeader = L"Range: bytes=" + std::to_wstring(start) + L"-" + std::to_wstring(end);
        return WinHttpAddRequestHeaders(hRequest, rangeHeader.c_str(), -1L, WINHTTP_ADDREQ_FLAG_ADD) == TRUE;
    }
    
    static bool SetProxy(HINTERNET hSession, const ProxyConfig& proxy) {
        if (!hSession) return false;
        
        if (proxy.type == ProxyType::None) {
            return true;
        }
        
        if (proxy.type == ProxyType::SOCKS4 || proxy.type == ProxyType::SOCKS5) {
            return true;
        }
        
        std::wstring proxyStr = Utils::Utf8ToWide(proxy.host) + L":" + std::to_wstring(proxy.port);
        WINHTTP_PROXY_INFO proxyInfo = {};
        proxyInfo.dwAccessType = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
        proxyInfo.lpszProxy = const_cast<LPWSTR>(proxyStr.c_str());
        proxyInfo.lpszProxyBypass = WINHTTP_NO_PROXY_BYPASS;
        
        return WinHttpSetOption(hSession, WINHTTP_OPTION_PROXY, &proxyInfo, sizeof(proxyInfo)) == TRUE;
    }
    
    static bool ConfigureHttps(HINTERNET hRequest, bool verifySsl) {
        if (!hRequest) return false;
        
        if (verifySsl) {
            DWORD secFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA;
            return WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags)) == TRUE;
        } else {
            DWORD secFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                            SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                            SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
            return WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags)) == TRUE;
        }
    }
};

class Socks4Client {
public:
    static bool ConnectViaSocks4(SOCKET sock, const std::string& targetHost, int targetPort,
                                  const std::string& proxyHost, int proxyPort) {
        sockaddr_in proxyAddr;
        proxyAddr.sin_family = AF_INET;
        proxyAddr.sin_port = htons(static_cast<u_short>(proxyPort));
        
        hostent* he = gethostbyname(proxyHost.c_str());
        if (!he) return false;
        proxyAddr.sin_addr = **(in_addr**)he->h_addr_list;
        
        if (connect(sock, (sockaddr*)&proxyAddr, sizeof(proxyAddr)) != 0) {
            return false;
        }
        
        std::vector<uint8_t> connectRequest;
        connectRequest.push_back(0x04);
        connectRequest.push_back(0x01);
        connectRequest.push_back(static_cast<uint8_t>((targetPort >> 8) & 0xFF));
        connectRequest.push_back(static_cast<uint8_t>(targetPort & 0xFF));
        
        hostent* targetHe = gethostbyname(targetHost.c_str());
        if (!targetHe) return false;
        in_addr targetAddr = **(in_addr**)targetHe->h_addr_list;
        connectRequest.push_back(reinterpret_cast<uint8_t*>(&targetAddr)[0]);
        connectRequest.push_back(reinterpret_cast<uint8_t*>(&targetAddr)[1]);
        connectRequest.push_back(reinterpret_cast<uint8_t*>(&targetAddr)[2]);
        connectRequest.push_back(reinterpret_cast<uint8_t*>(&targetAddr)[3]);
        
        connectRequest.push_back(0x00);
        
        if (send(sock, (char*)connectRequest.data(), static_cast<int>(connectRequest.size()), 0) !=
            static_cast<int>(connectRequest.size())) {
            return false;
        }
        
        uint8_t response[8];
        if (recv(sock, (char*)response, 8, 0) != 8) {
            return false;
        }
        
        if (response[0] != 0x00 || response[1] != 0x5A) {
            return false;
        }
        
        return true;
    }
};

class Socks5Client {
public:
    static bool ConnectViaSocks5(SOCKET sock, const std::string& targetHost, int targetPort,
                                  const std::string& proxyHost, int proxyPort,
                                  const std::string& username, const std::string& password) {
        sockaddr_in proxyAddr;
        proxyAddr.sin_family = AF_INET;
        proxyAddr.sin_port = htons(static_cast<u_short>(proxyPort));
        
        hostent* he = gethostbyname(proxyHost.c_str());
        if (!he) return false;
        proxyAddr.sin_addr = **(in_addr**)he->h_addr_list;
        
        if (connect(sock, (sockaddr*)&proxyAddr, sizeof(proxyAddr)) != 0) {
            return false;
        }
        
        std::vector<uint8_t> authRequest;
        authRequest.push_back(0x05);
        if (!username.empty()) {
            authRequest.push_back(0x02);
            authRequest.push_back(0x00);
            authRequest.push_back(0x02);
        } else {
            authRequest.push_back(0x01);
            authRequest.push_back(0x00);
        }
        
        if (send(sock, (char*)authRequest.data(), static_cast<int>(authRequest.size()), 0) != 
            static_cast<int>(authRequest.size())) {
            return false;
        }
        
        uint8_t authResponse[2];
        if (recv(sock, (char*)authResponse, 2, 0) != 2) {
            return false;
        }
        
        if (authResponse[0] != 0x05) {
            return false;
        }
        
        if (authResponse[1] == 0x02) {
            if (username.empty()) return false;
            
            std::vector<uint8_t> authData;
            authData.push_back(0x01);
            authData.push_back(static_cast<uint8_t>(username.length()));
            authData.insert(authData.end(), username.begin(), username.end());
            authData.push_back(static_cast<uint8_t>(password.length()));
            authData.insert(authData.end(), password.begin(), password.end());
            
            if (send(sock, (char*)authData.data(), static_cast<int>(authData.size()), 0) !=
                static_cast<int>(authData.size())) {
                return false;
            }
            
            uint8_t authResult[2];
            if (recv(sock, (char*)authResult, 2, 0) != 2 || authResult[1] != 0x00) {
                return false;
            }
        } else if (authResponse[1] != 0x00) {
            return false;
        }
        
        std::vector<uint8_t> connectRequest;
        connectRequest.push_back(0x05);
        connectRequest.push_back(0x01);
        connectRequest.push_back(0x00);
        connectRequest.push_back(0x03);
        connectRequest.push_back(static_cast<uint8_t>(targetHost.length()));
        connectRequest.insert(connectRequest.end(), targetHost.begin(), targetHost.end());
        connectRequest.push_back(static_cast<uint8_t>((targetPort >> 8) & 0xFF));
        connectRequest.push_back(static_cast<uint8_t>(targetPort & 0xFF));
        
        if (send(sock, (char*)connectRequest.data(), static_cast<int>(connectRequest.size()), 0) !=
            static_cast<int>(connectRequest.size())) {
            return false;
        }
        
        uint8_t connectResponse[10];
        if (recv(sock, (char*)connectResponse, 10, 0) < 2) {
            return false;
        }
        
        if (connectResponse[0] != 0x05 || connectResponse[1] != 0x00) {
            return false;
        }
        
        return true;
    }
};

class Md5Calculator {
public:
    static std::string Calculate(const std::string& filePath) {
        HCRYPTPROV hProv = 0;
        HCRYPTHASH hHash = 0;
        
        if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
            LOG_ERROR("CryptAcquireContext failed: " + std::to_string(GetLastError()));
            return "";
        }
        
        if (!CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash)) {
            LOG_ERROR("CryptCreateHash failed: " + std::to_string(GetLastError()));
            CryptReleaseContext(hProv, 0);
            return "";
        }
        
        std::wstring wpath = Utils::Utf8ToWide(filePath);
        HANDLE hFile = CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ, 
                                    NULL, OPEN_EXISTING, 0, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            LOG_ERROR("CreateFile failed for MD5: " + std::to_string(GetLastError()));
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            return "";
        }
        
        BYTE buffer[65536];
        DWORD bytesRead;
        BOOL success = TRUE;
        
        while (success && ReadFile(hFile, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
            success = CryptHashData(hHash, buffer, bytesRead, 0);
        }
        
        CloseHandle(hFile);
        
        if (!success) {
            LOG_ERROR("CryptHashData failed: " + std::to_string(GetLastError()));
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            return "";
        }
        
        DWORD hashLen = 16;
        BYTE hashData[16];
        DWORD hashLenSize = sizeof(DWORD);
        
        if (!CryptGetHashParam(hHash, HP_HASHSIZE, (BYTE*)&hashLen, &hashLenSize, 0)) {
            LOG_ERROR("CryptGetHashParam HP_HASHSIZE failed: " + std::to_string(GetLastError()));
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            return "";
        }
        
        if (!CryptGetHashParam(hHash, HP_HASHVAL, hashData, &hashLen, 0)) {
            LOG_ERROR("CryptGetHashParam HP_HASHVAL failed: " + std::to_string(GetLastError()));
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            return "";
        }
        
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        for (DWORD i = 0; i < hashLen; i++) {
            ss << std::setw(2) << static_cast<int>(hashData[i]);
        }
        
        std::string result = ss.str();
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        
        return result;
    }
};

class DownloadManagerImpl : public DownloadManager {
private:
    std::unordered_map<std::string, std::shared_ptr<DownloadTask>> tasksMap_;
    std::vector<std::shared_ptr<DownloadTask>> tasksList_;
    mutable std::shared_mutex tasksMutex_;
    std::queue<std::string> pendingQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCV_;
    std::atomic<bool> running_{false};
    std::vector<std::thread> workerThreads_;
    Config config_;
    std::atomic<int> activeDownloads_{0};
    std::atomic<uint64_t> globalSpeedLimit_{0};
    std::atomic<uint64_t> currentSpeed_{0};
    std::chrono::steady_clock::time_point lastSpeedCheck_;
    std::mutex speedMutex_;
    
    ProgressCallback progressCallback_;
    CompleteCallback completeCallback_;
    ErrorCallback errorCallback_;
    StatusCallback statusCallback_;
    LogCallback logCallback_;
    std::mutex callbackMutex_;
    
public:
    DownloadManagerImpl() { 
        lastSpeedCheck_ = std::chrono::steady_clock::now(); 
    }
    
    ~DownloadManagerImpl() override { 
        stop(); 
    }
    
    void setConfig(const Config& config) override {
        config_ = config;
        globalSpeedLimit_ = static_cast<uint64_t>(config_.speedLimitKB) * 1024;
        g_logger.setConfig(config.logging);
        LOG_INFO("Configuration updated: maxConcurrent=" + std::to_string(config.maxConcurrentDownloads) +
                 ", speedLimit=" + std::to_string(config.speedLimitKB) + "KB/s" +
                 ", maxRetries=" + std::to_string(config.maxRetries) +
                 ", verifySsl=" + (config.verifySsl ? "true" : "false"));
    }
    
    Config getConfig() const override {
        return config_;
    }
    
    void setProgressCallback(ProgressCallback callback) override {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        progressCallback_ = std::move(callback);
    }
    
    void setCompleteCallback(CompleteCallback callback) override {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        completeCallback_ = std::move(callback);
    }
    
    void setErrorCallback(ErrorCallback callback) override {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        errorCallback_ = std::move(callback);
    }
    
    void setStatusCallback(StatusCallback callback) override {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        statusCallback_ = std::move(callback);
    }
    
    void setLogCallback(LogCallback callback) override {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        logCallback_ = std::move(callback);
        g_logger.setCallback(logCallback_);
    }
    
    std::string addTask(const std::string& url, const std::string& savePath, int threads) override {
        return addTaskWithMd5(url, savePath, threads, "");
    }
    
    std::string addTaskWithMd5(const std::string& url, const std::string& savePath, 
                                int threads, const std::string& expectedMd5) override {
        if (url.empty()) {
            LOG_ERROR("addTask failed: URL is empty");
            return "";
        }
        
        std::string path = savePath.empty() ? config_.defaultSavePath : savePath;
        if (path.empty()) {
            LOG_ERROR("addTask failed: save path is empty");
            return "";
        }
        
        int threadCount = threads > 0 ? threads : config_.defaultThreadCount;
        threadCount = std::max(MIN_THREAD_COUNT, std::min(threadCount, MAX_THREAD_COUNT));
        
        auto task = std::make_shared<DownloadTask>();
        task->url = url;
        task->savePath = path;
        task->threadCount = threadCount;
        task->fileName = Utils::SafeToString(Utils::ExtractFileName(url));
        task->isHttps = (url.find("https://") == 0);
        task->expectedMd5 = expectedMd5;
        
        std::string taskId = task->id;
        
        {
            std::unique_lock<std::shared_mutex> lock(tasksMutex_);
            tasksMap_[taskId] = task;
            tasksList_.push_back(task);
        }
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            pendingQueue_.push(taskId);
        }
        queueCV_.notify_one();
        
        LOG_INFO("Task added: " + taskId + " URL=" + url + " threads=" + std::to_string(threadCount));
        notifyStatus(taskId, Status::Pending);
        
        return taskId;
    }
    
    bool pauseTask(const std::string& taskId) override {
        auto task = getTask(taskId);
        if (task && task->canPause()) {
            task->cancelRequested = true;
            task->setStatus(Status::Paused);
            saveTaskMeta(task);
            LOG_INFO("Task paused: " + taskId);
            notifyStatus(taskId, Status::Paused);
            return true;
        }
        LOG_WARN("pauseTask failed: task not found or cannot pause: " + taskId);
        return false;
    }
    
    bool resumeTask(const std::string& taskId) override {
        auto task = getTask(taskId);
        if (task && task->canResume()) {
            task->cancelRequested = false;
            task->setStatus(Status::Pending);
            {
                std::lock_guard<std::mutex> lock(queueMutex_);
                pendingQueue_.push(task->id);
            }
            queueCV_.notify_one();
            LOG_INFO("Task resumed: " + taskId);
            notifyStatus(taskId, Status::Pending);
            return true;
        }
        LOG_WARN("resumeTask failed: task not found or cannot resume: " + taskId);
        return false;
    }
    
    bool cancelTask(const std::string& taskId) override {
        auto task = getTask(taskId);
        if (task && task->canCancel()) {
            task->cancelRequested = true;
            task->setStatus(Status::Cancelled);
            LOG_INFO("Task cancelled: " + taskId);
            notifyStatus(taskId, Status::Cancelled);
            return true;
        }
        LOG_WARN("cancelTask failed: task not found or cannot cancel: " + taskId);
        return false;
    }
    
    bool removeTask(const std::string& taskId) override {
        std::unique_lock<std::shared_mutex> lock(tasksMutex_);
        
        auto mapIt = tasksMap_.find(taskId);
        if (mapIt == tasksMap_.end()) {
            return false;
        }
        tasksMap_.erase(mapIt);
        
        auto listIt = std::remove_if(tasksList_.begin(), tasksList_.end(),
            [&taskId](const std::shared_ptr<DownloadTask>& t) { return t->id == taskId; });
        if (listIt != tasksList_.end()) {
            tasksList_.erase(listIt, tasksList_.end());
            LOG_INFO("Task removed: " + taskId);
            return true;
        }
        return false;
    }
    
    bool retryTask(const std::string& taskId) override {
        auto task = getTask(taskId);
        if (task && task->canRetry()) {
            if (task->retryCount >= config_.maxRetries) {
                LOG_WARN("retryTask failed: max retries exceeded for task: " + taskId);
                return false;
            }
            
            task->cancelRequested = false;
            task->errorMessage.clear();
            task->setStatus(Status::Pending);
            {
                std::lock_guard<std::mutex> lock(queueMutex_);
                pendingQueue_.push(task->id);
            }
            queueCV_.notify_one();
            LOG_INFO("Task retry initiated: " + taskId);
            notifyStatus(taskId, Status::Pending);
            return true;
        }
        LOG_WARN("retryTask failed: task not found or cannot retry: " + taskId);
        return false;
    }
    
    TaskInfo getTaskInfo(const std::string& taskId) const override {
        auto task = getTask(taskId);
        if (task) {
            return task->getTaskInfo();
        }
        return TaskInfo();
    }
    
    std::vector<TaskInfo> getAllTasks() const override {
        std::shared_lock<std::shared_mutex> lock(tasksMutex_);
        std::vector<TaskInfo> result;
        result.reserve(tasksList_.size());
        for (const auto& task : tasksList_) {
            result.push_back(task->getTaskInfo());
        }
        return result;
    }
    
    void start() override {
        if (running_) return;
        running_ = true;
        int threadCount = std::max(1, config_.maxConcurrentDownloads);
        for (int i = 0; i < threadCount; i++) {
            workerThreads_.emplace_back(&DownloadManagerImpl::workerThread, this);
        }
        LOG_INFO("Download manager started with " + std::to_string(threadCount) + " worker threads");
    }
    
    void stop() override {
        running_ = false;
        queueCV_.notify_all();
        for (auto& t : workerThreads_) {
            if (t.joinable()) t.join();
        }
        workerThreads_.clear();
        LOG_INFO("Download manager stopped");
    }
    
    bool isRunning() const override {
        return running_;
    }
    
    void waitForTask(const std::string& taskId, int timeoutMs) override {
        auto task = getTask(taskId);
        if (!task) return;
        
        auto startTime = std::chrono::steady_clock::now();
        
        while (true) {
            Status s = task->getStatus();
            if (s == Status::Completed || s == Status::Error || s == Status::Cancelled) {
                return;
            }
            
            if (timeoutMs >= 0) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - startTime).count();
                if (elapsed >= timeoutMs) return;
            }
            
            std::unique_lock<std::mutex> lock(task->taskMutex);
            if (timeoutMs >= 0) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - startTime).count();
                int remaining = timeoutMs - static_cast<int>(elapsed);
                if (remaining <= 0) return;
                task->taskCV.wait_for(lock, std::chrono::milliseconds(std::min(remaining, 100)));
            } else {
                task->taskCV.wait_for(lock, std::chrono::milliseconds(100));
            }
        }
    }
    
    void waitForAll(int timeoutMs) override {
        auto startTime = std::chrono::steady_clock::now();
        while (true) {
            bool allDone = true;
            {
                std::shared_lock<std::shared_mutex> lock(tasksMutex_);
                for (const auto& task : tasksList_) {
                    Status s = task->getStatus();
                    if (s == Status::Pending || s == Status::Downloading || s == Status::Paused) {
                        allDone = false;
                        break;
                    }
                }
            }
            
            if (allDone) return;
            
            if (timeoutMs >= 0) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - startTime).count();
                if (elapsed >= timeoutMs) return;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
private:
    std::shared_ptr<DownloadTask> getTask(const std::string& id) const {
        std::shared_lock<std::shared_mutex> lock(tasksMutex_);
        auto it = tasksMap_.find(id);
        if (it != tasksMap_.end()) {
            return it->second;
        }
        return nullptr;
    }
    
    void workerThread() {
        while (running_) {
            std::string taskId;
            {
                std::unique_lock<std::mutex> lock(queueMutex_);
                queueCV_.wait(lock, [this] { 
                    return !pendingQueue_.empty() || !running_; 
                });
                
                if (!running_) return;
                
                if (activeDownloads_ >= config_.maxConcurrentDownloads) {
                    lock.unlock();
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }
                
                if (pendingQueue_.empty()) continue;
                
                taskId = pendingQueue_.front();
                pendingQueue_.pop();
                activeDownloads_++;
            }
            
            auto task = getTask(taskId);
            if (!task) {
                activeDownloads_--;
                continue;
            }
            
            executeDownload(task);
            activeDownloads_--;
        }
    }
    
    void executeDownload(std::shared_ptr<DownloadTask> task) {
        task->setStatus(Status::Downloading);
        task->startTime = std::chrono::system_clock::now();
        task->lastSpeedUpdate = std::chrono::steady_clock::now();
        task->lastMetaSave = std::chrono::steady_clock::now();
        
        LOG_DEBUG("Starting download: " + task->id + " URL=" + task->url);
        notifyStatus(task->id, Status::Downloading);
        
        if (!queryFileInfo(task)) {
            handleDownloadError(task, task->errorMessage);
            return;
        }
        
        if (!Utils::EnsureDirectoryExists(task->savePath)) {
            handleDownloadError(task, "Cannot create save directory");
            return;
        }
        
        if (task->totalSize == 0 || !task->supportsRange) {
            downloadSingleThread(task);
        } else {
            downloadMultiThread(task);
        }
    }
    
    void handleDownloadError(std::shared_ptr<DownloadTask> task, const std::string& error) {
        task->errorMessage = error;
        LOG_ERROR("Download error for " + task->id + ": " + error);
        
        if (task->retryCount < config_.maxRetries) {
            task->retryCount++;
            bool willRetry = true;
            notifyError(task->id, error, willRetry);
            
            LOG_INFO("Retrying download " + task->id + " (attempt " + 
                     std::to_string(task->retryCount) + "/" + std::to_string(config_.maxRetries) + ")");
            
            std::this_thread::sleep_for(std::chrono::milliseconds(config_.retryDelayMs));
            
            task->setStatus(Status::Pending);
            {
                std::lock_guard<std::mutex> lock(queueMutex_);
                pendingQueue_.push(task->id);
            }
            queueCV_.notify_one();
            notifyStatus(task->id, Status::Pending);
        } else {
            task->setStatus(Status::Error);
            notifyError(task->id, error, false);
            notifyStatus(task->id, Status::Error);
        }
    }
    
    bool queryFileInfo(std::shared_ptr<DownloadTask> task) {
        ParsedUrl parsedUrl;
        if (!HttpClient::ParseUrl(task->url, parsedUrl)) {
            task->errorMessage = "Failed to parse URL";
            return false;
        }
        
        if (config_.proxy.type == ProxyType::SOCKS5 || config_.proxy.type == ProxyType::SOCKS4) {
            return queryFileInfoViaSocks(task, parsedUrl);
        }
        
        auto hSession = HttpClient::CreateSession();
        if (!hSession.isValid()) {
            task->errorMessage = "Failed to create HTTP session";
            return false;
        }
        
        HttpClient::SetProxy(hSession.get(), config_.proxy);
        
        auto hConnect = HttpClient::CreateConnection(hSession.get(), parsedUrl.hostName, parsedUrl.port);
        if (!hConnect.isValid()) {
            task->errorMessage = "Failed to connect to server";
            return false;
        }
        
        auto hRequest = HttpClient::CreateRequest(hConnect.get(), parsedUrl.urlPath, parsedUrl.isHttps, L"HEAD");
        if (!hRequest.isValid()) {
            task->errorMessage = "Failed to create request";
            return false;
        }
        
        if (parsedUrl.isHttps) {
            HttpClient::ConfigureHttps(hRequest.get(), config_.verifySsl);
        }
        
        HttpClient::SetTimeouts(hRequest.get(), config_.connectTimeoutMs, config_.readTimeoutMs, config_.readTimeoutMs);
        
        if (!WinHttpSendRequest(hRequest.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
            task->errorMessage = "Failed to send request: " + std::to_string(GetLastError());
            return false;
        }
        
        if (!WinHttpReceiveResponse(hRequest.get(), NULL)) {
            task->errorMessage = "Failed to receive response: " + std::to_string(GetLastError());
            return false;
        }
        
        DWORD statusCode = 0;
        DWORD statusSize = sizeof(statusCode);
        WinHttpQueryHeaders(hRequest.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
        
        if (statusCode != 200 && statusCode != 206) {
            task->errorMessage = "Server returned error: HTTP " + std::to_string(statusCode);
            return false;
        }
        
        DWORD contentLength = 0;
        DWORD lengthSize = sizeof(contentLength);
        if (WinHttpQueryHeaders(hRequest.get(), WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &contentLength, &lengthSize, WINHTTP_NO_HEADER_INDEX)) {
            task->totalSize = contentLength;
        }
        
        WCHAR acceptRanges[64] = {0};
        DWORD rangeSize = sizeof(acceptRanges);
        if (WinHttpQueryHeaders(hRequest.get(), WINHTTP_QUERY_ACCEPT_RANGES,
            WINHTTP_HEADER_NAME_BY_INDEX, acceptRanges, &rangeSize, WINHTTP_NO_HEADER_INDEX)) {
            task->supportsRange = (wcsstr(acceptRanges, L"bytes") != nullptr);
        }
        
        WCHAR contentDisposition[1024] = {0};
        DWORD dispSize = sizeof(contentDisposition);
        if (WinHttpQueryHeaders(hRequest.get(), WINHTTP_QUERY_CONTENT_DISPOSITION,
            WINHTTP_HEADER_NAME_BY_INDEX, contentDisposition, &dispSize, WINHTTP_NO_HEADER_INDEX)) {
            std::wstring dispW(contentDisposition);
            size_t fnPos = dispW.find(L"filename=");
            if (fnPos != std::wstring::npos) {
                size_t start = dispW.find(L'"', fnPos);
                if (start != std::wstring::npos) {
                    start++;
                    size_t end = dispW.find(L'"', start);
                    if (end != std::wstring::npos) {
                        std::wstring fnW = dispW.substr(start, end - start);
                        task->fileName = Utils::SafeToString(Utils::WideToUtf8(fnW));
                    }
                }
            }
        }
        
        LOG_DEBUG("Query file info: " + task->id + " size=" + std::to_string(task->totalSize) + 
                  " supportsRange=" + (task->supportsRange ? "true" : "false"));
        return true;
    }
    
    bool queryFileInfoViaSocks(std::shared_ptr<DownloadTask> task, const ParsedUrl& parsedUrl) {
        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) {
            task->errorMessage = "Failed to create socket";
            return false;
        }
        
        DWORD timeout = config_.connectTimeoutMs;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));
        
        std::string targetHost = Utils::WideToUtf8(parsedUrl.hostName);
        int targetPort = parsedUrl.port;
        
        bool connected = false;
        if (config_.proxy.type == ProxyType::SOCKS5) {
            connected = Socks5Client::ConnectViaSocks5(sock, targetHost, targetPort,
                config_.proxy.host, config_.proxy.port,
                config_.proxy.username, config_.proxy.password);
        } else if (config_.proxy.type == ProxyType::SOCKS4) {
            connected = Socks4Client::ConnectViaSocks4(sock, targetHost, targetPort,
                config_.proxy.host, config_.proxy.port);
        }
        
        if (!connected) {
            closesocket(sock);
            task->errorMessage = "Failed to connect via SOCKS proxy";
            return false;
        }
        
        std::string request = "HEAD " + Utils::WideToUtf8(parsedUrl.urlPath) + " HTTP/1.1\r\n";
        request += "Host: " + targetHost + "\r\n";
        request += "Connection: close\r\n";
        request += "\r\n";
        
        if (send(sock, request.c_str(), static_cast<int>(request.length()), 0) != 
            static_cast<int>(request.length())) {
            closesocket(sock);
            task->errorMessage = "Failed to send request via SOCKS";
            return false;
        }
        
        std::vector<char> response(4096);
        int received = recv(sock, response.data(), static_cast<int>(response.size()), 0);
        closesocket(sock);
        
        if (received <= 0) {
            task->errorMessage = "Failed to receive response via SOCKS";
            return false;
        }
        
        std::string respStr(response.data(), received);
        
        if (respStr.find("HTTP/1.1 200") == std::string::npos && 
            respStr.find("HTTP/1.1 206") == std::string::npos &&
            respStr.find("HTTP/1.0 200") == std::string::npos) {
            task->errorMessage = "Server returned error via SOCKS";
            return false;
        }
        
        size_t contentLengthPos = respStr.find("Content-Length:");
        if (contentLengthPos == std::string::npos) {
            contentLengthPos = respStr.find("content-length:");
        }
        if (contentLengthPos != std::string::npos) {
            size_t valueStart = respStr.find_first_of("0123456789", contentLengthPos);
            if (valueStart != std::string::npos) {
                task->totalSize = std::stoull(respStr.substr(valueStart));
            }
        }
        
        if (respStr.find("Accept-Ranges: bytes") != std::string::npos ||
            respStr.find("accept-ranges: bytes") != std::string::npos) {
            task->supportsRange = true;
        }
        
        return true;
    }
    
    void downloadSingleThread(std::shared_ptr<DownloadTask> task) {
        ParsedUrl parsedUrl;
        if (!HttpClient::ParseUrl(task->url, parsedUrl)) {
            handleDownloadError(task, "Failed to parse URL");
            return;
        }
        
        if (config_.proxy.type == ProxyType::SOCKS5 || config_.proxy.type == ProxyType::SOCKS4) {
            downloadSingleThreadViaSocks(task, parsedUrl);
            return;
        }
        
        auto hSession = HttpClient::CreateSession();
        HttpClient::SetProxy(hSession.get(), config_.proxy);
        auto hConnect = HttpClient::CreateConnection(hSession.get(), parsedUrl.hostName, parsedUrl.port);
        auto hRequest = HttpClient::CreateRequest(hConnect.get(), parsedUrl.urlPath, parsedUrl.isHttps);
        
        if (!hRequest.isValid()) {
            handleDownloadError(task, "Failed to create request");
            return;
        }
        
        if (parsedUrl.isHttps) {
            HttpClient::ConfigureHttps(hRequest.get(), config_.verifySsl);
        }
        
        HttpClient::SetTimeouts(hRequest.get(), config_.connectTimeoutMs, config_.readTimeoutMs, config_.readTimeoutMs);
        
        if (!WinHttpSendRequest(hRequest.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
            handleDownloadError(task, "Failed to send request");
            return;
        }
        
        if (!WinHttpReceiveResponse(hRequest.get(), NULL)) {
            handleDownloadError(task, "Failed to receive response");
            return;
        }
        
        std::wstring fullPathW = Utils::Utf8ToWide(task->getFullPath());
        FileWrapper file(_wfopen(fullPathW.c_str(), L"wb"));
        if (!file.isOpen()) {
            handleDownloadError(task, "Failed to create file");
            return;
        }
        
        std::vector<BYTE> buffer(BUFFER_SIZE);
        DWORD bytesRead;
        
        while (task->isDownloading() && !task->cancelRequested) {
            if (!WinHttpReadData(hRequest.get(), buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead)) {
                DWORD err = GetLastError();
                if (err == ERROR_WINHTTP_TIMEOUT) {
                    continue;
                }
                break;
            }
            if (bytesRead == 0) break;
            
            fwrite(buffer.data(), 1, bytesRead, file.get());
            task->downloadedSize += bytesRead;
            updateSpeed(task, bytesRead);
            applySpeedLimit(bytesRead);
            
            if (task->totalSize > 0) {
                task->setProgress(static_cast<int>((task->downloadedSize * 100) / task->totalSize));
            }
            
            notifyProgress(task->id, task->getProgress(), task->downloadedSize, task->totalSize, task->speed);
        }
        
        file.close();
        
        if (task->isDownloading() && !task->cancelRequested) {
            if (verifyDownload(task)) {
                task->setStatus(Status::Completed);
                task->setProgress(100);
                LOG_INFO("Download completed: " + task->id + " -> " + task->getFullPath());
                notifyComplete(task->id, task->getFullPath());
                notifyStatus(task->id, Status::Completed);
            } else {
                handleDownloadError(task, "MD5 verification failed");
            }
        } else if (task->cancelRequested) {
            task->setStatus(Status::Cancelled);
            notifyStatus(task->id, Status::Cancelled);
        }
    }
    
    void downloadSingleThreadViaSocks(std::shared_ptr<DownloadTask> task, const ParsedUrl& parsedUrl) {
        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) {
            handleDownloadError(task, "Failed to create socket");
            return;
        }
        
        DWORD timeout = config_.readTimeoutMs;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));
        
        std::string targetHost = Utils::WideToUtf8(parsedUrl.hostName);
        int targetPort = parsedUrl.port;
        
        bool connected = false;
        if (config_.proxy.type == ProxyType::SOCKS5) {
            connected = Socks5Client::ConnectViaSocks5(sock, targetHost, targetPort,
                config_.proxy.host, config_.proxy.port,
                config_.proxy.username, config_.proxy.password);
        } else if (config_.proxy.type == ProxyType::SOCKS4) {
            connected = Socks4Client::ConnectViaSocks4(sock, targetHost, targetPort,
                config_.proxy.host, config_.proxy.port);
        }
        
        if (!connected) {
            closesocket(sock);
            handleDownloadError(task, "Failed to connect via SOCKS proxy");
            return;
        }
        
        std::string request = "GET " + Utils::WideToUtf8(parsedUrl.urlPath) + " HTTP/1.1\r\n";
        request += "Host: " + targetHost + "\r\n";
        request += "Connection: close\r\n";
        request += "\r\n";
        
        if (send(sock, request.c_str(), static_cast<int>(request.length()), 0) !=
            static_cast<int>(request.length())) {
            closesocket(sock);
            handleDownloadError(task, "Failed to send request via SOCKS");
            return;
        }
        
        std::vector<char> headerBuffer(4096);
        int headerReceived = 0;
        int totalReceived = 0;
        bool headerComplete = false;
        
        while (!headerComplete && totalReceived < static_cast<int>(headerBuffer.size())) {
            int received = recv(sock, headerBuffer.data() + totalReceived, 
                               static_cast<int>(headerBuffer.size()) - totalReceived, 0);
            if (received <= 0) break;
            
            totalReceived += received;
            
            for (int i = 0; i < totalReceived - 3; i++) {
                if (headerBuffer[i] == '\r' && headerBuffer[i+1] == '\n' &&
                    headerBuffer[i+2] == '\r' && headerBuffer[i+3] == '\n') {
                    headerComplete = true;
                    headerReceived = i + 4;
                    break;
                }
            }
        }
        
        if (!headerComplete) {
            closesocket(sock);
            handleDownloadError(task, "Failed to receive HTTP header via SOCKS");
            return;
        }
        
        std::wstring fullPathW = Utils::Utf8ToWide(task->getFullPath());
        FileWrapper file(_wfopen(fullPathW.c_str(), L"wb"));
        if (!file.isOpen()) {
            closesocket(sock);
            handleDownloadError(task, "Failed to create file");
            return;
        }
        
        if (totalReceived > headerReceived) {
            fwrite(headerBuffer.data() + headerReceived, 1, totalReceived - headerReceived, file.get());
            task->downloadedSize += totalReceived - headerReceived;
        }
        
        std::vector<char> buffer(BUFFER_SIZE);
        
        while (task->isDownloading() && !task->cancelRequested) {
            int received = recv(sock, buffer.data(), static_cast<int>(buffer.size()), 0);
            if (received <= 0) break;
            
            fwrite(buffer.data(), 1, received, file.get());
            task->downloadedSize += received;
            updateSpeed(task, received);
            applySpeedLimit(received);
            
            if (task->totalSize > 0) {
                task->setProgress(static_cast<int>((task->downloadedSize * 100) / task->totalSize));
            }
            
            notifyProgress(task->id, task->getProgress(), task->downloadedSize, task->totalSize, task->speed);
        }
        
        file.close();
        closesocket(sock);
        
        if (task->isDownloading() && !task->cancelRequested) {
            if (verifyDownload(task)) {
                task->setStatus(Status::Completed);
                task->setProgress(100);
                notifyComplete(task->id, task->getFullPath());
                notifyStatus(task->id, Status::Completed);
            } else {
                handleDownloadError(task, "MD5 verification failed");
            }
        } else if (task->cancelRequested) {
            task->setStatus(Status::Cancelled);
            notifyStatus(task->id, Status::Cancelled);
        }
    }
    
    void downloadMultiThread(std::shared_ptr<DownloadTask> task) {
        loadTaskMeta(task);
        
        if (task->segments.empty()) {
            std::lock_guard<std::mutex> lock(task->segmentMutex);
            task->segments.resize(task->threadCount);
            uint64_t segmentSize = task->totalSize / task->threadCount;
            for (int i = 0; i < task->threadCount; i++) {
                task->segments[i] = std::make_shared<DownloadSegment>();
                task->segments[i]->startByte = i * segmentSize;
                task->segments[i]->endByte = (i == task->threadCount - 1) ?
                    task->totalSize - 1 : (i + 1) * segmentSize - 1;
                task->segments[i]->tempFile = task->getFullPath() + ".part" + std::to_string(i);
            }
        }
        
        std::vector<std::thread> threads;
        task->activeSegmentCount = 0;
        
        for (int i = 0; i < static_cast<int>(task->segments.size()); i++) {
            if (!task->segments[i]->completed && !task->segments[i]->active) {
                task->activeSegmentCount++;
                threads.emplace_back(&DownloadManagerImpl::downloadSegment, this, task, i);
            }
        }
        
        auto lastUpdate = std::chrono::steady_clock::now();
        
        while (task->isDownloading() && !task->cancelRequested) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            uint64_t totalDownloaded = 0;
            int completedCount = 0;
            
            for (const auto& seg : task->segments) {
                totalDownloaded += seg->downloadedBytes.load();
                if (seg->completed || seg->isComplete()) {
                    completedCount++;
                }
            }
            
            task->downloadedSize = totalDownloaded;
            
            if (task->totalSize > 0) {
                task->setProgress(static_cast<int>((task->downloadedSize * 100) / task->totalSize));
            }
            
            uint64_t currentSpeed = task->speed.load();
            if (currentSpeed > 0 && task->totalSize > task->downloadedSize) {
                uint64_t remaining = task->totalSize - task->downloadedSize;
                task->remainingTime = remaining / currentSpeed;
            }
            
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate).count();
            if (elapsed >= 200) {
                notifyProgress(task->id, task->getProgress(), task->downloadedSize, task->totalSize, task->speed);
                lastUpdate = now;
            }
            
            auto metaElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - task->lastMetaSave).count();
            if (metaElapsed >= META_SAVE_INTERVAL_MS) {
                saveTaskMeta(task);
                task->lastMetaSave = now;
            }
            
            if (completedCount >= static_cast<int>(task->segments.size()) && 
                task->activeSegmentCount <= 0) {
                break;
            }
        }
        
        for (auto& t : threads) {
            if (t.joinable()) t.join();
        }
        
        if (task->isDownloading() && !task->cancelRequested) {
            bool allCompleted = true;
            for (const auto& seg : task->segments) {
                if (!seg->completed && !seg->isComplete()) {
                    allCompleted = false;
                    break;
                }
            }
            
            if (allCompleted) {
                task->merging = true;
                mergeSegments(task);
                
                if (verifyDownload(task)) {
                    task->setStatus(Status::Completed);
                    task->setProgress(100);
                    
                    for (const auto& seg : task->segments) {
                        std::wstring tempFileW = Utils::Utf8ToWide(seg->tempFile);
                        DeleteFileW(tempFileW.c_str());
                    }
                    std::wstring metaFileW = Utils::Utf8ToWide(task->getMetaPath());
                    DeleteFileW(metaFileW.c_str());
                    
                    LOG_INFO("Download completed: " + task->id + " -> " + task->getFullPath());
                    notifyComplete(task->id, task->getFullPath());
                    notifyStatus(task->id, Status::Completed);
                } else {
                    handleDownloadError(task, "MD5 verification failed");
                }
            }
        } else if (task->cancelRequested) {
            task->setStatus(Status::Cancelled);
            notifyStatus(task->id, Status::Cancelled);
        }
    }
    
    void downloadSegment(std::shared_ptr<DownloadTask> task, int segIndex) {
        auto segment = task->segments[segIndex];
        segment->active = true;
        
        ParsedUrl parsedUrl;
        if (!HttpClient::ParseUrl(task->url, parsedUrl)) {
            segment->active = false;
            segment->errorMessage = "Failed to parse URL";
            task->activeSegmentCount--;
            return;
        }
        
        if (config_.proxy.type == ProxyType::SOCKS5 || config_.proxy.type == ProxyType::SOCKS4) {
            downloadSegmentViaSocks(task, segIndex, parsedUrl);
            return;
        }
        
        auto hSession = HttpClient::CreateSession();
        HttpClient::SetProxy(hSession.get(), config_.proxy);
        auto hConnect = HttpClient::CreateConnection(hSession.get(), parsedUrl.hostName, parsedUrl.port);
        auto hRequest = HttpClient::CreateRequest(hConnect.get(), parsedUrl.urlPath, parsedUrl.isHttps);
        
        if (!hRequest.isValid()) {
            segment->active = false;
            segment->errorMessage = "Failed to create request";
            task->activeSegmentCount--;
            return;
        }
        
        if (parsedUrl.isHttps) {
            HttpClient::ConfigureHttps(hRequest.get(), config_.verifySsl);
        }
        
        HttpClient::SetTimeouts(hRequest.get(), config_.connectTimeoutMs, config_.readTimeoutMs, config_.readTimeoutMs);
        
        uint64_t segmentTotalSize = segment->totalSize();
        uint64_t currentPos = segment->startByte + segment->downloadedBytes.load();
        
        if (segment->downloadedBytes > 0) {
            HttpClient::AddRangeHeader(hRequest.get(), currentPos, segment->endByte);
        } else {
            HttpClient::AddRangeHeader(hRequest.get(), segment->startByte, segment->endByte);
        }
        
        if (!WinHttpSendRequest(hRequest.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
            segment->active = false;
            segment->errorMessage = "Failed to send request";
            task->activeSegmentCount--;
            return;
        }
        
        if (!WinHttpReceiveResponse(hRequest.get(), NULL)) {
            segment->active = false;
            segment->errorMessage = "Failed to receive response";
            task->activeSegmentCount--;
            return;
        }
        
        std::wstring tempFileW = Utils::Utf8ToWide(segment->tempFile);
        FileWrapper file(_wfopen(tempFileW.c_str(), segment->downloadedBytes == 0 ? L"wb" : L"ab"));
        if (!file.isOpen()) {
            segment->active = false;
            segment->errorMessage = "Failed to create temporary file";
            task->activeSegmentCount--;
            return;
        }
        
        std::vector<BYTE> buffer(BUFFER_SIZE);
        DWORD bytesRead;
        
        while (task->isDownloading() && !task->cancelRequested &&
               segment->downloadedBytes < segmentTotalSize) {
            uint64_t remaining = segmentTotalSize - segment->downloadedBytes.load();
            DWORD readSize = static_cast<DWORD>(std::min(static_cast<uint64_t>(buffer.size()), remaining));
            
            if (!WinHttpReadData(hRequest.get(), buffer.data(), readSize, &bytesRead)) {
                DWORD err = GetLastError();
                if (err == ERROR_WINHTTP_TIMEOUT) {
                    continue;
                }
                break;
            }
            if (bytesRead == 0) break;
            
            DWORD writeSize = static_cast<DWORD>(std::min(static_cast<uint64_t>(bytesRead), remaining));
            fwrite(buffer.data(), 1, writeSize, file.get());
            segment->downloadedBytes += writeSize;
            updateSpeed(task, writeSize);
            applySpeedLimit(writeSize);
        }
        
        file.close();
        
        if (segment->downloadedBytes >= segmentTotalSize) {
            segment->completed = true;
        }
        
        segment->active = false;
        task->activeSegmentCount--;
    }
    
    void downloadSegmentViaSocks(std::shared_ptr<DownloadTask> task, int segIndex, const ParsedUrl& parsedUrl) {
        auto segment = task->segments[segIndex];
        
        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) {
            segment->active = false;
            segment->errorMessage = "Failed to create socket";
            task->activeSegmentCount--;
            return;
        }
        
        DWORD timeout = config_.readTimeoutMs;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));
        
        std::string targetHost = Utils::WideToUtf8(parsedUrl.hostName);
        int targetPort = parsedUrl.port;
        
        bool connected = false;
        if (config_.proxy.type == ProxyType::SOCKS5) {
            connected = Socks5Client::ConnectViaSocks5(sock, targetHost, targetPort,
                config_.proxy.host, config_.proxy.port,
                config_.proxy.username, config_.proxy.password);
        } else if (config_.proxy.type == ProxyType::SOCKS4) {
            connected = Socks4Client::ConnectViaSocks4(sock, targetHost, targetPort,
                config_.proxy.host, config_.proxy.port);
        }
        
        if (!connected) {
            closesocket(sock);
            segment->active = false;
            segment->errorMessage = "Failed to connect via SOCKS proxy";
            task->activeSegmentCount--;
            return;
        }
        
        uint64_t segmentTotalSize = segment->totalSize();
        uint64_t currentPos = segment->startByte + segment->downloadedBytes.load();
        uint64_t endPos = segment->endByte;
        
        std::string request = "GET " + Utils::WideToUtf8(parsedUrl.urlPath) + " HTTP/1.1\r\n";
        request += "Host: " + targetHost + "\r\n";
        request += "Range: bytes=" + std::to_string(currentPos) + "-" + std::to_string(endPos) + "\r\n";
        request += "Connection: close\r\n";
        request += "\r\n";
        
        if (send(sock, request.c_str(), static_cast<int>(request.length()), 0) !=
            static_cast<int>(request.length())) {
            closesocket(sock);
            segment->active = false;
            segment->errorMessage = "Failed to send request via SOCKS";
            task->activeSegmentCount--;
            return;
        }
        
        std::vector<char> headerBuffer(4096);
        int headerReceived = 0;
        int totalReceived = 0;
        bool headerComplete = false;
        
        while (!headerComplete && totalReceived < static_cast<int>(headerBuffer.size())) {
            int received = recv(sock, headerBuffer.data() + totalReceived,
                               static_cast<int>(headerBuffer.size()) - totalReceived, 0);
            if (received <= 0) break;
            
            totalReceived += received;
            
            for (int i = 0; i < totalReceived - 3; i++) {
                if (headerBuffer[i] == '\r' && headerBuffer[i+1] == '\n' &&
                    headerBuffer[i+2] == '\r' && headerBuffer[i+3] == '\n') {
                    headerComplete = true;
                    headerReceived = i + 4;
                    break;
                }
            }
        }
        
        if (!headerComplete) {
            closesocket(sock);
            segment->active = false;
            segment->errorMessage = "Failed to receive HTTP header via SOCKS";
            task->activeSegmentCount--;
            return;
        }
        
        std::wstring tempFileW = Utils::Utf8ToWide(segment->tempFile);
        FileWrapper file(_wfopen(tempFileW.c_str(), segment->downloadedBytes == 0 ? L"wb" : L"ab"));
        if (!file.isOpen()) {
            closesocket(sock);
            segment->active = false;
            segment->errorMessage = "Failed to create temporary file";
            task->activeSegmentCount--;
            return;
        }
        
        if (totalReceived > headerReceived) {
            int extraBytes = totalReceived - headerReceived;
            fwrite(headerBuffer.data() + headerReceived, 1, extraBytes, file.get());
            segment->downloadedBytes += extraBytes;
        }
        
        std::vector<char> buffer(BUFFER_SIZE);
        
        while (task->isDownloading() && !task->cancelRequested &&
               segment->downloadedBytes < segmentTotalSize) {
            uint64_t remaining = segmentTotalSize - segment->downloadedBytes.load();
            int readSize = static_cast<int>(std::min(static_cast<uint64_t>(buffer.size()), remaining));
            
            int received = recv(sock, buffer.data(), readSize, 0);
            if (received <= 0) break;
            
            int writeSize = static_cast<int>(std::min(static_cast<uint64_t>(received), remaining));
            fwrite(buffer.data(), 1, writeSize, file.get());
            segment->downloadedBytes += writeSize;
            updateSpeed(task, writeSize);
            applySpeedLimit(writeSize);
        }
        
        file.close();
        closesocket(sock);
        
        if (segment->downloadedBytes >= segmentTotalSize) {
            segment->completed = true;
        }
        
        segment->active = false;
        task->activeSegmentCount--;
    }
    
    void mergeSegments(std::shared_ptr<DownloadTask> task) {
        LOG_DEBUG("Merging segments for task: " + task->id);
        std::wstring fullPathW = Utils::Utf8ToWide(task->getFullPath());
        FileWrapper outFile(_wfopen(fullPathW.c_str(), L"wb"));
        if (!outFile.isOpen()) {
            task->errorMessage = "Failed to create output file";
            return;
        }
        
        std::vector<char> buffer(65536);
        for (const auto& seg : task->segments) {
            std::wstring tempFileW = Utils::Utf8ToWide(seg->tempFile);
            FileWrapper inFile(_wfopen(tempFileW.c_str(), L"rb"));
            if (inFile.isOpen()) {
                size_t bytesRead;
                while ((bytesRead = fread(buffer.data(), 1, buffer.size(), inFile.get())) > 0) {
                    fwrite(buffer.data(), 1, bytesRead, outFile.get());
                }
            }
        }
    }
    
    bool verifyDownload(std::shared_ptr<DownloadTask> task) {
        if (task->expectedMd5.empty() || !config_.verifyChecksum) {
            return true;
        }
        
        LOG_DEBUG("Verifying MD5 for task: " + task->id);
        std::string actualMd5 = Md5Calculator::Calculate(task->getFullPath());
        
        if (actualMd5.empty()) {
            LOG_ERROR("Failed to calculate MD5 for: " + task->getFullPath());
            return false;
        }
        
        std::string expectedLower = task->expectedMd5;
        std::transform(expectedLower.begin(), expectedLower.end(), expectedLower.begin(), ::tolower);
        
        if (actualMd5 == expectedLower) {
            task->verified = true;
            LOG_INFO("MD5 verification passed for task: " + task->id);
            return true;
        } else {
            LOG_ERROR("MD5 verification failed for task: " + task->id + 
                      " expected=" + expectedLower + " actual=" + actualMd5);
            return false;
        }
    }
    
    void updateSpeed(std::shared_ptr<DownloadTask> task, uint64_t bytesDownloaded) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - task->lastSpeedUpdate).count();
        
        if (elapsed >= SPEED_UPDATE_INTERVAL_MS) {
            uint64_t currentDownloaded = task->downloadedSize.load();
            if (currentDownloaded >= task->lastDownloadedForSpeed) {
                uint64_t bytesDiff = currentDownloaded - task->lastDownloadedForSpeed;
                uint64_t elapsedMs = static_cast<uint64_t>(elapsed);
                if (elapsedMs > 0) {
                    task->speed = (bytesDiff * 1000) / elapsedMs;
                }
            }
            task->lastSpeedUpdate = now;
            task->lastDownloadedForSpeed = currentDownloaded;
        }
    }
    
    void applySpeedLimit(uint64_t bytes) {
        if (config_.speedLimitKB <= 0) return;
        
        std::lock_guard<std::mutex> lock(speedMutex_);
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSpeedCheck_).count();
        
        currentSpeed_ += bytes;
        
        if (elapsed >= 100) {
            uint64_t limitBytesPerMs = static_cast<uint64_t>(config_.speedLimitKB) * 1024 / 1000;
            uint64_t expectedBytes = limitBytesPerMs * static_cast<uint64_t>(elapsed);
            
            if (currentSpeed_ > expectedBytes && limitBytesPerMs > 0) {
                uint64_t excessBytes = currentSpeed_ - expectedBytes;
                int sleepMs = static_cast<int>(excessBytes * 1000 / (limitBytesPerMs * 1000));
                sleepMs = std::min(sleepMs, 500);
                if (sleepMs > 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
                }
            }
            
            currentSpeed_ = 0;
            lastSpeedCheck_ = std::chrono::steady_clock::now();
        }
    }
    
    void saveTaskMeta(std::shared_ptr<DownloadTask> task) {
        std::string metaPath = task->getMetaPath();
        std::ofstream file(metaPath, std::ios::binary);
        if (!file) {
            LOG_WARN("Failed to save meta file: " + metaPath);
            return;
        }
        
        file << "[DLMETA]\n";
        file << "url=" << task->url << "\n";
        file << "filename=" << task->fileName << "\n";
        file << "savepath=" << task->savePath << "\n";
        file << "totalsize=" << task->totalSize << "\n";
        file << "threadcount=" << task->threadCount << "\n";
        file << "segments=" << task->segments.size() << "\n";
        file << "expectedmd5=" << task->expectedMd5 << "\n";
        
        for (size_t i = 0; i < task->segments.size(); i++) {
            file << "seg" << i << "_start=" << task->segments[i]->startByte << "\n";
            file << "seg" << i << "_end=" << task->segments[i]->endByte << "\n";
            file << "seg" << i << "_downloaded=" << task->segments[i]->downloadedBytes.load() << "\n";
        }
    }
    
    void loadTaskMeta(std::shared_ptr<DownloadTask> task) {
        std::string metaPath = task->getMetaPath();
        std::ifstream file(metaPath);
        if (!file) return;
        
        LOG_DEBUG("Loading meta file: " + metaPath);
        std::string line;
        std::map<std::string, std::string> data;
        
        while (std::getline(file, line)) {
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                std::string key = line.substr(0, pos);
                std::string value = line.substr(pos + 1);
                data[key] = value;
            }
        }
        
        file.close();
        
        try {
            if (data.count("segments")) {
                int segCount = std::stoi(data["segments"]);
                if (segCount > 0 && segCount <= MAX_THREAD_COUNT) {
                    std::lock_guard<std::mutex> lock(task->segmentMutex);
                    task->segments.resize(segCount);
                    for (int i = 0; i < segCount; i++) {
                        task->segments[i] = std::make_shared<DownloadSegment>();
                        std::string prefix = "seg" + std::to_string(i) + "_";
                        task->segments[i]->startByte = std::stoull(data[prefix + "start"]);
                        task->segments[i]->endByte = std::stoull(data[prefix + "end"]);
                        task->segments[i]->downloadedBytes = std::stoull(data[prefix + "downloaded"]);
                        task->segments[i]->tempFile = task->getFullPath() + ".part" + std::to_string(i);
                        task->segments[i]->completed = task->segments[i]->isComplete();
                    }
                    
                    uint64_t totalDownloaded = 0;
                    for (const auto& seg : task->segments) {
                        totalDownloaded += seg->downloadedBytes.load();
                    }
                    task->downloadedSize = totalDownloaded;
                }
            }
            
            if (data.count("expectedmd5")) {
                task->expectedMd5 = data["expectedmd5"];
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Exception while loading meta file: " + std::string(e.what()));
        }
    }
    
    void notifyProgress(const std::string& taskId, int progress, uint64_t downloaded, uint64_t total, uint64_t speed) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        if (progressCallback_) {
            progressCallback_(taskId, progress, downloaded, total, speed);
        }
    }
    
    void notifyComplete(const std::string& taskId, const std::string& filePath) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        if (completeCallback_) {
            completeCallback_(taskId, filePath);
        }
    }
    
    void notifyError(const std::string& taskId, const std::string& errorMessage, bool willRetry) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        if (errorCallback_) {
            errorCallback_(taskId, errorMessage, willRetry);
        }
    }
    
    void notifyStatus(const std::string& taskId, Status status) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        if (statusCallback_) {
            statusCallback_(taskId, status);
        }
    }
};

DownloadManager* DownloadManager::create() {
    return new DownloadManagerImpl();
}

void DownloadManager::destroy(DownloadManager* manager) {
    delete manager;
}

bool Initialize() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return false;
    }
    LOG_INFO("DLCore initialized, version " + std::string(VERSION));
    return true;
}

void Cleanup() {
    WSACleanup();
    LOG_INFO("DLCore cleanup complete");
}

std::string GetVersion() {
    return VERSION;
}

std::string CalculateFileMd5(const std::string& filePath) {
    return Md5Calculator::Calculate(filePath);
}

}
