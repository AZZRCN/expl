#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#ifdef _WIN32
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <commctrl.h>
    #include <shellapi.h>
    #include <shlobj.h>
    #include <wincrypt.h>
    #include <winhttp.h>
    #pragma comment(lib, "ws2_32.lib")
    #pragma comment(lib, "comctl32.lib")
    #pragma comment(lib, "user32.lib")
    #pragma comment(lib, "shell32.lib")
    #pragma comment(lib, "ole32.lib")
    #pragma comment(lib, "winhttp.lib")
    #pragma comment(lib, "crypt32.lib")
#endif

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <queue>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <algorithm>
#include <iomanip>
#include <functional>
#include <memory>

#define WM_DOWNLOAD_COMPLETE (WM_USER + 100)
#define WM_DOWNLOAD_PROGRESS (WM_USER + 101)
#define WM_DOWNLOAD_ERROR (WM_USER + 102)
#define WM_TRAY_ICON (WM_USER + 103)

#define ID_BROWSE 1001
#define ID_ADD 1002
#define ID_PAUSE 1003
#define ID_RESUME 1004
#define ID_CANCEL 1005
#define ID_REMOVE 1006
#define ID_OPENFOLDER 1007
#define ID_SETTINGS 1008
#define ID_TRAY_SHOW 2001
#define ID_TRAY_EXIT 2002

#define ID_PROXY_HOST 3001
#define ID_PROXY_PORT 3002
#define ID_PROXY_USER 3003
#define ID_PROXY_PASS 3004
#define ID_PROXY_ENABLE 3005
#define ID_SPEED_LIMIT 3006
#define ID_MAX_CONCURRENT 3007
#define ID_DEFAULT_THREADS 3008
#define ID_SAVE_SETTINGS 3009
#define ID_CANCEL_SETTINGS 3010

enum class DownloadStatus {
    Pending,
    Downloading,
    Paused,
    Completed,
    Error,
    Cancelled
};

enum class ProxyType {
    None,
    HTTP,
    SOCKS5
};

struct DownloadSegment {
    uint64_t startByte;
    uint64_t endByte;
    uint64_t downloadedBytes;
    std::string tempFile;
    std::atomic<bool> completed{false};
    std::atomic<bool> active{false};
    
    DownloadSegment() : startByte(0), endByte(0), downloadedBytes(0) {}
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
    DownloadStatus status;
    int progress;
    int threadCount;
    std::vector<std::shared_ptr<DownloadSegment>> segments;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point lastSpeedUpdate;
    uint64_t lastDownloadedForSpeed;
    std::string errorMessage;
    bool supportsRange;
    bool isHttps;
    std::mutex segmentMutex;
    std::condition_variable segmentCV;
    std::atomic<int> activeSegmentCount{0};
    std::atomic<bool> merging{false};
    
    DownloadTask() : totalSize(0), status(DownloadStatus::Pending),
                     progress(0), threadCount(4), supportsRange(false), isHttps(false),
                     remainingTime(0) {
        id = generateId();
        startTime = std::chrono::system_clock::now();
        lastSpeedUpdate = std::chrono::system_clock::now();
        lastDownloadedForSpeed = 0;
    }
    
    static std::string generateId() {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        return std::to_string(millis);
    }
    
    std::string getFullPath() const {
        if (savePath.back() == '\\' || savePath.back() == '/') {
            return savePath + fileName;
        }
        return savePath + "\\" + fileName;
    }
    
    std::string getMetaPath() const {
        return getFullPath() + ".dmmeta";
    }
};

struct ProxyConfig {
    std::string host;
    int port;
    std::string username;
    std::string password;
    bool enabled;
    ProxyType type;
    
    ProxyConfig() : port(1080), enabled(false), type(ProxyType::SOCKS5) {}
};

struct AppSettings {
    ProxyConfig proxy;
    int maxConcurrentDownloads;
    int speedLimitKB;
    int defaultThreadCount;
    std::string defaultSavePath;
    
    AppSettings() : maxConcurrentDownloads(3), speedLimitKB(0), defaultThreadCount(4), 
                    defaultSavePath("E:\\Downloads") {}
};

class Socks5Client {
private:
    std::string proxyHost;
    int proxyPort;
    std::string username;
    std::string password;
    
public:
    Socks5Client(const std::string& host, int port, const std::string& user = "", const std::string& pass = "")
        : proxyHost(host), proxyPort(port), username(user), password(pass) {}
    
    SOCKET connect(const std::string& targetHost, int targetPort) {
        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) return INVALID_SOCKET;
        
        timeval timeout;
        timeout.tv_sec = 30;
        timeout.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));
        
        sockaddr_in proxyAddr;
        memset(&proxyAddr, 0, sizeof(proxyAddr));
        proxyAddr.sin_family = AF_INET;
        proxyAddr.sin_port = htons(proxyPort);
        
        hostent* he = gethostbyname(proxyHost.c_str());
        if (!he) {
            closesocket(sock);
            return INVALID_SOCKET;
        }
        memcpy(&proxyAddr.sin_addr, he->h_addr_list[0], he->h_length);
        
        if (::connect(sock, (sockaddr*)&proxyAddr, sizeof(proxyAddr)) == SOCKET_ERROR) {
            closesocket(sock);
            return INVALID_SOCKET;
        }
        
        if (!handshake(sock)) {
            closesocket(sock);
            return INVALID_SOCKET;
        }
        
        if (!authenticate(sock)) {
            closesocket(sock);
            return INVALID_SOCKET;
        }
        
        if (!connectTarget(sock, targetHost, targetPort)) {
            closesocket(sock);
            return INVALID_SOCKET;
        }
        
        return sock;
    }
    
