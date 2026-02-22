/**
 * @file dlcore_demo.cpp
 * @brief Demonstration program for DLCore download library
 * 
 * This file demonstrates all features of the DLCore library including:
 * - Basic downloads
 * - Multi-threaded downloads
 * - Proxy support (HTTP/SOCKS4/SOCKS5)
 * - Pause and resume
 * - Speed limiting
 * - MD5 verification
 * - Retry mechanism
 * - Logging
 */

#include "dlcore.h"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <iomanip>

using namespace dl;

void PrintTaskInfo(const TaskInfo& info) {
    std::cout << "Task ID: " << info.id << std::endl;
    std::cout << "  URL: " << info.url << std::endl;
    std::cout << "  File: " << info.fileName << std::endl;
    std::cout << "  Save Path: " << info.savePath << std::endl;
    std::cout << "  Progress: " << info.progress << "%" << std::endl;
    std::cout << "  Downloaded: " << info.downloadedSize << " / " << info.totalSize << " bytes" << std::endl;
    std::cout << "  Speed: " << (info.speed / 1024) << " KB/s" << std::endl;
    std::cout << "  Status: ";
    switch (info.status) {
        case Status::Pending: std::cout << "Pending"; break;
        case Status::Downloading: std::cout << "Downloading"; break;
        case Status::Paused: std::cout << "Paused"; break;
        case Status::Completed: std::cout << "Completed"; break;
        case Status::Error: std::cout << "Error"; break;
        case Status::Cancelled: std::cout << "Cancelled"; break;
    }
    std::cout << std::endl;
    if (!info.errorMessage.empty()) {
        std::cout << "  Error: " << info.errorMessage << std::endl;
    }
    if (!info.expectedMd5.empty()) {
        std::cout << "  Expected MD5: " << info.expectedMd5 << std::endl;
        std::cout << "  Verified: " << (info.verified ? "Yes" : "No") << std::endl;
    }
    if (info.retryCount > 0) {
        std::cout << "  Retry Count: " << info.retryCount << std::endl;
    }
    std::cout << std::endl;
}

void Example1_BasicDownload() {
    std::cout << "=== Example 1: Basic Download ===" << std::endl;
    
    DownloadManager* manager = DownloadManager::create();
    
    Config config;
    config.maxConcurrentDownloads = 2;
    config.defaultThreadCount = 4;
    config.defaultSavePath = "./downloads";
    config.maxRetries = 3;
    config.retryDelayMs = 1000;
    manager->setConfig(config);
    
    manager->setProgressCallback([](const std::string& taskId, int progress, 
                                     uint64_t downloaded, uint64_t total, uint64_t speed) {
        std::cout << "\rProgress: " << progress << "% | " 
                  << downloaded << "/" << total << " bytes | "
                  << (speed / 1024) << " KB/s    " << std::flush;
    });
    
    manager->setCompleteCallback([](const std::string& taskId, const std::string& filePath) {
        std::cout << std::endl << "Download completed: " << filePath << std::endl;
    });
    
    manager->setErrorCallback([](const std::string& taskId, const std::string& errorMessage, bool willRetry) {
        std::cout << std::endl << "Download error: " << errorMessage;
        if (willRetry) {
            std::cout << " (will retry...)";
        }
        std::cout << std::endl;
    });
    
    manager->start();
    
    std::string taskId = manager->addTask(
        "https://www.baidu.com/index.html",
        "./downloads",
        4
    );
    
    if (taskId.empty()) {
        std::cout << "Failed to add task" << std::endl;
    } else {
        std::cout << "Task added with ID: " << taskId << std::endl;
        manager->waitForTask(taskId, 60000);
    }
    
    manager->stop();
    DownloadManager::destroy(manager);
    
    std::cout << std::endl;
}

