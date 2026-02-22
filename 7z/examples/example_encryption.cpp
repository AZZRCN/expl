/*
 * SevenZip SDK - 加密功能示例
 * 
 * 本示例演示:
 * - 创建加密压缩包
 * - 解压加密压缩包
 * - 使用加密增强器
 * - 密码管理
 */

#include "../7zsdk.cpp"
#include <iostream>

using namespace SevenZip;

void PrintSeparator(const std::string& title) {
    std::cout << "\n========== " << title << " ==========\n" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "   SevenZip SDK - 加密功能示例" << std::endl;
    std::cout << "========================================" << std::endl;
    
    SevenZipArchive archive;
    
    // ========================================
    // 1. 基础加密压缩
    // ========================================
    PrintSeparator("1. 基础加密压缩");
    
    CompressionOptions encOpts;
    encOpts.method = CompressionMethod::LZMA2;
    encOpts.level = CompressionLevel::Maximum;
    encOpts.password = "MySecretPassword123";
    encOpts.encryptHeaders = true;  // 加密文件名
    
    std::cout << "密码: MySecretPassword123" << std::endl;
    std::cout << "加密文件头: 是 (文件名也被加密)" << std::endl;
    
    // archive.CompressDirectory("encrypted.7z", "C:\\SecretData", encOpts, true);
    
    std::cout << "\n示例代码:" << std::endl;
    std::cout << "  CompressionOptions encOpts;" << std::endl;
    std::cout << "  encOpts.password = \"MySecretPassword123\";" << std::endl;
    std::cout << "  encOpts.encryptHeaders = true;" << std::endl;
    std::cout << "  archive.CompressDirectory(\"encrypted.7z\", \"C:\\\\SecretData\", encOpts, true);" << std::endl;
    
    // ========================================
    // 2. 解压加密压缩包
    // ========================================
    PrintSeparator("2. 解压加密压缩包");
    
    ExtractOptions decOpts;
    decOpts.outputDir = "decrypted_output";
    decOpts.password = "MySecretPassword123";
    decOpts.overwriteExisting = true;
    
    std::cout << "解压密码: MySecretPassword123" << std::endl;
    
    // archive.ExtractArchive("encrypted.7z", decOpts);
    
    std::cout << "\n示例代码:" << std::endl;
    std::cout << "  ExtractOptions decOpts;" << std::endl;
    std::cout << "  decOpts.password = \"MySecretPassword123\";" << std::endl;
    std::cout << "  archive.ExtractArchive(\"encrypted.7z\", decOpts);" << std::endl;
    
    // ========================================
    // 3. 使用加密增强器
    // ========================================
    PrintSeparator("3. 使用加密增强器");
    
    EncryptionEnhancer::EncryptionConfig encConfig;
    encConfig.algorithm = EncryptionEnhancer::Algorithm::AES256;
    encConfig.kdf = EncryptionEnhancer::KeyDerivation::PBKDF2;
    encConfig.iterations = 100000;
    encConfig.encryptMetadata = true;
    
    std::cout << "加密算法: AES-256" << std::endl;
    std::cout << "密钥派生: PBKDF2" << std::endl;
    std::cout << "迭代次数: 100,000" << std::endl;
    std::cout << "加密元数据: 是" << std::endl;
    
    EncryptionEnhancer enhancer(encConfig);
    
    // enhancer.EncryptArchive("plain.7z", encConfig);
    
    std::cout << "\n示例代码:" << std::endl;
    std::cout << "  EncryptionEnhancer::EncryptionConfig encConfig;" << std::endl;
    std::cout << "  encConfig.algorithm = EncryptionEnhancer::Algorithm::AES256;" << std::endl;
    std::cout << "  encConfig.kdf = EncryptionEnhancer::KeyDerivation::PBKDF2;" << std::endl;
    std::cout << "  encConfig.iterations = 100000;" << std::endl;
    std::cout << "  EncryptionEnhancer enhancer(encConfig);" << std::endl;
    
    // ========================================
    // 4. 分析加密信息
    // ========================================
    PrintSeparator("4. 分析压缩包加密信息");
    
    // auto encInfo = enhancer.AnalyzeEncryption("encrypted.7z");
    
    std::cout << "可获取的信息:" << std::endl;
    std::cout << "  - 是否加密" << std::endl;
    std::cout << "  - 加密算法" << std::endl;
    std::cout << "  - 密钥派生方法" << std::endl;
    std::cout << "  - 迭代次数" << std::endl;
    std::cout << "  - 文件头是否加密" << std::endl;
    
    std::cout << "\n示例代码:" << std::endl;
    std::cout << "  auto encInfo = enhancer.AnalyzeEncryption(\"encrypted.7z\");" << std::endl;
    std::cout << "  if (encInfo.isEncrypted) {" << std::endl;
    std::cout << "      std::cout << \"算法: \" << encInfo.algorithm << std::endl;" << std::endl;
    std::cout << "  }" << std::endl;
    
    // ========================================
    // 5. 密码管理器
    // ========================================
    PrintSeparator("5. 密码管理器");
    
    PasswordManager pwdMgr;
    
    // 设置密码策略
    PasswordManager::PasswordPolicy policy;
    policy.minLength = 12;
    policy.requireUppercase = true;
    policy.requireLowercase = true;
    policy.requireNumbers = true;
    policy.requireSymbols = true;
    
    std::cout << "密码策略:" << std::endl;
    std::cout << "  最小长度: " << policy.minLength << std::endl;
    std::cout << "  需要大写字母: 是" << std::endl;
    std::cout << "  需要小写字母: 是" << std::endl;
    std::cout << "  需要数字: 是" << std::endl;
    std::cout << "  需要符号: 是" << std::endl;
    
    // 生成密码
    std::string generatedPassword = pwdMgr.GeneratePassword(16, policy);
    std::cout << "\n生成的密码: " << generatedPassword << std::endl;
    
    // 验证密码
    bool isValid = pwdMgr.ValidatePassword(generatedPassword, policy);
    std::cout << "密码验证: " << (isValid ? "通过" : "失败") << std::endl;
    
    // ========================================
    // 6. 存储和检索密码
    // ========================================
    PrintSeparator("6. 存储和检索密码");
    
    // 存储密码
    // pwdMgr.AddPassword("important_archive.7z", "SuperSecretPassword!");
    
    std::cout << "存储密码示例:" << std::endl;
    std::cout << "  pwdMgr.AddPassword(\"archive.7z\", \"password123\");" << std::endl;
    
    // 检索密码
    // std::string storedPassword = pwdMgr.GetPassword("important_archive.7z");
    
    std::cout << "\n检索密码示例:" << std::endl;
    std::cout << "  std::string pwd = pwdMgr.GetPassword(\"archive.7z\");" << std::endl;
    
    // ========================================
    // 7. 导出/导入密码库
    // ========================================
    PrintSeparator("7. 导出/导入密码库");
    
    std::cout << "导出密码库:" << std::endl;
    std::cout << "  pwdMgr.ExportPasswords(\"passwords.dat\", \"master_key\");" << std::endl;
    
    std::cout << "\n导入密码库:" << std::endl;
    std::cout << "  pwdMgr.ImportPasswords(\"passwords.dat\", \"master_key\");" << std::endl;
    
    std::cout << "\n注意: 密码库使用主密钥加密存储" << std::endl;
    
    // ========================================
    // 8. 支持的加密算法
    // ========================================
    PrintSeparator("8. 支持的加密算法");
    
    std::cout << "加密算法:" << std::endl;
    std::cout << "  - AES-256 (推荐)" << std::endl;
    std::cout << "  - ChaCha20" << std::endl;
    std::cout << "  - Twofish" << std::endl;
    std::cout << "  - Serpent" << std::endl;
    std::cout << "  - Camellia" << std::endl;
    
    std::cout << "\n密钥派生函数:" << std::endl;
    std::cout << "  - PBKDF2 (兼容性最好)" << std::endl;
    std::cout << "  - Argon2 (推荐，抗GPU攻击)" << std::endl;
    std::cout << "  - Scrypt" << std::endl;
    std::cout << "  - BCrypt" << std::endl;
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "   加密功能示例完成" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