private:
    bool handshake(SOCKET sock) {
        unsigned char request[3] = {0x05, 0x01, 0x00};
        if (send(sock, (char*)request, 3, 0) != 3) return false;
        
        unsigned char response[2];
        if (recv(sock, (char*)response, 2, 0) != 2) return false;
        
        if (response[0] != 0x05) return false;
        
        if (response[1] == 0x02 && !username.empty()) {
            return true;
        }
        
        return response[1] == 0x00;
    }
    
    bool authenticate(SOCKET sock) {
        if (username.empty() && password.empty()) return true;
        
        unsigned char authRequest[513];
        authRequest[0] = 0x01;
        int ulen = std::min((int)username.length(), 255);
        authRequest[1] = (unsigned char)ulen;
        memcpy(authRequest + 2, username.c_str(), ulen);
        int plen = std::min((int)password.length(), 255);
        authRequest[2 + ulen] = (unsigned char)plen;
        memcpy(authRequest + 3 + ulen, password.c_str(), plen);
        
        int authLen = 3 + ulen + plen;
        if (send(sock, (char*)authRequest, authLen, 0) != authLen) return false;
        
        unsigned char authResponse[2];
        if (recv(sock, (char*)authResponse, 2, 0) != 2) return false;
        
        return authResponse[1] == 0x00;
    }
    
    bool connectTarget(SOCKET sock, const std::string& host, int port) {
        unsigned char request[262];
        request[0] = 0x05;
        request[1] = 0x01;
        request[2] = 0x00;
        request[3] = 0x03;
        
        int hostLen = std::min((int)host.length(), 255);
        request[4] = (unsigned char)hostLen;
        memcpy(request + 5, host.c_str(), hostLen);
        
        unsigned short nport = htons(port);
        memcpy(request + 5 + hostLen, &nport, 2);
        
        int requestLen = 7 + hostLen;
        if (send(sock, (char*)request, requestLen, 0) != requestLen) return false;
        
        unsigned char response[10];
        if (recv(sock, (char*)response, 10, 0) != 10) return false;
        
        if (response[0] != 0x05 || response[1] != 0x00) return false;
        
        return true;
    }
};

class TlsConnection {
private:
    SOCKET sock;
    CredHandle credHandle;
    CtxtHandle context;
    bool initialized;
    bool handshakeComplete;
    std::vector<char> receivedData;
    std::string host;
    
public:
    TlsConnection() : sock(INVALID_SOCKET), initialized(false), handshakeComplete(false) {
        memset(&credHandle, 0, sizeof(credHandle));
        memset(&context, 0, sizeof(context));
    }
    
    ~TlsConnection() {
        close();
    }
    
    bool connect(SOCKET s, const std::string& hostname) {
        sock = s;
        host = hostname;
        
        SCHANNEL_CRED cred;
        memset(&cred, 0, sizeof(cred));
        cred.dwVersion = SCHANNEL_CRED_VERSION;
        cred.grbitEnabledProtocols = SP_PROT_TLS1_2_CLIENT | SP_PROT_TLS1_3_CLIENT | SP_PROT_TLS1_1_CLIENT | SP_PROT_TLS1_CLIENT;
        cred.dwFlags = SCH_CRED_AUTO_CRED_VALIDATION | SCH_CRED_NO_DEFAULT_CREDS | SCH_CRED_MANUAL_CRED_VALIDATION;
        
        SECURITY_STATUS status = AcquireCredentialsHandleW(
            NULL, (SEC_WCHAR*)UNISP_NAME_W, SECPKG_CRED_OUTBOUND,
            NULL, &cred, NULL, NULL, &credHandle, NULL);
        
        if (status != SEC_E_OK) {
            return false;
        }
        initialized = true;
        
        return performHandshake();
    }
    
    bool performHandshake() {
        SecBuffer inBuffers[2];
        SecBuffer outBuffers[1];
        DWORD contextAttr = 0;
        std::vector<char> outToken(16384);
        std::vector<char> inToken;
        
        bool firstCall = true;
        SECURITY_STATUS status = SEC_I_CONTINUE_NEEDED;
        
        while (status == SEC_I_CONTINUE_NEEDED || firstCall) {
            inBuffers[0].cbBuffer = (ULONG)inToken.size();
            inBuffers[0].BufferType = SECBUFFER_TOKEN;
            inBuffers[0].pvBuffer = inToken.empty() ? NULL : inToken.data();
            
            inBuffers[1].cbBuffer = 0;
            inBuffers[1].BufferType = SECBUFFER_EMPTY;
            inBuffers[1].pvBuffer = NULL;
            
            SecBufferDesc inBuffer;
            inBuffer.ulVersion = SECBUFFER_VERSION;
            inBuffer.cBuffers = 2;
            inBuffer.pBuffers = inBuffers;
            
            outBuffers[0].cbBuffer = (ULONG)outToken.size();
            outBuffers[0].BufferType = SECBUFFER_TOKEN;
            outBuffers[0].pvBuffer = outToken.data();
            
            SecBufferDesc outBuffer;
            outBuffer.ulVersion = SECBUFFER_VERSION;
            outBuffer.cBuffers = 1;
            outBuffer.pBuffers = outBuffers;
            
            status = InitializeSecurityContextW(
                &credHandle,
                firstCall ? NULL : &context,
                (SEC_WCHAR*)std::wstring(host.begin(), host.end()).c_str(),
                ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONFIDENTIALITY |
                ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM | ISC_REQ_MANUAL_CRED_VALIDATION,
                0, 0,
                firstCall ? NULL : &inBuffer,
                0, &context,
                &outBuffer,
                &contextAttr, NULL);
            
            firstCall = false;
            
            if (status == SEC_E_OK || status == SEC_I_CONTINUE_NEEDED) {
                if (outBuffers[0].cbBuffer > 0) {
                    int sent = ::send(sock, (char*)outBuffers[0].pvBuffer, outBuffers[0].cbBuffer, 0);
                    if (sent == SOCKET_ERROR) {
                        return false;
                    }
                }
                
                if (status == SEC_I_CONTINUE_NEEDED) {
                    char recvBuf[16384];
                    int received = ::recv(sock, recvBuf, sizeof(recvBuf), 0);
                    if (received <= 0) return false;
                    inToken.assign(recvBuf, recvBuf + received);
                }
            } else {
                return false;
            }
        }
        
        handshakeComplete = true;
        return true;
    }
    
    int send(const char* data, int len) {
        if (!handshakeComplete) return -1;
        
        SecBufferDesc message;
        SecBuffer buffers[4];
        
        std::vector<char> encrypted(8192 + len);
        
        buffers[0].BufferType = SECBUFFER_STREAM_HEADER;
        buffers[0].cbBuffer = 5;
        buffers[0].pvBuffer = encrypted.data();
        
        buffers[1].BufferType = SECBUFFER_DATA;
        buffers[1].cbBuffer = len;
        buffers[1].pvBuffer = encrypted.data() + 5;
        memcpy(buffers[1].pvBuffer, data, len);
        
        buffers[2].BufferType = SECBUFFER_STREAM_TRAILER;
        buffers[2].cbBuffer = 36;
        buffers[2].pvBuffer = encrypted.data() + 5 + len;
        
        buffers[3].BufferType = SECBUFFER_EMPTY;
        buffers[3].cbBuffer = 0;
        buffers[3].pvBuffer = NULL;
        
        message.ulVersion = SECBUFFER_VERSION;
        message.cBuffers = 4;
        message.pBuffers = buffers;
        
        SECURITY_STATUS status = EncryptMessage(&context, 0, &message, 0);
        if (status != SEC_E_OK) return -1;
        
        int totalLen = buffers[0].cbBuffer + buffers[1].cbBuffer + buffers[2].cbBuffer;
        int sent = ::send(sock, encrypted.data(), totalLen, 0);
        
        return (sent == totalLen) ? len : -1;
    }
    