void Example2_MultiDownload() {
    std::cout << "=== Example 2: Multiple Downloads ===" << std::endl;
    
    DownloadManager* manager = DownloadManager::create();
    
    Config config;
    config.maxConcurrentDownloads = 3;
    config.defaultThreadCount = 8;
    manager->setConfig(config);
    
    manager->setStatusCallback([](const std::string& taskId, Status status) {
        std::cout << "Task " << taskId << " status changed to ";
        switch (status) {
            case Status::Pending: std::cout << "Pending"; break;
            case Status::Downloading: std::cout << "Downloading"; break;
            case Status::Paused: std::cout << "Paused"; break;
            case Status::Completed: std::cout << "Completed"; break;
            case Status::Error: std::cout << "Error"; break;
            case Status::Cancelled: std::cout << "Cancelled"; break;
        }
        std::cout << std::endl;
    });
    
    manager->start();
    
    std::vector<std::string> urls = {
        "https://www.baidu.com/",
        "https://www.bing.com/",
    };
    
    std::vector<std::string> taskIds;
    for (const auto& url : urls) {
        std::string taskId = manager->addTask(url, "./downloads", 4);
        if (!taskId.empty()) {
            taskIds.push_back(taskId);
            std::cout << "Added task: " << taskId << " for " << url << std::endl;
        }
    }
    
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    std::cout << "\n--- Task Status ---" << std::endl;
    auto tasks = manager->getAllTasks();
    for (const auto& info : tasks) {
        PrintTaskInfo(info);
    }
    
    manager->waitForAll(30000);
    
    manager->stop();
    DownloadManager::destroy(manager);
}

void Example3_WithProxy() {
    std::cout << "=== Example 3: Download with Proxy ===" << std::endl;
    
    DownloadManager* manager = DownloadManager::create();
    
    Config config;
    config.maxConcurrentDownloads = 2;
    config.defaultThreadCount = 4;
    config.defaultSavePath = "./downloads";
    
    std::cout << "Proxy configuration examples (commented out):" << std::endl;
    std::cout << "  HTTP Proxy: host=127.0.0.1, port=8080" << std::endl;
    std::cout << "  SOCKS5 Proxy: host=127.0.0.1, port=1080" << std::endl;
    std::cout << "  SOCKS4 Proxy: host=127.0.0.1, port=1080" << std::endl;
    std::cout << std::endl;
    
    // HTTP Proxy
    // config.proxy.type = ProxyType::HTTP;
    // config.proxy.host = "127.0.0.1";
    // config.proxy.port = 8080;
    
    // SOCKS5 Proxy
    // config.proxy.type = ProxyType::SOCKS5;
    // config.proxy.host = "127.0.0.1";
    // config.proxy.port = 1080;
    // config.proxy.username = "user";
    // config.proxy.password = "pass";
    
    // SOCKS4 Proxy
    // config.proxy.type = ProxyType::SOCKS4;
    // config.proxy.host = "127.0.0.1";
    // config.proxy.port = 1080;
    
    manager->setConfig(config);
    manager->start();
    
    std::string taskId = manager->addTask(
        "https://www.baidu.com/",
        "./downloads",
        4
    );
    
    if (!taskId.empty()) {
        std::cout << "Task added: " << taskId << std::endl;
        manager->waitForTask(taskId, 30000);
    }
    
    manager->stop();
    DownloadManager::destroy(manager);
    
    std::cout << std::endl;
}

void Example4_PauseResume() {
    std::cout << "=== Example 4: Pause and Resume ===" << std::endl;
    
    DownloadManager* manager = DownloadManager::create();
    
    Config config;
    config.defaultThreadCount = 4;
    manager->setConfig(config);
    
    manager->setProgressCallback([](const std::string& taskId, int progress, 
                                     uint64_t downloaded, uint64_t total, uint64_t speed) {
        std::cout << "\r[" << taskId << "] Progress: " << progress << "%    " << std::flush;
    });
    
    manager->start();
    
    std::string taskId = manager->addTask(
        "https://www.baidu.com/",
        "./downloads",
        4
    );
    
    if (!taskId.empty()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        std::cout << std::endl << "Pausing task..." << std::endl;
        manager->pauseTask(taskId);
        
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        std::cout << "Resuming task..." << std::endl;
        manager->resumeTask(taskId);
        
        manager->waitForTask(taskId, 30000);
    }
    
    manager->stop();
    DownloadManager::destroy(manager);
    
    std::cout << std::endl;
}

