/*
 * SevenZip SDK - 进度回调示例
 * 
 * 本示例演示:
 * - 设置进度回调
 * - 显示压缩/解压进度
 * - 取消操作
 * - 自定义进度显示
 */

#include "../7zsdk.cpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>

using namespace SevenZip;

void PrintSeparator(const std::string& title) {
    std::cout << "\n========== " << title << " ==========\n" << std::endl;
}

// 进度条显示
void ShowProgressBar(double percent, const std::string& currentFile = "") {
    int barWidth = 50;
    std::cout << "\r  [";
    
    int pos = (int)(barWidth * percent / 100.0);
    for (int i = 0; i < barWidth; i++) {
        if (i < pos) std::cout << "=";
        else if (i == pos) std::cout << ">";
        else std::cout << " ";
    }
    
    std::cout << "] " << std::fixed << std::setprecision(1) << percent << "%";
    
    if (!currentFile.empty()) {
        std::cout << " - " << currentFile.substr(0, 30);
        if (currentFile.length() > 30) std::cout << "...";
    }
    
    std::cout << std::flush;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "   SevenZip SDK - 进度回调示例" << std::endl;
    std::cout << "========================================" << std::endl;
    
    SevenZipArchive archive;
    
    // ========================================
    // 1. 基础进度回调
    // ========================================
    PrintSeparator("1. 基础进度回调");
    
    ProgressCallback basicCallback = [](const ProgressInfo& info) {
        std::cout << "进度: " << info.percentDone << "%" << std::endl;
        std::cout << "当前文件: " << info.currentFile << std::endl;
        std::cout << "已处理: " << info.bytesProcessed << " / " << info.totalBytes << " bytes" << std::endl;
        return true;  // 返回 true 继续，false 取消
    };
    
    archive.SetProgressCallback(basicCallback);
    
    std::cout << "已设置进度回调" << std::endl;
    std::cout << "\n示例代码:" << std::endl;
    std::cout << "  ProgressCallback callback = [](const ProgressInfo& info) {" << std::endl;
    std::cout << "      std::cout << \"进度: \" << info.percentDone << \"%\" << std::endl;" << std::endl;
    std::cout << "      return true;" << std::endl;
    std::cout << "  };" << std::endl;
    std::cout << "  archive.SetProgressCallback(callback);" << std::endl;
    
    // ========================================
    // 2. 进度条显示
    // ========================================
    PrintSeparator("2. 进度条显示");
    
    ProgressCallback progressBarCallback = [](const ProgressInfo& info) {
        ShowProgressBar(info.percentDone, info.currentFile);
        return true;
    };
    
    std::cout << "进度条效果预览:" << std::endl;
    
    // 模拟进度
    for (int i = 0; i <= 100; i += 5) {
        ShowProgressBar(i, "example_file_" + std::to_string(i) + ".txt");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << std::endl;
    
    // ========================================
    // 3. 详细进度信息
    // ========================================
    PrintSeparator("3. 详细进度信息");
    
    std::cout << "ProgressInfo 结构包含:" << std::endl;
    std::cout << "  - percentDone: 完成百分比" << std::endl;
    std::cout << "  - currentFile: 当前处理的文件" << std::endl;
    std::cout << "  - bytesProcessed: 已处理字节数" << std::endl;
    std::cout << "  - totalBytes: 总字节数" << std::endl;
    std::cout << "  - filesProcessed: 已处理文件数" << std::endl;
    std::cout << "  - totalFiles: 总文件数" << std::endl;
    std::cout << "  - currentSpeed: 当前处理速度 (bytes/s)" << std::endl;
    std::cout << "  - elapsedTime: 已用时间 (秒)" << std::endl;
    std::cout << "  - estimatedRemaining: 预计剩余时间 (秒)" << std::endl;
    
    // ========================================
    // 4. 带速度显示的回调
    // ========================================
    PrintSeparator("4. 带速度显示的回调");
    
    ProgressCallback detailedCallback = [](const ProgressInfo& info) {
        std::cout << "\r  " << std::fixed << std::setprecision(1) << info.percentDone << "%";
        std::cout << " | " << std::setw(10) << FormatSize(info.bytesProcessed);
        std::cout << " / " << std::setw(10) << FormatSize(info.totalBytes);
        std::cout << " | " << std::setw(8) << FormatSize(info.currentSpeed) << "/s";
        std::cout << " | ETA: " << std::setw(5) << info.estimatedRemaining << "s";
        std::cout << std::flush;
        return true;
    };
    
    std::cout << "格式说明:" << std::endl;
    std::cout << "  百分比 | 已处理 / 总大小 | 速度 | 预计剩余时间" << std::endl;
    
    // ========================================
    // 5. 取消操作
    // ========================================
    PrintSeparator("5. 取消操作");
    
    bool cancelRequested = false;
    int fileCount = 0;
    
    ProgressCallback cancellableCallback = [&](const ProgressInfo& info) {
        fileCount++;
        
        // 示例: 处理超过 100 个文件后取消
        if (fileCount > 100) {
            std::cout << "\n请求取消操作..." << std::endl;
            return false;  // 返回 false 取消操作
        }
        
        return true;
    };
    
    std::cout << "取消操作示例:" << std::endl;
    std::cout << "  在回调函数中返回 false 即可取消操作" << std::endl;
    std::cout << "\n示例代码:" << std::endl;
    std::cout << "  ProgressCallback callback = [](const ProgressInfo& info) {" << std::endl;
    std::cout << "      if (shouldCancel) {" << std::endl;
    std::cout << "          return false;  // 取消操作" << std::endl;
    std::cout << "      }" << std::endl;
    std::cout << "      return true;  // 继续操作" << std::endl;
    std::cout << "  };" << std::endl;
    
    // ========================================
    // 6. 多线程安全回调
    // ========================================
    PrintSeparator("6. 多线程安全回调");
    
    std::mutex consoleMutex;
    
    ProgressCallback threadSafeCallback = [&](const ProgressInfo& info) {
        std::lock_guard<std::mutex> lock(consoleMutex);
        
        // 线程安全的输出
        std::cout << "\r  进度: " << info.percentDone << "%" << std::flush;
        
        return true;
    };
    
    std::cout << "多线程环境下使用互斥锁保护控制台输出" << std::endl;
    
    // ========================================
    // 7. 完整示例
    // ========================================
    PrintSeparator("7. 完整进度回调示例");
    
    auto startTime = std::chrono::steady_clock::now();
    
    ProgressCallback completeCallback = [&](const ProgressInfo& info) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
        
        // 清除当前行
        std::cout << "\r" << std::string(80, ' ') << "\r";
        
        // 进度条
        int barWidth = 30;
        std::cout << "  [";
        int pos = (int)(barWidth * info.percentDone / 100.0);
        for (int i = 0; i < barWidth; i++) {
            if (i < pos) std::cout << "=";
            else std::cout << " ";
        }
        std::cout << "] ";
        
        // 百分比
        std::cout << std::fixed << std::setprecision(1) << info.percentDone << "%";
        
        // 文件计数
        std::cout << " (" << info.filesProcessed << "/" << info.totalFiles << " files)";
        
        // 速度
        if (info.currentSpeed > 0) {
            std::cout << " " << FormatSize(info.currentSpeed) << "/s";
        }
        
        // 时间
        std::cout << " [" << elapsed << "s";
        if (info.estimatedRemaining > 0) {
            std::cout << " / ETA: " << info.estimatedRemaining << "s";
        }
        std::cout << "]";
        
        std::cout << std::flush;
        return true;
    };
    
    std::cout << "完整进度显示格式:" << std::endl;
    std::cout << "  [=============           ] 45.5% (45/100 files) 25.3 MB/s [5s / ETA: 6s]" << std::endl;
    
    // ========================================
    // 8. 使用回调进行压缩
    // ========================================
    PrintSeparator("8. 使用回调进行压缩");
    
    std::cout << "完整使用示例:" << std::endl;
    std::cout << std::endl;
    std::cout << "  SevenZipArchive archive;" << std::endl;
    std::cout << std::endl;
    std::cout << "  // 设置进度回调" << std::endl;
    std::cout << "  archive.SetProgressCallback([](const ProgressInfo& info) {" << std::endl;
    std::cout << "      std::cout << \"\\r进度: \" << info.percentDone << \"%\" << std::flush;" << std::endl;
    std::cout << "      return true;" << std::endl;
    std::cout << "  });" << std::endl;
    std::cout << std::endl;
    std::cout << "  // 执行压缩" << std::endl;
    std::cout << "  CompressionOptions opts;" << std::endl;
    std::cout << "  archive.CompressDirectory(\"backup.7z\", \"C:\\\\Data\", opts, true);" << std::endl;
    std::cout << std::endl;
    std::cout << "  std::cout << std::endl << \"完成!\" << std::endl;" << std::endl;
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "   进度回调示例完成" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}

// 辅助函数: 格式化大小
std::string FormatSize(uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double size = (double)bytes;
    
    while (size >= 1024 && unitIndex < 4) {
        size /= 1024;
        unitIndex++;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << size << " " << units[unitIndex];
    return oss.str();
}