    int recv(char* buffer, int len) {
        if (!receivedData.empty()) {
            int copyLen = std::min((int)receivedData.size(), len);
            memcpy(buffer, receivedData.data(), copyLen);
            receivedData.erase(receivedData.begin(), receivedData.begin() + copyLen);
            return copyLen;
        }
        
        char encrypted[32768];
        int received = ::recv(sock, encrypted, sizeof(encrypted), 0);
        if (received <= 0) return received;
        
        SecBufferDesc message;
        SecBuffer buffers[4];
        
        std::vector<char> tempData(encrypted, encrypted + received);
        
        buffers[0].BufferType = SECBUFFER_DATA;
        buffers[0].cbBuffer = received;
        buffers[0].pvBuffer = tempData.data();
        
        buffers[1].BufferType = SECBUFFER_EMPTY;
        buffers[1].cbBuffer = 0;
        buffers[1].pvBuffer = NULL;
        
        buffers[2].BufferType = SECBUFFER_EMPTY;
        buffers[2].cbBuffer = 0;
        buffers[2].pvBuffer = NULL;
        
        buffers[3].BufferType = SECBUFFER_EMPTY;
        buffers[3].cbBuffer = 0;
        buffers[3].pvBuffer = NULL;
        
        message.ulVersion = SECBUFFER_VERSION;
        message.cBuffers = 4;
        message.pBuffers = buffers;
        
        SECURITY_STATUS status = DecryptMessage(&context, &message, 0, 0);
        
        if (status == SEC_E_OK) {
            for (int i = 0; i < 4; i++) {
                if (buffers[i].BufferType == SECBUFFER_DATA) {
                    int copyLen = std::min((int)buffers[i].cbBuffer, len);
                    memcpy(buffer, buffers[i].pvBuffer, copyLen);
                    
                    if (buffers[i].cbBuffer > (ULONG)copyLen) {
                        char* extraStart = (char*)buffers[i].pvBuffer + copyLen;
                        receivedData.insert(receivedData.end(), extraStart, extraStart + buffers[i].cbBuffer - copyLen);
                    }
                    
                    return copyLen;
                }
            }
        } else if (status == SEC_E_INCOMPLETE_MESSAGE) {
            receivedData.insert(receivedData.end(), encrypted, encrypted + received);
            return 0;
        }
        
        return 0;
    }
    
    void close() {
        if (handshakeComplete) {
            DWORD type = SCHANNEL_SHUTDOWN;
            SecBufferDesc desc;
            SecBuffer buf;
            buf.BufferType = SECBUFFER_TOKEN;
            buf.cbBuffer = sizeof(type);
            buf.pvBuffer = &type;
            desc.ulVersion = SECBUFFER_VERSION;
            desc.cBuffers = 1;
            desc.pBuffers = &buf;
            ApplyControlToken(&context, &desc);
        }
        
        if (initialized) {
            DeleteSecurityContext(&context);
            FreeCredentialHandle(&credHandle);
        }
        
        if (sock != INVALID_SOCKET) {
            closesocket(sock);
            sock = INVALID_SOCKET;
        }
        
        initialized = false;
        handshakeComplete = false;
    }
    
    SOCKET getSocket() const { return sock; }
};

class DownloadEngine {
private:
    std::vector<std::shared_ptr<DownloadTask>> tasks;
    std::mutex tasksMutex;
    std::queue<std::string> pendingQueue;
    std::mutex queueMutex;
    std::condition_variable queueCV;
    std::atomic<bool> running{false};
    std::vector<std::thread> workerThreads;
    AppSettings settings;
    int activeDownloads{0};
    std::mutex activeMutex;
    HWND notifyWindow{nullptr};
    std::atomic<uint64_t> globalSpeedLimit{0};
    std::atomic<uint64_t> currentSpeed{0};
    std::chrono::steady_clock::time_point lastSpeedCheck;
    std::mutex speedMutex;
    
public:
    DownloadEngine() {
        lastSpeedCheck = std::chrono::steady_clock::now();
    }
    ~DownloadEngine() { stop(); }
    
    void setNotifyWindow(HWND hwnd) { notifyWindow = hwnd; }
    void setSettings(const AppSettings& s) { settings = s; }
    AppSettings getSettings() const { return settings; }
    
    void start() {
        if (running) return;
        running = true;
        for (int i = 0; i < settings.maxConcurrentDownloads; i++) {
            workerThreads.emplace_back(&DownloadEngine::workerThread, this);
        }
    }
    
    void stop() {
        running = false;
        queueCV.notify_all();
        for (auto& t : workerThreads) {
            if (t.joinable()) t.join();
        }
        workerThreads.clear();
    }
    
    std::shared_ptr<DownloadTask> addTask(const std::string& url, const std::string& savePath, int threads = 4) {
        auto task = std::make_shared<DownloadTask>();
        task->url = url;
        task->savePath = savePath;
        task->threadCount = threads;
        task->fileName = extractFileName(url);
        task->isHttps = (url.find("https://") == 0);
        
        {
            std::lock_guard<std::mutex> lock(tasksMutex);
            tasks.push_back(task);
        }
        
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            pendingQueue.push(task->id);
        }
        queueCV.notify_one();
        
        return task;
    }
    
    std::vector<std::shared_ptr<DownloadTask>> getTasks() {
        std::lock_guard<std::mutex> lock(tasksMutex);
        return tasks;
    }
    
    std::shared_ptr<DownloadTask> getTask(const std::string& id) {
        std::lock_guard<std::mutex> lock(tasksMutex);
        for (auto& task : tasks) {
            if (task->id == id) return task;
        }
        return nullptr;
    }
    
    void pauseTask(const std::string& id) {
        auto task = getTask(id);
        if (task && task->status == DownloadStatus::Downloading) {
            task->status = DownloadStatus::Paused;
            saveTaskMeta(task);
        }
    }
    
    void resumeTask(const std::string& id) {
        auto task = getTask(id);
        if (task && task->status == DownloadStatus::Paused) {
            task->status = DownloadStatus::Pending;
            {
                std::lock_guard<std::mutex> lock(queueMutex);
                pendingQueue.push(task->id);
            }
            queueCV.notify_one();
        }
    }
    
    void cancelTask(const std::string& id) {
        auto task = getTask(id);
        if (task) {
            task->status = DownloadStatus::Cancelled;
        }
    }
    
    void removeTask(const std::string& id) {
        std::lock_guard<std::mutex> lock(tasksMutex);
        tasks.erase(std::remove_if(tasks.begin(), tasks.end(),
            [&id](const std::shared_ptr<DownloadTask>& t) { return t->id == id; }),
            tasks.end());
    }
    
