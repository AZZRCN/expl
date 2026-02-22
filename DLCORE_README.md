# DLCore 下载库

一个用于 Windows 的多线程下载库，支持 HTTP/HTTPS、代理、断点续传等功能。

## 功能特性

- ✅ 多线程分片下载
- ✅ 断点续传
- ✅ HTTP/HTTPS 协议
- ✅ 代理支持 (HTTP/SOCKS4/SOCKS5)
- ✅ 下载限速
- ✅ 自动重试
- ✅ MD5 校验
- ✅ 日志系统
- ✅ SSL 证书验证

## 快速开始

```cpp
#include "dlcore.h"
using namespace dl;

int main() {
    Initialize();
    
    auto manager = DownloadManager::create();
    
    Config config;
    config.defaultThreadCount = 4;
    config.defaultSavePath = "./downloads";
    manager->setConfig(config);
    
    manager->setProgressCallback([](const std::string& taskId, int progress, 
                                     uint64_t downloaded, uint64_t total, uint64_t speed) {
        std::cout << "Progress: " << progress << "%" << std::endl;
    });
    
    manager->start();
    
    std::string taskId = manager->addTask("https://example.com/file.zip", "./downloads");
    manager->waitForTask(taskId);
    
    manager->stop();
    DownloadManager::destroy(manager);
    
    Cleanup();
    return 0;
}
```

## 配置选项

| 选项 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `maxConcurrentDownloads` | int | 3 | 最大并发下载数 |
| `speedLimitKB` | int | 0 | 限速 (KB/s)，0=不限 |
| `defaultThreadCount` | int | 4 | 默认线程数 |
| `defaultSavePath` | string | "." | 默认保存路径 |
| `maxRetries` | int | 3 | 最大重试次数 |
| `retryDelayMs` | int | 1000 | 重试间隔 (ms) |
| `verifySsl` | bool | true | 是否验证 SSL |
| `verifyChecksum` | bool | true | 是否校验 MD5 |
| `connectTimeoutMs` | int | 30000 | 连接超时 (ms) |
| `readTimeoutMs` | int | 30000 | 读取超时 (ms) |

## 主要 API

### 全局函数

```cpp
bool Initialize();              // 初始化库
void Cleanup();                 // 清理资源
std::string GetVersion();       // 获取版本号
std::string CalculateFileMd5(const std::string& path);  // 计算文件 MD5
```

### DownloadManager

```cpp
// 创建/销毁
static DownloadManager* create();
static void destroy(DownloadManager* manager);

// 配置
void setConfig(const Config& config);
Config getConfig() const;

// 回调
void setProgressCallback(ProgressCallback cb);
void setCompleteCallback(CompleteCallback cb);
void setErrorCallback(ErrorCallback cb);
void setStatusCallback(StatusCallback cb);
void setLogCallback(LogCallback cb);

// 任务管理
std::string addTask(const std::string& url, const std::string& savePath, int threads = 0);
std::string addTaskWithMd5(const std::string& url, const std::string& savePath, int threads, const std::string& md5);
bool pauseTask(const std::string& taskId);
bool resumeTask(const std::string& taskId);
bool cancelTask(const std::string& taskId);
bool removeTask(const std::string& taskId);
bool retryTask(const std::string& taskId);

// 查询
TaskInfo getTaskInfo(const std::string& taskId) const;
std::vector<TaskInfo> getAllTasks() const;

// 控制
void start();
void stop();
bool isRunning() const;
void waitForTask(const std::string& taskId, int timeoutMs = -1);
void waitForAll(int timeoutMs = -1);
```

## 任务状态

| 状态 | 说明 |
|------|------|
| `Pending` | 等待中 |
| `Downloading` | 下载中 |
| `Paused` | 已暂停 |
| `Completed` | 已完成 |
| `Error` | 出错 |
| `Cancelled` | 已取消 |

## 代理配置

```cpp
Config config;

// HTTP 代理
config.proxy.type = ProxyType::HTTP;
config.proxy.host = "127.0.0.1";
config.proxy.port = 8080;

// SOCKS5 代理
config.proxy.type = ProxyType::SOCKS5;
config.proxy.host = "127.0.0.1";
config.proxy.port = 1080;
config.proxy.username = "user";
config.proxy.password = "pass";

// SOCKS4 代理
config.proxy.type = ProxyType::SOCKS4;
config.proxy.host = "127.0.0.1";
config.proxy.port = 1080;
```

## 日志配置

```cpp
Config config;
config.logging.level = LogLevel::Debug;
config.logging.logToConsole = true;
config.logging.logToFile = true;
config.logging.logFilePath = "./dlcore.log";
```

## 编译

需要链接以下库：
- `ws2_32` (Winsock)
- `winhttp` (WinHTTP)
- `advapi32` (CryptoAPI for MD5)

### MSVC (cl.exe)

```bash
# 编译动态库
cl /LD /std:c++17 dlcore.cpp /Fe:dlcore.dll /link ws2_32.lib winhttp.lib advapi32.lib

# 编译示例程序
cl /std:c++17 dlcore_demo.cpp /Fe:dlcore_demo.exe /link dlcore.lib ws2_32.lib winhttp.lib advapi32.lib
```

### MinGW-w64 (g++)

```bash
# 编译动态库
g++ -std=c++17 -shared -o dlcore.dll dlcore.cpp -lws2_32 -lwinhttp -ladvapi32

# 编译示例程序
g++ -std=c++17 -o dlcore_demo.exe dlcore_demo.cpp -L. -ldlcore -lws2_32 -lwinhttp -ladvapi32
```

### 一键编译脚本 (build.bat)

```batch
@echo off
echo Building DLCore...

REM 编译动态库
g++ -std=c++17 -shared -o dlcore.dll dlcore.cpp -lws2_32 -lwinhttp -ladvapi32
if %errorlevel% neq 0 exit /b %errorlevel%

REM 编译示例程序
g++ -std=c++17 -o dlcore_demo.exe dlcore_demo.cpp -L. -ldlcore -lws2_32 -lwinhttp -ladvapi32
if %errorlevel% neq 0 exit /b %errorlevel%

echo Build complete!
echo Run: dlcore_demo.exe
```

## 文件说明

| 文件 | 说明 |
|------|------|
| `dlcore.h` | 头文件，包含所有公开 API |
| `dlcore.cpp` | 实现文件 |
| `dlcore.dll` | 编译后的动态库 |
| `dlcore_demo.cpp` | 示例程序 |

## 版本

当前版本：**1.1.0**
