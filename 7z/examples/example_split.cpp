/*
 * SevenZip SDK - 分卷压缩示例
 * 
 * 本示例演示:
 * - 创建分卷压缩包
 * - 自定义分卷大小
 * - 解压分卷压缩包
 * - 合并分卷
 */

#include "../7zsdk.cpp"
#include <iostream>

using namespace SevenZip;

void PrintSeparator(const std::string& title) {
    std::cout << "\n========== " << title << " ==========\n" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "   SevenZip SDK - 分卷压缩示例" << std::endl;
    std::cout << "========================================" << std::endl;
    
    SevenZipArchive archive;
    
    // ========================================
    // 1. 创建分卷压缩包
    // ========================================
    PrintSeparator("1. 创建分卷压缩包");
    
    CompressionOptions opts;
    opts.method = CompressionMethod::LZMA2;
    opts.level = CompressionLevel::Normal;
    
    // 分卷大小: 100 MB
    uint64_t volumeSize = 100 * 1024 * 1024;
    
    std::cout << "分卷大小: 100 MB" << std::endl;
    std::cout << "输出文件: backup.7z.001, backup.7z.002, ..." << std::endl;
    
    // archive.CreateSplitArchive("backup.7z", "C:\\LargeData", volumeSize, opts);
    
    std::cout << "\n示例代码:" << std::endl;
    std::cout << "  uint64_t volumeSize = 100 * 1024 * 1024;  // 100 MB" << std::endl;
    std::cout << "  archive.CreateSplitArchive(\"backup.7z\", \"C:\\\\LargeData\", volumeSize, opts);" << std::endl;
    
    // ========================================
    // 2. 常用分卷大小
    // ========================================
    PrintSeparator("2. 常用分卷大小");
    
    std::cout << "存储介质           分卷大小" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "CD-ROM (700 MB)    " << (700 * 1024 * 1024) << " bytes" << std::endl;
    std::cout << "DVD (4.7 GB)       " << (4700 * 1024 * 1024ULL) << " bytes" << std::endl;
    std::cout << "DVD-DL (8.5 GB)    " << (8500 * 1024 * 1024ULL) << " bytes" << std::endl;
    std::cout << "BD-R (25 GB)       " << (25 * 1024 * 1024 * 1024ULL) << " bytes" << std::endl;
    std::cout << "FAT32 (最大 4GB-1) " << (0xFFFFFFFF) << " bytes" << std::endl;
    std::cout << "电子邮件 (25 MB)   " << (25 * 1024 * 1024) << " bytes" << std::endl;
    std::cout << "USB (1 GB)         " << (1024 * 1024 * 1024ULL) << " bytes" << std::endl;
    
    // ========================================
    // 3. 解压分卷压缩包
    // ========================================
    PrintSeparator("3. 解压分卷压缩包");
    
    std::cout << "解压分卷压缩包只需要指定第一个分卷文件:" << std::endl;
    std::cout << "  backup.7z.001 (第一个分卷)" << std::endl;
    std::cout << "  backup.7z.002" << std::endl;
    std::cout << "  backup.7z.003" << std::endl;
    std::cout << "  ..." << std::endl;
    
    ExtractOptions extractOpts;
    extractOpts.outputDir = "restored_data";
    
    // archive.ExtractSplitArchive("backup.7z.001", "restored_data");
    
    std::cout << "\n示例代码:" << std::endl;
    std::cout << "  archive.ExtractSplitArchive(\"backup.7z.001\", \"C:\\\\Restore\");" << std::endl;
    
    // ========================================
    // 4. 合并分卷
    // ========================================
    PrintSeparator("4. 合并分卷为单个文件");
    
    std::cout << "将分卷合并为单个压缩包:" << std::endl;
    
    // archive.MergeSplitArchive("backup.7z.001", "backup_complete.7z");
    
    std::cout << "示例代码:" << std::endl;
    std::cout << "  archive.MergeSplitArchive(\"backup.7z.001\", \"backup_complete.7z\");" << std::endl;
    
    std::cout << "\n注意: 合并后可以删除分卷文件" << std::endl;
    
    // ========================================
    // 5. 分卷压缩 + 加密
    // ========================================
    PrintSeparator("5. 分卷压缩 + 加密");
    
    CompressionOptions encSplitOpts;
    encSplitOpts.method = CompressionMethod::LZMA2;
    encSplitOpts.level = CompressionLevel::Maximum;
    encSplitOpts.password = "MySecretPassword";
    encSplitOpts.encryptHeaders = true;
    
    std::cout << "加密分卷压缩选项:" << std::endl;
    std::cout << "  密码: MySecretPassword" << std::endl;
    std::cout << "  加密文件头: 是" << std::endl;
    std::cout << "  分卷大小: 100 MB" << std::endl;
    
    // archive.CreateSplitArchive("encrypted_backup.7z", "C:\\SecretData", volumeSize, encSplitOpts);
    
    // ========================================
    // 6. 获取分卷信息
    // ========================================
    PrintSeparator("6. 获取分卷信息");
    
    // auto splitInfo = archive.GetSplitArchiveInfo("backup.7z.001");
    
    std::cout << "可获取的分卷信息:" << std::endl;
    std::cout << "  - 分卷数量" << std::endl;
    std::cout << "  - 每个分卷的大小" << std::endl;
    std::cout << "  - 总大小" << std::endl;
    std::cout << "  - 是否完整 (所有分卷都存在)" << std::endl;
    
    std::cout << "\n示例代码:" << std::endl;
    std::cout << "  auto info = archive.GetSplitArchiveInfo(\"backup.7z.001\");" << std::endl;
    std::cout << "  std::cout << \"分卷数: \" << info.volumeCount << std::endl;" << std::endl;
    std::cout << "  std::cout << \"总大小: \" << info.totalSize << std::endl;" << std::endl;
    
    // ========================================
    // 7. 验证分卷完整性
    // ========================================
    PrintSeparator("7. 验证分卷完整性");
    
    std::cout << "验证所有分卷是否完整:" << std::endl;
    
    // bool isComplete = archive.VerifySplitArchive("backup.7z.001");
    
    std::cout << "示例代码:" << std::endl;
    std::cout << "  bool isComplete = archive.VerifySplitArchive(\"backup.7z.001\");" << std::endl;
    std::cout << "  if (isComplete) {" << std::endl;
    std::cout << "      std::cout << \"所有分卷完整\" << std::endl;" << std::endl;
    std::cout << "  } else {" << std::endl;
    std::cout << "      std::cout << \"缺少分卷文件\" << std::endl;" << std::endl;
    std::cout << "  }" << std::endl;
    
    // ========================================
    // 8. 自动分卷大小计算
    // ========================================
    PrintSeparator("8. 自动计算分卷大小");
    
    std::cout << "根据目标介质自动计算:" << std::endl;
    std::cout << std::endl;
    std::cout << "  // 计算适合 DVD 的分卷大小" << std::endl;
    std::cout << "  uint64_t dvdSize = 4700 * 1024 * 1024ULL;" << std::endl;
    std::cout << "  // 预留 5% 空间给文件系统开销" << std::endl;
    std::cout << "  uint64_t safeSize = (dvdSize * 95) / 100;" << std::endl;
    std::cout << std::endl;
    std::cout << "  archive.CreateSplitArchive(\"backup.7z\", \"C:\\\\Data\", safeSize, opts);" << std::endl;
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "   分卷压缩示例完成" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