private:
    void workerThread() {
        while (running) {
            std::string taskId;
            {
                std::unique_lock<std::mutex> lock(queueMutex);
                queueCV.wait(lock, [this] { return !pendingQueue.empty() || !running; });
                
                if (!running) return;
                
                {
                    std::lock_guard<std::mutex> activeLock(activeMutex);
                    if (activeDownloads >= settings.maxConcurrentDownloads) continue;
                    activeDownloads++;
                }
                
                taskId = pendingQueue.front();
                pendingQueue.pop();
            }
            
            auto task = getTask(taskId);
            if (!task) {
                std::lock_guard<std::mutex> activeLock(activeMutex);
                activeDownloads--;
                continue;
            }
            
            executeDownload(task);
            
            {
                std::lock_guard<std::mutex> activeLock(activeMutex);
                activeDownloads--;
            }
        }
    }
    
    void executeDownload(std::shared_ptr<DownloadTask> task) {
        task->status = DownloadStatus::Downloading;
        task->startTime = std::chrono::system_clock::now();
        
        if (!queryFileInfo(task)) {
            task->status = DownloadStatus::Error;
            if (notifyWindow) {
                PostMessage(notifyWindow, WM_DOWNLOAD_ERROR, (WPARAM)task.get(), 0);
            }
            return;
        }
        
        if (task->totalSize == 0 || !task->supportsRange) {
            downloadSingleThread(task);
        } else {
            downloadMultiThread(task);
        }
    }
    
    bool queryFileInfo(std::shared_ptr<DownloadTask> task) {
        std::string host, path;
        int port;
        parseUrl(task->url, host, path, port);
        
        SOCKET sock = connectToHost(host, port, task->isHttps);
        if (sock == INVALID_SOCKET) {
            task->errorMessage = "连接服务器失败";
            return false;
        }
        
        std::string request = "HEAD " + path + " HTTP/1.1\r\n";
        request += "Host: " + host + "\r\n";
        request += "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36\r\n";
        request += "Connection: close\r\n";
        request += "\r\n";
        
        if (task->isHttps) {
            TlsConnection tls;
            if (!tls.connect(sock, host)) {
                task->errorMessage = "TLS握手失败";
                return false;
            }
            tls.send(request.c_str(), request.length());
            
            char buffer[8192];
            int received = tls.recv(buffer, sizeof(buffer) - 1);
            tls.close();
            
            if (received <= 0) {
                task->errorMessage = "获取文件信息失败";
                return false;
            }
            buffer[received] = '\0';
            std::string headers(buffer);
            parseHeaders(headers, task);
        } else {
            send(sock, request.c_str(), request.length(), 0);
            
            char buffer[8192];
            int received = recv(sock, buffer, sizeof(buffer) - 1, 0);
            closesocket(sock);
            
            if (received <= 0) {
                task->errorMessage = "获取文件信息失败";
                return false;
            }
            buffer[received] = '\0';
            std::string headers(buffer);
            parseHeaders(headers, task);
        }
        
        return true;
    }
    
    void parseHeaders(const std::string& headers, std::shared_ptr<DownloadTask> task) {
        size_t clPos = headers.find("Content-Length:");
        if (clPos == std::string::npos) {
            clPos = headers.find("content-length:");
        }
        if (clPos != std::string::npos) {
            size_t start = headers.find_first_of("0123456789", clPos);
            size_t end = headers.find("\r\n", start);
            task->totalSize = std::stoull(headers.substr(start, end - start));
        }
        
        size_t acceptPos = headers.find("Accept-Ranges:");
        if (acceptPos == std::string::npos) {
            acceptPos = headers.find("accept-ranges:");
        }
        if (acceptPos != std::string::npos) {
            std::string rangeValue = headers.substr(acceptPos + 13, 10);
            task->supportsRange = (rangeValue.find("bytes") != std::string::npos);
        }
        
        size_t filenamePos = headers.find("filename=");
        if (filenamePos != std::string::npos) {
            size_t start = headers.find("\"", filenamePos) + 1;
            size_t end = headers.find("\"", start);
            if (start != std::string::npos && end != std::string::npos) {
                task->fileName = headers.substr(start, end - start);
            }
        }
        
        size_t contentDispPos = headers.find("Content-Disposition:");
        if (contentDispPos == std::string::npos) {
            contentDispPos = headers.find("content-disposition:");
        }
        if (contentDispPos != std::string::npos) {
            size_t fnPos = headers.find("filename*=", contentDispPos);
            if (fnPos != std::string::npos) {
                size_t start = headers.find("''", fnPos);
                if (start != std::string::npos) {
                    start += 2;
                    size_t end = headers.find("\r\n", start);
                    if (end != std::string::npos) {
                        std::string encodedName = headers.substr(start, end - start);
                        task->fileName = urlDecode(encodedName);
                    }
                }
            }
        }
    }
    
    std::string urlDecode(const std::string& str) {
        std::string result;
        for (size_t i = 0; i < str.length(); i++) {
            if (str[i] == '%' && i + 2 < str.length()) {
                int hex;
                std::stringstream ss;
                ss << std::hex << str.substr(i + 1, 2);
                ss >> hex;
                result += (char)hex;
                i += 2;
            } else if (str[i] == '+') {
                result += ' ';
            } else {
                result += str[i];
            }
        }
        return result;
    }
    
    void downloadSingleThread(std::shared_ptr<DownloadTask> task) {
        std::string host, path;
        int port;
        parseUrl(task->url, host, path, port);
        
        SOCKET sock = connectToHost(host, port, task->isHttps);
        if (sock == INVALID_SOCKET) {
            task->status = DownloadStatus::Error;
            task->errorMessage = "连接服务器失败";
            return;
        }
        
        std::string request = "GET " + path + " HTTP/1.1\r\n";
        request += "Host: " + host + "\r\n";
        request += "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36\r\n";
        request += "Connection: close\r\n";
        request += "\r\n";
        
        FILE* file = fopen(task->getFullPath().c_str(), "wb");
        if (!file) {
            closesocket(sock);
            task->status = DownloadStatus::Error;
            task->errorMessage = "无法创建文件";
            return;
        }
        
        std::function<int(char*, int)> recvFunc;
        TlsConnection tls;
        
        if (task->isHttps) {
            if (!tls.connect(sock, host)) {
                fclose(file);
                closesocket(sock);
                task->status = DownloadStatus::Error;
                task->errorMessage = "TLS握手失败";
                return;
            }
            tls.send(request.c_str(), request.length());
            recvFunc = [&tls](char* buf, int len) { return tls.recv(buf, len); };
        } else {
            send(sock, request.c_str(), request.length(), 0);
            recvFunc = [sock](char* buf, int len) { return recv(sock, buf, len, 0); };
        }
        
        char buffer[16384];
        int received = recvFunc(buffer, sizeof(buffer) - 1);
        if (received <= 0) {
            fclose(file);
            if (task->isHttps) tls.close(); else closesocket(sock);
            task->status = DownloadStatus::Error;
            return;
        }
        
        char* bodyStart = strstr(buffer, "\r\n\r\n");
        if (!bodyStart) {
            fclose(file);
            if (task->isHttps) tls.close(); else closesocket(sock);
            task->status = DownloadStatus::Error;
            return;
        }
        bodyStart += 4;
        int headerLen = bodyStart - buffer;
        
        int bodyLen = received - headerLen;
        if (bodyLen > 0) {
            fwrite(bodyStart, 1, bodyLen, file);
            task->downloadedSize += bodyLen;
        }
        
        while (task->status == DownloadStatus::Downloading) {
            received = recvFunc(buffer, sizeof(buffer));
            if (received <= 0) break;
            
            fwrite(buffer, 1, received, file);
            task->downloadedSize += received;
            
            updateSpeed(task, received);
            applySpeedLimit(received);
            
            if (task->totalSize > 0) {
                task->progress = (int)((task->downloadedSize * 100) / task->totalSize);
            }
            
            if (notifyWindow) {
                PostMessage(notifyWindow, WM_DOWNLOAD_PROGRESS, (WPARAM)task.get(), 0);
            }
        }
        
        fclose(file);
        if (task->isHttps) tls.close(); else closesocket(sock);
        
        if (task->status == DownloadStatus::Downloading) {
            task->status = DownloadStatus::Completed;
            task->progress = 100;
            if (notifyWindow) {
                PostMessage(notifyWindow, WM_DOWNLOAD_COMPLETE, (WPARAM)task.get(), 0);
            }
        }
    }
    
    void downloadMultiThread(std::shared_ptr<DownloadTask> task) {
        loadTaskMeta(task);
        
        if (task->segments.empty()) {
            task->segments.resize(task->threadCount);
            uint64_t segmentSize = task->totalSize / task->threadCount;
            
            for (int i = 0; i < task->threadCount; i++) {
                task->segments[i] = std::make_shared<DownloadSegment>();
                task->segments[i]->startByte = i * segmentSize;
                task->segments[i]->endByte = (i == task->threadCount - 1) ? 
                    task->totalSize - 1 : (i + 1) * segmentSize - 1;
                task->segments[i]->downloadedBytes = 0;
                task->segments[i]->tempFile = task->getFullPath() + ".part" + std::to_string(i);
                task->segments[i]->completed = false;
            }
        }
        
        std::vector<std::thread> threads;
        task->activeSegmentCount = 0;
        
        for (int i = 0; i < task->threadCount; i++) {
            if (!task->segments[i]->completed) {
                task->activeSegmentCount++;
                threads.emplace_back(&DownloadEngine::downloadSegment, this, task, i);
            }
        }
        
        while (task->activeSegmentCount > 0 && task->status == DownloadStatus::Downloading) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            
            uint64_t totalDownloaded = 0;
            for (const auto& seg : task->segments) {
                totalDownloaded += seg->downloadedBytes;
            }
            task->downloadedSize = totalDownloaded;
            
            if (task->totalSize > 0) {
                task->progress = (int)((task->downloadedSize * 100) / task->totalSize);
            }
            
            if (task->speed > 0 && task->totalSize > task->downloadedSize) {
                uint64_t remaining = task->totalSize - task->downloadedSize;
                task->remainingTime = remaining / task->speed;
            }
            
            if (notifyWindow) {
                PostMessage(notifyWindow, WM_DOWNLOAD_PROGRESS, (WPARAM)task.get(), 0);
            }
            
            saveTaskMeta(task);
        }
        
        for (auto& t : threads) {
            if (t.joinable()) t.join();
        }
        
        if (task->status == DownloadStatus::Downloading) {
            bool allCompleted = true;
            for (const auto& seg : task->segments) {
                if (!seg->completed) {
                    allCompleted = false;
                    break;
                }
            }
            
            if (allCompleted) {
                task->merging = true;
                mergeSegments(task);
                task->status = DownloadStatus::Completed;
                task->progress = 100;
                
                for (const auto& seg : task->segments) {
                    remove(seg->tempFile.c_str());
                }
                remove(task->getMetaPath().c_str());
                
                if (notifyWindow) {
                    PostMessage(notifyWindow, WM_DOWNLOAD_COMPLETE, (WPARAM)task.get(), 0);
                }
            }
        }
    }
    
    void downloadSegment(std::shared_ptr<DownloadTask> task, int segIndex) {
        auto segment = task->segments[segIndex];
        segment->active = true;
        
        std::string host, path;
        int port;
        parseUrl(task->url, host, path, port);
        
        SOCKET sock = connectToHost(host, port, task->isHttps);
        if (sock == INVALID_SOCKET) {
            segment->active = false;
            task->activeSegmentCount--;
            return;
        }
        
        uint64_t segmentTotalSize = segment->endByte - segment->startByte + 1;
        uint64_t currentPos = segment->startByte + segment->downloadedBytes;
        
        std::string request = "GET " + path + " HTTP/1.1\r\n";
        request += "Host: " + host + "\r\n";
        request += "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36\r\n";
        request += "Range: bytes=" + std::to_string(currentPos) + 
                   "-" + std::to_string(segment->endByte) + "\r\n";
        request += "Connection: close\r\n";
        request += "\r\n";
        
        std::function<int(char*, int)> recvFunc;
        TlsConnection tls;
        
        if (task->isHttps) {
            if (!tls.connect(sock, host)) {
                closesocket(sock);
                segment->active = false;
                task->activeSegmentCount--;
                return;
            }
            tls.send(request.c_str(), request.length());
            recvFunc = [&tls](char* buf, int len) { return tls.recv(buf, len); };
        } else {
            send(sock, request.c_str(), request.length(), 0);
            recvFunc = [sock](char* buf, int len) { return recv(sock, buf, len, 0); };
        }
        
        char buffer[16384];
        int received = recvFunc(buffer, sizeof(buffer) - 1);
        if (received <= 0) {
            if (task->isHttps) tls.close(); else closesocket(sock);
            segment->active = false;
            task->activeSegmentCount--;
            return;
        }
        
        char* bodyStart = strstr(buffer, "\r\n\r\n");
        if (!bodyStart) {
            if (task->isHttps) tls.close(); else closesocket(sock);
            segment->active = false;
            task->activeSegmentCount--;
            return;
        }
        bodyStart += 4;
        int headerLen = bodyStart - buffer;
        
        FILE* file = fopen(segment->tempFile.c_str(), segment->downloadedBytes == 0 ? "wb" : "ab");
        if (!file) {
            if (task->isHttps) tls.close(); else closesocket(sock);
            segment->active = false;
            task->activeSegmentCount--;
            return;
        }
        
        int bodyLen = received - headerLen;
        if (bodyLen > 0) {
            fwrite(bodyStart, 1, bodyLen, file);
            segment->downloadedBytes += bodyLen;
        }
        
        while (task->status == DownloadStatus::Downloading && segment->downloadedBytes < segmentTotalSize) {
            received = recvFunc(buffer, sizeof(buffer));
            if (received <= 0) break;
            
            fwrite(buffer, 1, received, file);
            segment->downloadedBytes += received;
            
            updateSpeed(task, received);
            applySpeedLimit(received);
        }
        
        fclose(file);
        if (task->isHttps) tls.close(); else closesocket(sock);
        
        if (segment->downloadedBytes >= segmentTotalSize) {
            segment->completed = true;
        }
        
        segment->active = false;
        task->activeSegmentCount--;
    }
    
    void mergeSegments(std::shared_ptr<DownloadTask> task) {
        FILE* outFile = fopen(task->getFullPath().c_str(), "wb");
        if (!outFile) return;
        
        char buffer[65536];
        for (const auto& seg : task->segments) {
            FILE* inFile = fopen(seg->tempFile.c_str(), "rb");
            if (inFile) {
                size_t bytesRead;
                while ((bytesRead = fread(buffer, 1, sizeof(buffer), inFile)) > 0) {
                    fwrite(buffer, 1, bytesRead, outFile);
                }
                fclose(inFile);
            }
        }
        
        fclose(outFile);
    }
    
    void updateSpeed(std::shared_ptr<DownloadTask> task, uint64_t bytesDownloaded) {
        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - task->lastSpeedUpdate).count();
        
        if (elapsed >= 500) {
            uint64_t currentDownloaded = task->downloadedSize;
            uint64_t bytesDiff = currentDownloaded - task->lastDownloadedForSpeed;
            task->speed = (bytesDiff * 1000) / elapsed;
            task->lastSpeedUpdate = now;
            task->lastDownloadedForSpeed = currentDownloaded;
        }
    }
    
    void applySpeedLimit(uint64_t bytes) {
        if (settings.speedLimitKB <= 0) return;
        
        std::lock_guard<std::mutex> lock(speedMutex);
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSpeedCheck).count();
        
        if (elapsed >= 100) {
            uint64_t limitBytesPerMs = (uint64_t)settings.speedLimitKB * 1024 / 1000;
            uint64_t expectedBytes = limitBytesPerMs * elapsed;
            
            if (currentSpeed > expectedBytes) {
                int sleepMs = (int)((currentSpeed - expectedBytes) * 1000 / (limitBytesPerMs * 1000 + 1));
                if (sleepMs > 0 && sleepMs < 1000) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
                }
            }
            
            currentSpeed = 0;
            lastSpeedCheck = std::chrono::steady_clock::now();
        }
        
        currentSpeed += bytes;
    }
    
    void saveTaskMeta(std::shared_ptr<DownloadTask> task) {
        std::ofstream file(task->getMetaPath(), std::ios::binary);
        if (!file) return;
        
        file << "[DMMETA]\n";
        file << "url=" << task->url << "\n";
        file << "filename=" << task->fileName << "\n";
        file << "savepath=" << task->savePath << "\n";
        file << "totalsize=" << task->totalSize << "\n";
        file << "threadcount=" << task->threadCount << "\n";
        file << "segments=" << task->segments.size() << "\n";
        
        for (size_t i = 0; i < task->segments.size(); i++) {
            file << "seg" << i << "_start=" << task->segments[i]->startByte << "\n";
            file << "seg" << i << "_end=" << task->segments[i]->endByte << "\n";
            file << "seg" << i << "_downloaded=" << task->segments[i]->downloadedBytes << "\n";
        }
        
        file.close();
    }
    
    void loadTaskMeta(std::shared_ptr<DownloadTask> task) {
        std::ifstream file(task->getMetaPath());
        if (!file) return;
        
        std::string line;
        std::map<std::string, std::string> data;
        
        while (std::getline(file, line)) {
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                data[line.substr(0, pos)] = line.substr(pos + 1);
            }
        }
        
        file.close();
        
        if (data.count("segments")) {
            int segCount = std::stoi(data["segments"]);
            task->segments.resize(segCount);
            
            for (int i = 0; i < segCount; i++) {
                task->segments[i] = std::make_shared<DownloadSegment>();
                task->segments[i]->startByte = std::stoull(data["seg" + std::to_string(i) + "_start"]);
                task->segments[i]->endByte = std::stoull(data["seg" + std::to_string(i) + "_end"]);
                task->segments[i]->downloadedBytes = std::stoull(data["seg" + std::to_string(i) + "_downloaded"]);
                task->segments[i]->tempFile = task->getFullPath() + ".part" + std::to_string(i);
                uint64_t segmentTotalSize = task->segments[i]->endByte - task->segments[i]->startByte + 1;
                task->segments[i]->completed = (task->segments[i]->downloadedBytes >= segmentTotalSize);
            }
            
            uint64_t totalDownloaded = 0;
            for (const auto& seg : task->segments) {
                totalDownloaded += seg->downloadedBytes;
            }
            task->downloadedSize = totalDownloaded;
        }
    }
    
    SOCKET connectToHost(const std::string& host, int port, bool isHttps) {
        if (settings.proxy.enabled && settings.proxy.type == ProxyType::SOCKS5) {
            Socks5Client socks(settings.proxy.host, settings.proxy.port,
                             settings.proxy.username, settings.proxy.password);
            return socks.connect(host, port);
        }
        
        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) return INVALID_SOCKET;
        
        timeval timeout;
        timeout.tv_sec = 30;
        timeout.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));
        
        sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        
        hostent* he = gethostbyname(host.c_str());
        if (!he) {
            closesocket(sock);
            return INVALID_SOCKET;
        }
        memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
        
        if (settings.proxy.enabled && settings.proxy.type == ProxyType::HTTP) {
            if (::connect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
                closesocket(sock);
                return INVALID_SOCKET;
            }
            
            std::string connectReq = "CONNECT " + host + ":" + std::to_string(port) + " HTTP/1.1\r\n";
            connectReq += "Host: " + host + ":" + std::to_string(port) + "\r\n";
            if (!settings.proxy.username.empty()) {
                std::string auth = settings.proxy.username + ":" + settings.proxy.password;
                connectReq += "Proxy-Authorization: Basic " + base64Encode(auth) + "\r\n";
            }
            connectReq += "\r\n";
            
            send(sock, connectReq.c_str(), connectReq.length(), 0);
            
            char response[1024];
            int received = recv(sock, response, sizeof(response) - 1, 0);
            if (received <= 0 || strstr(response, "200") == NULL) {
                closesocket(sock);
                return INVALID_SOCKET;
            }
            
            return sock;
        }
        
        if (::connect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            closesocket(sock);
            return INVALID_SOCKET;
        }
        
        return sock;
    }
    
    std::string base64Encode(const std::string& input) {
        static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string result;
        int val = 0, valb = -6;
        
        for (unsigned char c : input) {
            val = (val << 8) + c;
            valb += 8;
            while (valb >= 0) {
                result.push_back(chars[(val >> valb) & 0x3F]);
                valb -= 6;
            }
        }
        
        if (valb > -6) {
            result.push_back(chars[((val << 8) >> (valb + 8)) & 0x3F]);
        }
        
        while (result.size() % 4) {
            result.push_back('=');
        }
        
        return result;
    }
    
    bool parseUrl(const std::string& url, std::string& host, std::string& path, int& port) {
        size_t pos = url.find("://");
        if (pos == std::string::npos) return false;
        
        std::string protocol = url.substr(0, pos);
        port = (protocol == "https") ? 443 : 80;
        
        size_t hostStart = pos + 3;
        size_t hostEnd = url.find('/', hostStart);
        
        if (hostEnd == std::string::npos) {
            host = url.substr(hostStart);
            path = "/";
        } else {
            host = url.substr(hostStart, hostEnd - hostStart);
            path = url.substr(hostEnd);
        }
        
        size_t colonPos = host.find(':');
        if (colonPos != std::string::npos) {
            port = std::stoi(host.substr(colonPos + 1));
            host = host.substr(0, colonPos);
        }
        
        return true;
    }
    
    std::string extractFileName(const std::string& url) {
        size_t pos = url.rfind('/');
        if (pos != std::string::npos && pos < url.length() - 1) {
            std::string name = url.substr(pos + 1);
            size_t queryPos = name.find('?');
            if (queryPos != std::string::npos) {
                name = name.substr(0, queryPos);
            }
            return name.empty() ? "download" : name;
        }
        return "download";
    }
};

