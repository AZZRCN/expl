/*
 * SevenZip SDK - 完整性验证示例
 * 
 * 本示例演示:
 * - 验证压缩包完整性
 * - 生成校验和
 * - CRC/MD5/SHA256 校验
 */

#include "../7zsdk.cpp"
#include <iostream>

using namespace SevenZip;

void PrintSeparator(const std::string& title) {
    std::cout << "\n========== " << title << " ==========\n" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "   SevenZip SDK - 完整性验证示例" << std::endl;
    std::cout << "========================================" << std::endl;
    
    SevenZipArchive archive;
    ArchiveValidator validator(archive);
    
    // ========================================
    // 1. 基础验证
    // ========================================
    PrintSeparator("1. 基础验证");
    
    ArchiveValidator::ValidationOptions opts;
    opts.checkCRC = true;
    opts.checkHeaders = true;
    opts.extractTest = false;
    opts.deepScan = true;
    
    std::cout << "验证选项:" << std::endl;
    std::cout << "  CRC 校验: 是" << std::endl;
    std::cout << "  头部检查: 是" << std::endl;
    std::cout << "  提取测试: 否" << std::endl;
    std::cout << "  深度扫描: 是" << std::endl;
    
    // auto result = validator.ValidateArchive("archive.7z", opts);
    
    std::cout << "\n示例代码:" << std::endl;
    std::cout << "  auto result = validator.ValidateArchive(\"archive.7z\", opts);" << std::endl;
    std::cout << "  if (result.isValid) {" << std::endl;
    std::cout << "      std::cout << \"压缩包完整\" << std::endl;" << std::endl;
    std::cout << "  } else {" << std::endl;
    std::cout << "      for (const auto& error : result.errors) {" << std::endl;
    std::cout << "          std::cout << \"错误: \" << error << std::endl;" << std::endl;
    std::cout << "      }" << std::endl;
    std::cout << "  }" << std::endl;
    
    // ========================================
    // 2. 快速验证
    // ========================================
    PrintSeparator("2. 快速验证");
    
    std::cout << "只检查头部签名，不验证数据:" << std::endl;
    
    // bool isValid = validator.QuickValidate("archive.7z");
    
    std::cout << "\n示例代码:" << std::endl;
    std::cout << "  bool isValid = validator.QuickValidate(\"archive.7z\");" << std::endl;
    
    // ========================================
    // 3. 验证结果
    // ========================================
    PrintSeparator("3. 验证结果");
    
    std::cout << "ValidationResult 结构:" << std::endl;
    std::cout << "  - isValid: 总体是否有效" << std::endl;
    std::cout << "  - headersValid: 头部是否有效" << std::endl;
    std::cout << "  - dataValid: 数据是否有效" << std::endl;
    std::cout << "  - checksumsValid: 校验和是否有效" << std::endl;
    std::cout << "  - corruptedFiles: 损坏文件数" << std::endl;
    std::cout << "  - corruptedBytes: 损坏字节数" << std::endl;
    std::cout << "  - errors: 错误列表" << std::endl;
    std::cout << "  - warnings: 警告列表" << std::endl;
    
    // ========================================
    // 4. 验证单个文件
    // ========================================
    PrintSeparator("4. 验证单个文件");
    
    // bool valid = validator.ValidateFile("archive.7z", "important.doc");
    
    std::cout << "示例代码:" << std::endl;
    std::cout << "  bool valid = validator.ValidateFile(\"archive.7z\", \"important.doc\");" << std::endl;
    std::cout << "  if (valid) {" << std::endl;
    std::cout << "      std::cout << \"文件完整\" << std::endl;" << std::endl;
    std::cout << "  }" << std::endl;
    
    // ========================================
    // 5. 生成校验和
    // ========================================
    PrintSeparator("5. 生成校验和");
    
    std::cout << "支持的算法:" << std::endl;
    std::cout << "  - CRC32: 32位循环冗余校验" << std::endl;
    std::cout << "  - MD5: 128位哈希" << std::endl;
    std::cout << "  - SHA256: 256位安全哈希" << std::endl;
    
    std::cout << "\n示例代码:" << std::endl;
    std::cout << "  std::string crc32 = validator.GenerateChecksum(\"file.7z\", \"crc32\");" << std::endl;
    std::cout << "  std::string md5 = validator.GenerateChecksum(\"file.7z\", \"md5\");" << std::endl;
    std::cout << "  std::string sha256 = validator.GenerateChecksum(\"file.7z\", \"sha256\");" << std::endl;
    
    std::cout << "\n输出示例:" << std::endl;
    std::cout << "  CRC32:  A1B2C3D4" << std::endl;
    std::cout << "  MD5:    d41d8cd98f00b204e9800998ecf8427e" << std::endl;
    std::cout << "  SHA256: e3b0c44298fc1c149afbf4c8996fb924..." << std::endl;
    
    // ========================================
    // 6. 验证校验和
    // ========================================
    PrintSeparator("6. 验证校验和");
    
    std::cout << "验证文件是否匹配预期的校验和:" << std::endl;
    
    // bool match = validator.VerifyChecksum("archive.7z", "expected_sha256_value", "sha256");
    
    std::cout << "\n示例代码:" << std::endl;
    std::cout << "  bool match = validator.VerifyChecksum(" << std::endl;
    std::cout << "      \"archive.7z\"," << std::endl;
    std::cout << "      \"e3b0c44298fc1c149afbf4c8996fb924...\"," << std::endl;
    std::cout << "      \"sha256\"" << std::endl;
    std::cout << "  );" << std::endl;
    
    // ========================================
    // 7. 提取测试
    // ========================================
    PrintSeparator("7. 提取测试");
    
    ArchiveValidator::ValidationOptions testOpts;
    testOpts.extractTest = true;  // 实际提取文件进行测试
    
    std::cout << "提取测试会:" << std::endl;
    std::cout << "  1. 将文件提取到临时目录" << std::endl;
    std::cout << "  2. 验证提取是否成功" << std::endl;
    std::cout << "  3. 删除临时文件" << std::endl;
    
    std::cout << "\n注意: 提取测试耗时较长，但能确保文件可以正确解压" << std::endl;
    
    // ========================================
    // 8. 损坏检测
    // ========================================
    PrintSeparator("8. 损坏检测");
    
    std::cout << "可检测的损坏类型:" << std::endl;
    std::cout << "  - 头部损坏: 签名不匹配" << std::endl;
    std::cout << "  - 数据损坏: CRC 校验失败" << std::endl;
    std::cout << "  - 截断文件: 文件大小不匹配" << std::endl;
    std::cout << "  - 密码错误: 无法解密" << std::endl;
    std::cout << "  - 格式错误: 无法解析结构" << std::endl;
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "   完整性验证示例完成" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
