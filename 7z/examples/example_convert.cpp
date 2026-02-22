/*
 * SevenZip SDK - 格式转换示例
 * 
 * 本示例演示:
 * - 压缩包格式转换
 * - 批量转换
 * - 转换选项配置
 */

#include "../7zsdk.cpp"
#include <iostream>

using namespace SevenZip;

void PrintSeparator(const std::string& title) {
    std::cout << "\n========== " << title << " ==========\n" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "   SevenZip SDK - 格式转换示例" << std::endl;
    std::cout << "========================================" << std::endl;
    
    SevenZipArchive archive;
    ArchiveConverter converter(archive);
    
    // ========================================
    // 1. 基础格式转换
    // ========================================
    PrintSeparator("1. 基础格式转换");
    
    ArchiveConverter::ConversionOptions opts;
    opts.targetFormat = ArchiveFormat::FMT_7Z;
    opts.method = CompressionMethod::LZMA2;
    opts.level = CompressionLevel::Normal;
    opts.preserveTimestamps = true;
    
    std::cout << "转换选项:" << std::endl;
    std::cout << "  目标格式: 7z" << std::endl;
    std::cout << "  压缩方法: LZMA2" << std::endl;
    std::cout << "  压缩级别: Normal" << std::endl;
    std::cout << "  保留时间戳: 是" << std::endl;
    
    // auto result = converter.ConvertArchive("source.zip", "target.7z", opts);
    
    std::cout << "\n示例代码:" << std::endl;
    std::cout << "  auto result = converter.ConvertArchive(\"source.zip\", \"target.7z\", opts);" << std::endl;
    std::cout << "  if (result.success) {" << std::endl;
    std::cout << "      std::cout << \"转换成功\" << std::endl;" << std::endl;
    std::cout << "      std::cout << \"原始大小: \" << result.originalSize << std::endl;" << std::endl;
    std::cout << "      std::cout << \"转换后大小: \" << result.convertedSize << std::endl;" << std::endl;
    std::cout << "  }" << std::endl;
    
    // ========================================
    // 2. 快捷转换方法
    // ========================================
    PrintSeparator("2. 快捷转换方法");
    
    std::cout << "转换为 7z:" << std::endl;
    std::cout << "  auto result = converter.ConvertTo7z(\"source.zip\", \"target.7z\", CompressionLevel::Maximum);" << std::endl;
    
    std::cout << "\n转换为 ZIP:" << std::endl;
    std::cout << "  auto result = converter.ConvertToZip(\"source.7z\", \"target.zip\", CompressionLevel::Normal);" << std::endl;
    
    // ========================================
    // 3. 支持的格式转换
    // ========================================
    PrintSeparator("3. 支持的格式转换");
    
    std::cout << "源格式 -> 目标格式" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "  7z    -> ZIP, TAR, GZIP, BZIP2, XZ" << std::endl;
    std::cout << "  ZIP   -> 7z, TAR, GZIP, BZIP2, XZ" << std::endl;
    std::cout << "  TAR   -> 7z, ZIP, GZIP, BZIP2, XZ" << std::endl;
    std::cout << "  RAR   -> 7z, ZIP, TAR (只读源)" << std::endl;
    
    // ========================================
    // 4. 批量转换
    // ========================================
    PrintSeparator("4. 批量转换");
    
    std::vector<std::string> sources = {
        "file1.zip",
        "file2.zip",
        "file3.zip"
    };
    
    std::cout << "批量转换文件列表:" << std::endl;
    for (const auto& file : sources) {
        std::cout << "  - " << file << std::endl;
    }
    
    std::cout << "\n示例代码:" << std::endl;
    std::cout << "  std::vector<std::string> sources = {\"file1.zip\", \"file2.zip\"};" << std::endl;
    std::cout << std::endl;
    std::cout << "  converter.BatchConvert(sources, \"output_dir\", opts, [](const auto& path, const auto& result) {" << std::endl;
    std::cout << "      if (result.success) {" << std::endl;
    std::cout << "          std::cout << \"转换成功: \" << path << std::endl;" << std::endl;
    std::cout << "      } else {" << std::endl;
    std::cout << "          std::cout << \"转换失败: \" << result.errorMessage << std::endl;" << std::endl;
    std::cout << "      }" << std::endl;
    std::cout << "  });" << std::endl;
    
    // ========================================
    // 5. 转换时更改密码
    // ========================================
    PrintSeparator("5. 转换时更改密码");
    
    ArchiveConverter::ConversionOptions pwdOpts;
    pwdOpts.password = "old_password";      // 原密码
    pwdOpts.newPassword = "new_password";   // 新密码
    
    std::cout << "原密码: old_password" << std::endl;
    std::cout << "新密码: new_password" << std::endl;
    
    // converter.ConvertArchive("encrypted.zip", "encrypted.7z", pwdOpts);
    
    std::cout << "\n示例代码:" << std::endl;
    std::cout << "  ConversionOptions opts;" << std::endl;
    std::cout << "  opts.password = \"old_password\";" << std::endl;
    std::cout << "  opts.newPassword = \"new_password\";" << std::endl;
    std::cout << "  converter.ConvertArchive(\"source.zip\", \"target.7z\", opts);" << std::endl;
    
    // ========================================
    // 6. 转换结果
    // ========================================
    PrintSeparator("6. 转换结果");
    
    std::cout << "ConversionResult 结构:" << std::endl;
    std::cout << "  - success: 是否成功" << std::endl;
    std::cout << "  - originalSize: 原始大小" << std::endl;
    std::cout << "  - convertedSize: 转换后大小" << std::endl;
    std::cout << "  - filesConverted: 转换文件数" << std::endl;
    std::cout << "  - errorMessage: 错误信息" << std::endl;
    
    // ========================================
    // 7. 格式特性对比
    // ========================================
    PrintSeparator("7. 格式特性对比");
    
    std::cout << "格式      压缩率  速度   加密   固实   Unicode" << std::endl;
    std::cout << "------------------------------------------------" << std::endl;
    std::cout << "7z        最高    中等   AES    是     是" << std::endl;
    std::cout << "ZIP       中等    快     AES    否     是" << std::endl;
    std::cout << "TAR       无      最快   否     否     是" << std::endl;
    std::cout << "GZIP      中等    快     否     否     是" << std::endl;
    std::cout << "BZIP2     高      慢     否     否     是" << std::endl;
    std::cout << "XZ        最高    慢     否     否     是" << std::endl;
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "   格式转换示例完成" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