class DownloadManager {
private:
    DownloadEngine engine;
    HWND mainWindow{nullptr};
    HWND listView{nullptr};
    HWND urlEdit{nullptr};
    HWND pathEdit{nullptr};
    HWND threadEdit{nullptr};
    HWND addButton{nullptr};
    HWND pauseButton{nullptr};
    HWND resumeButton{nullptr};
    HWND cancelButton{nullptr};
    HWND removeButton{nullptr};
    NOTIFYICONDATA nid{};
    AppSettings settings;
    int selectedTaskIndex{-1};
    HWND settingsWindow{nullptr};
    
public:
    DownloadManager() {}
    ~DownloadManager() { engine.stop(); }
    
    void run(HINSTANCE hInstance) {
        InitCommonControls();
        
        WNDCLASSEX wc = {0};
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = hInstance;
        wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"DownloadManagerClass";
        wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
        RegisterClassEx(&wc);
        
        WNDCLASSEX swc = {0};
        swc.cbSize = sizeof(WNDCLASSEX);
        swc.style = CS_HREDRAW | CS_VREDRAW;
        swc.lpfnWndProc = SettingsWindowProc;
        swc.hInstance = hInstance;
        swc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        swc.hCursor = LoadCursor(NULL, IDC_ARROW);
        swc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        swc.lpszClassName = L"SettingsWindowClass";
        swc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
        RegisterClassEx(&swc);
        
        mainWindow = CreateWindowEx(
            0, L"DownloadManagerClass", L"下载管理器 - DM v3.0",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, 1000, 680,
            NULL, NULL, hInstance, this
        );
        
        createControls(hInstance);
        createTrayIcon(hInstance);
        
        engine.setNotifyWindow(mainWindow);
        engine.setSettings(settings);
        engine.start();
        
        ShowWindow(mainWindow, SW_SHOW);
        UpdateWindow(mainWindow);
        
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    
private:
    void createControls(HINSTANCE hInstance) {
        CreateWindowEx(0, L"STATIC", L"URL:", WS_VISIBLE | WS_CHILD,
            10, 10, 50, 20, mainWindow, NULL, hInstance, NULL);
        
        urlEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL,
            60, 10, 700, 22, mainWindow, NULL, hInstance, NULL);
        
        CreateWindowEx(0, L"STATIC", L"保存到:", WS_VISIBLE | WS_CHILD,
            10, 40, 50, 20, mainWindow, NULL, hInstance, NULL);
        
        std::wstring defaultPathW(settings.defaultSavePath.begin(), settings.defaultSavePath.end());
        pathEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", 
            defaultPathW.c_str(),
            WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL,
            60, 40, 500, 22, mainWindow, NULL, hInstance, NULL);
        