void Example5_SpeedLimit() {
    std::cout << "=== Example 5: Speed Limit ===" << std::endl;
    
    DownloadManager* manager = DownloadManager::create();
    
    Config config;
    config.speedLimitKB = 500;
    config.defaultThreadCount = 4;
    manager->setConfig(config);
    
    manager->setProgressCallback([](const std::string& taskId, int progress, 
                                     uint64_t downloaded, uint64_t total, uint64_t speed) {
        std::cout << "\rProgress: " << progress << "% | Speed: " 
                  << (speed / 1024) << " KB/s    " << std::flush;
    });
    
    manager->start();
    
    std::string taskId = manager->addTask(
        "https://www.baidu.com/",
        "./downloads",
        4
    );
    
    if (!taskId.empty()) {
        manager->waitForTask(taskId, 60000);
    }
    
    manager->stop();
    DownloadManager::destroy(manager);
    
    std::cout << std::endl;
}

void Example6_Md5Verification() {
    std::cout << "=== Example 6: MD5 Verification ===" << std::endl;
    
    DownloadManager* manager = DownloadManager::create();
    
    Config config;
    config.defaultThreadCount = 4;
    config.verifyChecksum = true;
    manager->setConfig(config);
    
    manager->setCompleteCallback([](const std::string& taskId, const std::string& filePath) {
        std::cout << std::endl << "Download completed: " << filePath << std::endl;
        
        std::string md5 = CalculateFileMd5(filePath);
        if (!md5.empty()) {
            std::cout << "File MD5: " << md5 << std::endl;
        }
    });
    
    manager->setProgressCallback([](const std::string& taskId, int progress, 
                                     uint64_t downloaded, uint64_t total, uint64_t speed) {
        std::cout << "\rProgress: " << progress << "%    " << std::flush;
    });
    
    manager->start();
    
    std::string taskId = manager->addTaskWithMd5(
        "https://www.baidu.com/",
        "./downloads",
        4,
        ""
    );
    
    if (!taskId.empty()) {
        std::cout << "Task added with MD5 verification: " << taskId << std::endl;
        manager->waitForTask(taskId, 60000);
        
        TaskInfo info = manager->getTaskInfo(taskId);
        if (info.status == Status::Completed) {
            std::string actualMd5 = CalculateFileMd5(info.savePath + "\\" + info.fileName);
            std::cout << "Actual file MD5: " << actualMd5 << std::endl;
        }
    }
    
    manager->stop();
    DownloadManager::destroy(manager);
    
    std::cout << std::endl;
}

void Example7_Logging() {
    std::cout << "=== Example 7: Logging ===" << std::endl;
    
    DownloadManager* manager = DownloadManager::create();
    
    Config config;
    config.defaultThreadCount = 4;
    config.logging.level = LogLevel::Debug;
    config.logging.logToConsole = true;
    config.logging.logToFile = false;
    manager->setConfig(config);
    
    manager->setLogCallback([](LogLevel level, const std::string& message) {
        std::string levelStr;
        switch (level) {
            case LogLevel::Error: levelStr = "ERR"; break;
            case LogLevel::Warning: levelStr = "WRN"; break;
            case LogLevel::Info: levelStr = "INF"; break;
            case LogLevel::Debug: levelStr = "DBG"; break;
            default: levelStr = "???"; break;
        }
        std::cout << "[CUSTOM LOG][" << levelStr << "] " << message << std::endl;
    });
    
    manager->start();
    
    std::string taskId = manager->addTask(
        "https://www.baidu.com/",
        "./downloads",
        4
    );
    
    if (!taskId.empty()) {
        manager->waitForTask(taskId, 60000);
    }
    
    manager->stop();
    DownloadManager::destroy(manager);
    
    std::cout << std::endl;
}

