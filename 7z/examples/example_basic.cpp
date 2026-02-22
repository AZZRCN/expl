/*
 * SevenZip SDK - 基础示例
 * 
 * 本示例演示:
 * - 压缩目录
 * - 压缩指定文件
 * - 解压压缩包
 * - 列出压缩包内容
 * - 测试压缩包完整性
 */

#include "../7zsdk.cpp"
#include <iostream>
#include <iomanip>

using namespace SevenZip;

void PrintSeparator(const std::string& title) {
    std::cout << "\n========== " << title << " ==========\n" << std::endl;
}

void PrintFileInfo(const FileInfo& file) {
    std::cout << "  " << std::left << std::setw(40) << file.path;
    
    if (file.isDirectory) {
        std::cout << " [DIR]";
    } else {
        std::cout << " " << std::right << std::setw(12) << file.size << " bytes";
        std::cout << " -> " << std::setw(10) << file.packedSize << " bytes";
        std::cout << " CRC:" << std::hex << file.crc << std::dec;
    }
    
    if (file.isEncrypted) {
        std::cout << " [ENCRYPTED]";
    }
    
    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "========================================" << std::endl;
    std::cout << "   SevenZip SDK - 基础示例" << std::endl;
    std::cout << "========================================" << std::endl;
    
    SevenZipArchive archive;
    
    // ========================================
    // 1. 压缩目录
    // ========================================
    PrintSeparator("1. 压缩目录");
    
    CompressionOptions compOpts;
    compOpts.method = CompressionMethod::LZMA2;
    compOpts.level = CompressionLevel::Normal;
    compOpts.solidMode = true;
    compOpts.threadCount = 4;
    
    std::cout << "压缩方法: LZMA2" << std::endl;
    std::cout << "压缩级别: Normal" << std::endl;
    std::cout << "固实模式: 启用" << std::endl;
    std::cout << "线程数: 4" << std::endl;
    
    // 注意: 需要实际存在的目录
    // bool success = archive.CompressDirectory("output.7z", "C:\\Data", compOpts, true);
    std::cout << "\n示例代码: archive.CompressDirectory(\"output.7z\", \"C:\\\\Data\", compOpts, true);" << std::endl;
    
    // ========================================
    // 2. 压缩指定文件
    // ========================================
    PrintSeparator("2. 压缩指定文件");
    
    std::vector<std::string> filesToCompress = {
        "file1.txt",
        "file2.txt",
        "subdir/file3.txt"
    };
    
    std::cout << "要压缩的文件:" << std::endl;
    for (const auto& file : filesToCompress) {
        std::cout << "  - " << file << std::endl;
    }
    
    // bool success = archive.CompressFiles("files.7z", filesToCompress, compOpts);
    std::cout << "\n示例代码: archive.CompressFiles(\"files.7z\", filesToCompress, compOpts);" << std::endl;
    
    // ========================================
    // 3. 解压压缩包
    // ========================================
    PrintSeparator("3. 解压压缩包");
    
    ExtractOptions extractOpts;
    extractOpts.outputDir = "output";
    extractOpts.overwriteExisting = true;
    extractOpts.preserveDirectoryStructure = true;
    extractOpts.preserveFileTime = true;
    
    std::cout << "输出目录: " << extractOpts.outputDir << std::endl;
    std::cout << "覆盖已存在: " << (extractOpts.overwriteExisting ? "是" : "否") << std::endl;
    std::cout << "保留目录结构: " << (extractOpts.preserveDirectoryStructure ? "是" : "否") << std::endl;
    
    // bool success = archive.ExtractArchive("archive.7z", extractOpts);
    std::cout << "\n示例代码: archive.ExtractArchive(\"archive.7z\", extractOpts);" << std::endl;
    
    // ========================================
    // 4. 列出压缩包内容
    // ========================================
    PrintSeparator("4. 列出压缩包内容");
    
    ArchiveInfo info;
    // archive.ListArchive("archive.7z", info);
    
    std::cout << "压缩包信息示例:" << std::endl;
    std::cout << "  文件数: " << info.fileCount << std::endl;
    std::cout << "  目录数: " << info.directoryCount << std::endl;
    std::cout << "  原始大小: " << info.uncompressedSize << " bytes" << std::endl;
    std::cout << "  压缩大小: " << info.compressedSize << " bytes" << std::endl;
    std::cout << "  是否加密: " << (info.isEncrypted ? "是" : "否") << std::endl;
    std::cout << "  是否固实: " << (info.isSolid ? "是" : "否") << std::endl;
    
    std::cout << "\n文件列表:" << std::endl;
    for (const auto& file : info.files) {
        PrintFileInfo(file);
    }
    
    // ========================================
    // 5. 测试压缩包完整性
    // ========================================
    PrintSeparator("5. 测试压缩包完整性");
    
    // bool isValid = archive.TestArchive("archive.7z");
    std::cout << "示例代码: bool isValid = archive.TestArchive(\"archive.7z\");" << std::endl;
    std::cout << "返回 true 表示压缩包完整无损" << std::endl;
    
    // ========================================
    // 6. 提取单个文件到内存
    // ========================================
    PrintSeparator("6. 提取单个文件到内存");
    
    std::vector<uint8_t> fileData;
    // bool success = archive.ExtractSingleFileToMemory("archive.7z", "readme.txt", fileData);
    
    std::cout << "示例代码:" << std::endl;
    std::cout << "  std::vector<uint8_t> fileData;" << std::endl;
    std::cout << "  archive.ExtractSingleFileToMemory(\"archive.7z\", \"readme.txt\", fileData);" << std::endl;
    std::cout << "  // fileData 现在包含文件内容" << std::endl;
    
    // ========================================
    // 7. 添加文件到已有压缩包
    // ========================================
    PrintSeparator("7. 添加文件到已有压缩包");
    
    std::vector<std::string> newFiles = {"newfile.txt", "another.txt"};
    // bool success = archive.AddToArchive("existing.7z", newFiles, compOpts);
    
    std::cout << "示例代码:" << std::endl;
    std::cout << "  std::vector<std::string> newFiles = {\"newfile.txt\"};" << std::endl;
    std::cout << "  archive.AddToArchive(\"existing.7z\", newFiles, compOpts);" << std::endl;
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "   基础示例完成" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