        CreateWindowEx(0, L"BUTTON", L"浏览...", WS_VISIBLE | WS_CHILD,
            570, 40, 60, 22, mainWindow, (HMENU)ID_BROWSE, hInstance, NULL);
        
        CreateWindowEx(0, L"STATIC", L"线程:", WS_VISIBLE | WS_CHILD,
            640, 40, 35, 20, mainWindow, NULL, hInstance, NULL);
        
        std::wstring threadsW = std::to_wstring(settings.defaultThreadCount);
        threadEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", threadsW.c_str(),
            WS_VISIBLE | WS_CHILD | ES_NUMBER,
            680, 40, 40, 22, mainWindow, NULL, hInstance, NULL);
        
        addButton = CreateWindowEx(0, L"BUTTON", L"添加下载", WS_VISIBLE | WS_CHILD,
            780, 10, 100, 52, mainWindow, (HMENU)ID_ADD, hInstance, NULL);
        
        CreateWindowEx(0, L"STATIC", L"支持: HTTP/HTTPS | SOCKS5/HTTP代理 | 多线程 | 断点续传 | 剩余时间", 
            WS_VISIBLE | WS_CHILD,
            10, 65, 800, 20, mainWindow, NULL, hInstance, NULL);
        
        listView = CreateWindowEx(0, WC_LISTVIEW, L"",
            WS_VISIBLE | WS_CHILD | LVS_REPORT | LVS_SINGLESEL | WS_VSCROLL | WS_HSCROLL,
            10, 90, 965, 480, mainWindow, NULL, hInstance, NULL);
        
        LVCOLUMN lvc;
        lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
        
        lvc.iSubItem = 0; lvc.pszText = L"文件名"; lvc.cx = 250; lvc.fmt = LVCFMT_LEFT;
        ListView_InsertColumn(listView, 0, &lvc);
        lvc.iSubItem = 1; lvc.pszText = L"大小"; lvc.cx = 100; lvc.fmt = LVCFMT_RIGHT;
        ListView_InsertColumn(listView, 1, &lvc);
        lvc.iSubItem = 2; lvc.pszText = L"进度"; lvc.cx = 80; lvc.fmt = LVCFMT_CENTER;
        ListView_InsertColumn(listView, 2, &lvc);
        lvc.iSubItem = 3; lvc.pszText = L"速度"; lvc.cx = 100; lvc.fmt = LVCFMT_RIGHT;
        ListView_InsertColumn(listView, 3, &lvc);
        lvc.iSubItem = 4; lvc.pszText = L"剩余"; lvc.cx = 80; lvc.fmt = LVCFMT_CENTER;
        ListView_InsertColumn(listView, 4, &lvc);
        lvc.iSubItem = 5; lvc.pszText = L"状态"; lvc.cx = 80; lvc.fmt = LVCFMT_CENTER;
        ListView_InsertColumn(listView, 5, &lvc);
        lvc.iSubItem = 6; lvc.pszText = L"URL"; lvc.cx = 275; lvc.fmt = LVCFMT_LEFT;
        ListView_InsertColumn(listView, 6, &lvc);
        
        pauseButton = CreateWindowEx(0, L"BUTTON", L"暂停", WS_VISIBLE | WS_CHILD,
            10, 580, 80, 30, mainWindow, (HMENU)ID_PAUSE, hInstance, NULL);
        
        resumeButton = CreateWindowEx(0, L"BUTTON", L"继续", WS_VISIBLE | WS_CHILD,
            100, 580, 80, 30, mainWindow, (HMENU)ID_RESUME, hInstance, NULL);
        
        cancelButton = CreateWindowEx(0, L"BUTTON", L"取消", WS_VISIBLE | WS_CHILD,
            190, 580, 80, 30, mainWindow, (HMENU)ID_CANCEL, hInstance, NULL);
        
        removeButton = CreateWindowEx(0, L"BUTTON", L"删除", WS_VISIBLE | WS_CHILD,
            280, 580, 80, 30, mainWindow, (HMENU)ID_REMOVE, hInstance, NULL);
        
        CreateWindowEx(0, L"BUTTON", L"打开文件夹", WS_VISIBLE | WS_CHILD,
            370, 580, 100, 30, mainWindow, (HMENU)ID_OPENFOLDER, hInstance, NULL);
        
        CreateWindowEx(0, L"BUTTON", L"设置", WS_VISIBLE | WS_CHILD,
            890, 580, 80, 30, mainWindow, (HMENU)ID_SETTINGS, hInstance, NULL);
    }
    
    void createTrayIcon(HINSTANCE hInstance) {
        nid.cbSize = sizeof(NOTIFYICONDATA);
        nid.hWnd = mainWindow;
        nid.uID = 1;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = WM_TRAY_ICON;
        nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        wcscpy_s(nid.szTip, L"下载管理器 v3.0");
        
        Shell_NotifyIcon(NIM_ADD, &nid);
    }
    
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        DownloadManager* dm = nullptr;
        
        if (uMsg == WM_CREATE) {
            CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
            dm = (DownloadManager*)cs->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)dm);
        } else {
            dm = (DownloadManager*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        }
        
        if (dm) {
            return dm->handleMessage(hwnd, uMsg, wParam, lParam);
        }
        
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    
    static LRESULT CALLBACK SettingsWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        DownloadManager* dm = nullptr;
        if (uMsg == WM_CREATE) {
            CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
            dm = (DownloadManager*)cs->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)dm);
        } else {
            dm = (DownloadManager*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        }
        
        if (dm) {
            switch (uMsg) {
                case WM_COMMAND: {
                    int id = LOWORD(wParam);
                    if (id == ID_SAVE_SETTINGS) {
                        dm->saveSettings(hwnd);
                        DestroyWindow(hwnd);
                    } else if (id == ID_CANCEL_SETTINGS) {
                        DestroyWindow(hwnd);
                    }
                    break;
                }
                case WM_CLOSE:
                    DestroyWindow(hwnd);
                    break;
                case WM_DESTROY:
                    if (dm) dm->settingsWindow = nullptr;
                    break;
                default:
                    return DefWindowProc(hwnd, uMsg, wParam, lParam);
            }
            return 0;
        }
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    
    LRESULT handleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
            case WM_COMMAND: {
                int id = LOWORD(wParam);
                
                switch (id) {
                    case ID_BROWSE: browseFolder(); break;
                    case ID_ADD: addDownload(); break;
                    case ID_PAUSE: pauseDownload(); break;
                    case ID_RESUME: resumeDownload(); break;
                    case ID_CANCEL: cancelDownload(); break;
                    case ID_REMOVE: removeDownload(); break;
                    case ID_OPENFOLDER: openFolder(); break;
                    case ID_SETTINGS: showSettings(); break;
                    case ID_TRAY_SHOW: ShowWindow(hwnd, SW_SHOW); SetForegroundWindow(hwnd); break;
                    case ID_TRAY_EXIT: DestroyWindow(hwnd); break;
                }
                break;
            }
            
            case WM_NOTIFY: {
                NMHDR* nmhdr = (NMHDR*)lParam;
                if (nmhdr->hwndFrom == listView && nmhdr->code == LVN_ITEMCHANGED) {
                    NMLISTVIEW* nmlv = (NMLISTVIEW*)lParam;
                    if (nmlv->uNewState & LVIS_SELECTED) {
                        selectedTaskIndex = nmlv->iItem;
                    }
                }
                break;
            }
            
            case WM_DOWNLOAD_COMPLETE: {
                DownloadTask* task = (DownloadTask*)wParam;
                showNotification(L"下载完成", 
                    std::wstring(task->fileName.begin(), task->fileName.end()).c_str());
                refreshList();
                break;
            }
            
            case WM_DOWNLOAD_PROGRESS: {
                refreshList();
                break;
            }
            
            case WM_DOWNLOAD_ERROR: {
                DownloadTask* task = (DownloadTask*)wParam;
                showNotification(L"下载失败", 
                    std::wstring(task->errorMessage.begin(), task->errorMessage.end()).c_str());
                refreshList();
                break;
            }
            
            case WM_TRAY_ICON: {
                if (lParam == WM_RBUTTONUP) {
                    POINT pt;
                    GetCursorPos(&pt);
                    
                    HMENU hMenu = CreatePopupMenu();
                    AppendMenu(hMenu, MF_STRING, ID_TRAY_SH