void Example8_RetryMechanism() {
    std::cout << "=== Example 8: Retry Mechanism ===" << std::endl;
    
    DownloadManager* manager = DownloadManager::create();
    
    Config config;
    config.defaultThreadCount = 4;
    config.maxRetries = 3;
    config.retryDelayMs = 2000;
    manager->setConfig(config);
    
    manager->setErrorCallback([](const std::string& taskId, const std::string& errorMessage, bool willRetry) {
        std::cout << "Error: " << errorMessage;
        if (willRetry) {
            std::cout << " - Will retry automatically...";
        }
        std::cout << std::endl;
    });
    
    manager->setStatusCallback([](const std::string& taskId, Status status) {
        if (status == Status::Error) {
            std::cout << "Task entered error state" << std::endl;
        }
    });
    
    manager->start();
    
    std::string taskId = manager->addTask(
        "https://this-url-does-not-exist-12345.com/file.zip",
        "./downloads",
        4
    );
    
    if (!taskId.empty()) {
        std::cout << "Task added (this will fail and retry): " << taskId << std::endl;
        manager->waitForTask(taskId, 30000);
        
        TaskInfo info = manager->getTaskInfo(taskId);
        std::cout << "Final retry count: " << info.retryCount << std::endl;
    }
    
    manager->stop();
    DownloadManager::destroy(manager);
    
    std::cout << std::endl;
}

void Example9_SslConfiguration() {
    std::cout << "=== Example 9: SSL Configuration ===" << std::endl;
    
    DownloadManager* manager = DownloadManager::create();
    
    Config config;
    config.defaultThreadCount = 4;
    config.verifySsl = true;
    std::cout << "SSL verification: " << (config.verifySsl ? "enabled" : "disabled") << std::endl;
    manager->setConfig(config);
    
    manager->setProgressCallback([](const std::string& taskId, int progress, 
                                     uint64_t downloaded, uint64_t total, uint64_t speed) {
        std::cout << "\rProgress: " << progress << "%    " << std::flush;
    });
    
    manager->start();
    
    std::string taskId = manager->addTask(
        "https://www.baidu.com/",
        "./downloads",
        4
    );
    
    if (!taskId.empty()) {
        manager->waitForTask(taskId, 60000);
    }
    
    manager->stop();
    DownloadManager::destroy(manager);
    
    std::cout << std::endl;
}

void PrintUsage() {
    std::cout << "DLCore Demo - Download Library Example" << std::endl;
    std::cout << "Version: " << GetVersion() << std::endl;
    std::cout << std::endl;
    std::cout << "Usage: dlcore_demo [example_number]" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  1 - Basic single file download" << std::endl;
    std::cout << "  2 - Multiple concurrent downloads" << std::endl;
    std::cout << "  3 - Download with proxy (SOCKS4/SOCKS5/HTTP)" << std::endl;
    std::cout << "  4 - Pause and resume download" << std::endl;
    std::cout << "  5 - Download with speed limit" << std::endl;
    std::cout << "  6 - MD5 verification" << std::endl;
    std::cout << "  7 - Logging system" << std::endl;
    std::cout << "  8 - Retry mechanism" << std::endl;
    std::cout << "  9 - SSL configuration" << std::endl;
    std::cout << "  all - Run all examples" << std::endl;
    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
    if (!Initialize()) {
        std::cerr << "Failed to initialize DLCore" << std::endl;
        return 1;
    }
    
    std::string example = "1";
    if (argc > 1) {
        example = argv[1];
    }
    
    if (example == "all") {
        Example1_BasicDownload();
        Example2_MultiDownload();
        Example3_WithProxy();
        Example4_PauseResume();
        Example5_SpeedLimit();
        Example6_Md5Verification();
        Example7_Logging();
        Example8_RetryMechanism();
        Example9_SslConfiguration();
    } else if (example == "1") {
        Example1_BasicDownload();
    } else if (example == "2") {
        Example2_MultiDownload();
    } else if (example == "3") {
        Example3_WithProxy();
    } else if (example == "4") {
        Example4_PauseResume();
    } else if (example == "5") {
        Example5_SpeedLimit();
    } else if (example == "6") {
        Example6_Md5Verification();
    } else if (example == "7") {
        Example7_Logging();
    } else if (example == "8") {
        Example8_RetryMechanism();
    } else if (example == "9") {
        Example9_SslConfiguration();
    } else {
        PrintUsage();
    }
    
    Cleanup();
    return 0;
}
