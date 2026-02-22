# SevenZip SDK

**企业级 7-Zip 压缩包管理解决方案**

一个功能完整、易于使用的 C++ 压缩包管理 SDK，基于 7-Zip SDK 封装，提供从基础压缩/解压到企业级备份管理的完整解决方案。

---

## 📋 目录

- [特性](#特性)
- [快速开始](#快速开始)
- [编译说明](#编译说明)
- [API 参考](#api-参考)
- [示例代码](#示例代码)
- [配置文件](#配置文件)
- [许可证](#许可证)

---

## ✨ 特性

### 核心功能

| 功能 | 描述 |
|------|------|
| 🗜️ **压缩/解压** | 支持 7z, ZIP, TAR, GZIP, BZIP2, XZ 等格式 |
| 🔐 **加密支持** | AES-256, ChaCha20, Twofish, Serpent, Camellia |
| 📦 **分卷压缩** | 支持自定义分卷大小，自动分割/合并 |
| 🧵 **多线程** | 并行压缩/解压，充分利用多核 CPU |

### 高级功能

| 功能 | 描述 |
|------|------|
| 💾 **备份管理** | 完整备份、增量备份、差异备份 |
| ⏰ **时间线备份** | 基于时间点的版本控制 |
| 🔍 **智能分类** | 自动识别文件类型，智能归档 |
| 🛡️ **病毒扫描** | 内置特征码扫描，启发式分析 |
| 🔄 **格式转换** | 压缩包格式互转，批量转换 |
| ✅ **完整性验证** | CRC32, MD5, SHA256 校验 |
| 🔑 **密码管理** | 密码生成、存储、策略验证 |
| 📊 **差异备份** | 创建/应用增量包 |
| 🗃️ **数据去重** | 基于内容哈希的重复数据消除 |
| ☁️ **云存储** | 支持上传/下载到云存储 |

---

## 🚀 快速开始

### 最小示例

```cpp
#include "7zsdk.cpp"

using namespace SevenZip;

int main() {
    SevenZipArchive archive;
    
    // 压缩目录
    CompressionOptions opts;
    archive.CompressDirectory("backup.7z", "C:\\MyData", opts, true);
    
    // 解压文件
    ExtractOptions extractOpts;
    extractOpts.outputDir = "C:\\Restore";
    archive.ExtractArchive("backup.7z", extractOpts);
    
    return 0;
}
```

### 带进度的压缩

```cpp
#include "7zsdk.cpp"
#include <iostream>

using namespace SevenZip;

int main() {
    SevenZipArchive archive;
    
    ProgressCallback callback = [](const ProgressInfo& info) {
        std::cout << "进度: " << info.percentDone << "%"
                  << " - " << info.currentFile << std::endl;
        return true; // 返回 false 取消操作
    };
    
    archive.SetProgressCallback(callback);
    archive.CompressDirectory("backup.7z", "C:\\Data", CompressionOptions(), true);
    
    return 0;
}
```

---

## 🔨 编译说明

### 系统要求

- Windows 10/11
- MinGW-w64 GCC 8.0+ 或 MSVC 2019+
- C++17 支持

### 使用 MinGW 编译

```bash
# 编译目标文件
g++ -std=c++17 -c 7zsdk.cpp -o 7zsdk.o

# 链接可执行文件
g++ 7zsdk.o -o app.exe -lwininet -lole32 -loleaut32 -luuid
```

### 使用 MSVC 编译

```cmd
cl /std:c++17 /c 7zsdk.cpp
link 7zsdk.obj wininet.lib ole32.lib oleaut32.lib uuid.lib
```

### 依赖库

| 库 | 用途 |
|---|------|
| wininet.lib | 网络功能 (云存储、更新) |
| ole32.lib | COM 接口 |
| oleaut32.lib | BSTR 字符串处理 |
| uuid.lib | GUID 生成 |

---

## 📖 API 参考

### SevenZipArchive - 主类

```cpp
class SevenZipArchive {
public:
    // 压缩操作
    bool CompressDirectory(const std::string& archivePath, 
                          const std::string& directory,
                          const CompressionOptions& options,
                          bool recursive = true);
    
    bool CompressFiles(const std::string& archivePath,
                       const std::vector<std::string>& files,
                       const CompressionOptions& options);
    
    bool AddToArchive(const std::string& archivePath,
                      const std::vector<std::string>& files,
                      const CompressionOptions& options);
    
    // 解压操作
    bool ExtractArchive(const std::string& archivePath,
                        const ExtractOptions& options);
    
    bool ExtractFiles(const std::string& archivePath,
                      const std::vector<std::string>& files,
                      const std::string& outputDir,
                      const std::string& password = "");
    
    bool ExtractSingleFileToMemory(const std::string& archivePath,
                                   const std::string& filePath,
                                   std::vector<uint8_t>& data,
                                   const std::string& password = "");
    
    // 信息查询
    bool ListArchive(const std::string& archivePath,
                     ArchiveInfo& info,
                     const std::string& password = "");
    
    bool TestArchive(const std::string& archivePath,
                     const std::string& password = "");
    
    // 进度回调
    void SetProgressCallback(ProgressCallback callback);
    
    // 分卷操作
    bool CreateSplitArchive(const std::string& archivePath,
                           const std::string& directory,
                           uint64_t volumeSize,
                           const CompressionOptions& options);
    
    bool ExtractSplitArchive(const std::string& firstVolume,
                            const std::string& outputDir);
};
```

### CompressionOptions - 压缩选项

```cpp
struct CompressionOptions {
    CompressionMethod method = CompressionMethod::LZMA2;
    CompressionLevel level = CompressionLevel::Normal;
    std::string password;
    bool solidMode = false;
    uint32_t threadCount = 0;        // 0 = 自动
    uint64_t solidBlockSize = 0;     // 0 = 自动
    bool encryptHeaders = false;
};
```

### ExtractOptions - 解压选项

```cpp
struct ExtractOptions {
    std::string outputDir;
    std::string password;
    bool overwriteExisting = false;
    bool preserveDirectoryStructure = true;
    bool preserveFileTime = true;
    bool preserveFileAttrib = true;
    std::vector<std::string> fileFilters;
};
```

### ArchiveInfo - 压缩包信息

```cpp
struct ArchiveInfo {
    uint64_t uncompressedSize;
    uint64_t compressedSize;
    uint32_t fileCount;
    uint32_t directoryCount;
    bool isEncrypted;
    bool isSolid;
    std::vector<FileInfo> files;
};

struct FileInfo {
    std::string path;
    uint64_t size;
    uint64_t packedSize;
    uint32_t crc;
    bool isDirectory;
    bool isEncrypted;
    FILETIME lastWriteTime;
    uint32_t attributes;
};
```

---

## 📚 功能模块

### 1. 备份管理 (BackupManager)

```cpp
SevenZipArchive archive;
BackupManager backupMgr(archive);

BackupOptions opts;
opts.type = BackupType::Incremental;
opts.preservePermissions = true;
opts.excludePatterns = {"*.tmp", "*.log"};

BackupResult result;
backupMgr.CreateBackup("backup.7z", "C:\\Data", opts, result);
```

### 2. 时间线备份 (TimelineBackup)

```cpp
TimelineBackup timeline(archive, "timeline_path");

// 创建时间点
std::string entryId = timeline.CreateEntry("C:\\Data", "Daily backup");

// 恢复到特定时间点
timeline.RestoreEntry(entryId, "C:\\Restore");

// 清理旧条目
timeline.PruneOldEntries(30, 90);
```

### 3. 加密增强 (EncryptionEnhancer)

```cpp
EncryptionEnhancer::EncryptionConfig config;
config.algorithm = EncryptionEnhancer::Algorithm::AES256;
config.kdf = EncryptionEnhancer::KeyDerivation::PBKDF2;
config.iterations = 100000;

EncryptionEnhancer enhancer(config);
enhancer.EncryptArchive("archive.7z", config);
```

### 4. 智能分类 (IntelligentClassifier)

```cpp
IntelligentClassifier classifier(archive);

// 分类文件
auto result = classifier.ClassifyFile("document.pdf");
// result.type = FileType::Document
// result.confidence = 0.7

// 分类整个压缩包
auto archiveClass = classifier.ClassifyArchive("archive.7z");
```

### 5. 病毒扫描 (VirusScannerInterface)

```cpp
VirusScannerInterface scanner(archive);

VirusScannerInterface::ScanOptions opts;
opts.scanArchives = true;
opts.heuristicsEnabled = true;

auto report = scanner.ScanArchive("archive.7z", opts);
if (report.overallResult == ScanResult::Infected) {
    // 处理威胁
}
```

### 6. 压缩包转换 (ArchiveConverter)

```cpp
ArchiveConverter converter(archive);

ArchiveConverter::ConversionOptions opts;
opts.targetFormat = ArchiveFormat::FMT_ZIP;

auto result = converter.ConvertArchive("source.7z", "target.zip", opts);
```

### 7. 压缩包验证 (ArchiveValidator)

```cpp
ArchiveValidator validator(archive);

ArchiveValidator::ValidationOptions opts;
opts.checkCRC = true;
opts.deepScan = true;

auto result = validator.ValidateArchive("archive.7z", opts);

// 生成校验和
std::string sha256 = validator.GenerateChecksum("archive.7z", "sha256");
```

### 8. 密码管理 (PasswordManager)

```cpp
PasswordManager pwdMgr;

// 生成密码
PasswordManager::PasswordPolicy policy;
policy.minLength = 12;
policy.requireUppercase = true;
std::string password = pwdMgr.GeneratePassword(16, policy);

// 存储密码
pwdMgr.AddPassword("archive.7z", "mypassword");

// 导出/导入
pwdMgr.ExportPasswords("passwords.dat", "master_key");
```

### 9. 差异备份 (ArchiveDiffer)

```cpp
ArchiveDiffer differ(archive);

// 比较两个压缩包
auto diff = differ.CompareArchives("v1.7z", "v2.7z");

// 创建增量包
differ.CreateDeltaArchive("base.7z", "new.7z", "delta.7z", opts);

// 应用增量包
differ.ApplyDeltaArchive("base.7z", "delta.7z", "restored.7z");
```

### 10. 数据去重 (DeduplicationEngine)

```cpp
DeduplicationEngine engine;

// 创建去重存储
engine.CreateStore("dedup_store");

// 去重存储文件
auto result = engine.StoreFile("large_file.dat", "dedup_store");
```

### 11. 云存储 (CloudStorageClient)

```cpp
CloudStorageClient cloud;

// 上传
cloud.UploadArchive("archive.7z", "https://cloud.example.com/upload");

// 下载
cloud.DownloadArchive("https://cloud.example.com/archive.7z", "local.7z");
```

---

## 📁 示例代码

完整的示例代码位于 `examples/` 目录：

| 文件 | 描述 |
|------|------|
| `example_basic.cpp` | 基础压缩/解压 |
| `example_backup.cpp` | 备份管理 |
| `example_encryption.cpp` | 加密功能 |
| `example_progress.cpp` | 进度回调 |
| `example_split.cpp` | 分卷压缩 |
| `example_classify.cpp` | 智能分类 |
| `example_scan.cpp` | 病毒扫描 |
| `example_convert.cpp` | 格式转换 |
| `example_validate.cpp` | 完整性验证 |
| `example_timeline.cpp` | 时间线备份 |

---

## ⚙️ 配置文件

SDK 支持 JSON 格式的配置文件：

```json
{
    "compression": {
        "method": "LZMA2",
        "level": "Maximum",
        "solidMode": true,
        "threadCount": 4
    },
    "encryption": {
        "algorithm": "AES256",
        "kdf": "PBKDF2",
        "iterations": 100000
    },
    "backup": {
        "type": "Incremental",
        "preservePermissions": true,
        "excludePatterns": ["*.tmp", "*.log", "*.bak"]
    },
    "cloud": {
        "endpoint": "https://cloud.example.com",
        "timeout": 30000
    }
}
```

使用方式：

```cpp
SDKConfig config;
config.LoadFromFile("config.json");

SevenZipArchive archive(config.GetCompressionOptions());
```

---

## 📊 性能参考

| 操作 | 文件数 | 原始大小 | 压缩后 | 时间 |
|------|--------|---------|--------|------|
| 压缩 (LZMA2) | 10,000 | 1 GB | 350 MB | ~45s |
| 解压 | 10,000 | 350 MB | 1 GB | ~15s |
| 分卷 (100MB) | 10,000 | 1 GB | 350 MB | ~50s |
| 加密压缩 | 10,000 | 1 GB | 350 MB | ~60s |

*测试环境: Intel i7-10700, 32GB RAM, NVMe SSD*

---

## 📜 许可证

本项目基于 LGPL 许可证发布，包含 7-Zip SDK 代码。

7-Zip SDK 使用 LGPL 和 unRAR 许可证，详见 `sdk/DOC/` 目录。

---

## 🙏 致谢

- [7-Zip](https://www.7-zip.org/) - Igor Pavlov
- LZMA SDK - 优秀的压缩算法实现

---

## 📮 联系方式

如有问题或建议，请提交 Issue。

---

*SevenZip SDK - 企业级压缩包管理解决方案*
