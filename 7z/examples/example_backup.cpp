/*
 * SevenZip SDK - 备份管理示例
 * 
 * 本示例演示:
 * - 完整备份
 * - 增量备份
 * - 差异备份
 * - 恢复备份
 */

#include "../7zsdk.cpp"
#include <iostream>
#include <iomanip>

using namespace SevenZip;

void PrintSeparator(const std::string& title) {
    std::cout << "\n========== " << title << " ==========\n" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "   SevenZip SDK - 备份管理示例" << std::endl;
    std::cout << "========================================" << std::endl;
    
    SevenZipArchive archive;
    BackupManager backupMgr(archive);
    
    // ========================================
    // 1. 完整备份
    // ========================================
    PrintSeparator("1. 完整备份");
    
    BackupOptions fullBackupOpts;
    fullBackupOpts.type = BackupType::Full;
    fullBackupOpts.preservePermissions = true;
    fullBackupOpts.preserveTimestamps = true;
    fullBackupOpts.includeEmptyDirectories = true;
    
    std::cout << "备份类型: 完整备份" << std::endl;
    std::cout << "保留权限: 是" << std::endl;
    std::cout << "保留时间戳: 是" << std::endl;
    std::cout << "包含空目录: 是" << std::endl;
    
    BackupResult fullResult;
    // backupMgr.CreateBackup("full_backup.7z", "C:\\ImportantData", fullBackupOpts, fullResult);
    
    std::cout << "\n示例代码:" << std::endl;
    std::cout << "  BackupResult fullResult;" << std::endl;
    std::cout << "  backupMgr.CreateBackup(\"full_backup.7z\", \"C:\\\\ImportantData\", fullBackupOpts, fullResult);" << std::endl;
    std::cout << "\n备份结果:" << std::endl;
    std::cout << "  处理文件数: " << fullResult.filesProcessed << std::endl;
    std::cout << "  处理字节数: " << fullResult.bytesProcessed << std::endl;
    std::cout << "  跳过文件数: " << fullResult.filesSkipped << std::endl;
    
    // ========================================
    // 2. 增量备份
    // ========================================
    PrintSeparator("2. 增量备份");
    
    BackupOptions incBackupOpts;
    incBackupOpts.type = BackupType::Incremental;
    incBackupOpts.baseArchive = "full_backup.7z";  // 基于完整备份
    incBackupOpts.preserveTimestamps = true;
    
    std::cout << "备份类型: 增量备份" << std::endl;
    std::cout << "基础备份: full_backup.7z" << std::endl;
    std::cout << "\n说明: 只备份自上次备份以来修改过的文件" << std::endl;
    
    BackupResult incResult;
    // backupMgr.CreateBackup("incremental_001.7z", "C:\\ImportantData", incBackupOpts, incResult);
    
    std::cout << "\n示例代码:" << std::endl;
    std::cout << "  incBackupOpts.baseArchive = \"full_backup.7z\";" << std::endl;
    std::cout << "  backupMgr.CreateBackup(\"incremental_001.7z\", \"C:\\\\ImportantData\", incBackupOpts, incResult);" << std::endl;
    
    // ========================================
    // 3. 差异备份
    // ========================================
    PrintSeparator("3. 差异备份");
    
    BackupOptions diffBackupOpts;
    diffBackupOpts.type = BackupType::Differential;
    diffBackupOpts.baseArchive = "full_backup.7z";  // 始终基于完整备份
    
    std::cout << "备份类型: 差异备份" << std::endl;
    std::cout << "基础备份: full_backup.7z" << std::endl;
    std::cout << "\n说明: 备份自完整备份以来所有修改过的文件" << std::endl;
    std::cout << "与增量备份的区别: 差异备份不依赖中间的增量备份" << std::endl;
    
    BackupResult diffResult;
    // backupMgr.CreateBackup("differential_001.7z", "C:\\ImportantData", diffBackupOpts, diffResult);
    
    // ========================================
    // 4. 排除模式
    // ========================================
    PrintSeparator("4. 使用排除模式");
    
    BackupOptions excludeOpts;
    excludeOpts.type = BackupType::Full;
    excludeOpts.excludePatterns = {
        "*.tmp",      // 临时文件
        "*.log",      // 日志文件
        "*.bak",      // 备份文件
        "Thumbs.db",  // Windows 缩略图缓存
        ".git",       // Git 目录
        "node_modules" // Node.js 模块
    };
    
    std::cout << "排除模式:" << std::endl;
    for (const auto& pattern : excludeOpts.excludePatterns) {
        std::cout << "  - " << pattern << std::endl;
    }
    
    // backupMgr.CreateBackup("clean_backup.7z", "C:\\Project", excludeOpts, result);
    
    // ========================================
    // 5. 包含模式
    // ========================================
    PrintSeparator("5. 使用包含模式");
    
    BackupOptions includeOpts;
    includeOpts.type = BackupType::Full;
    includeOpts.includePatterns = {
        "*.cpp",
        "*.h",
        "*.hpp",
        "CMakeLists.txt",
        "*.cmake"
    };
    
    std::cout << "只包含以下文件类型:" << std::endl;
    for (const auto& pattern : includeOpts.includePatterns) {
        std::cout << "  - " << pattern << std::endl;
    }
    
    // backupMgr.CreateBackup("source_backup.7z", "C:\\Project", includeOpts, result);
    
    // ========================================
    // 6. 恢复备份
    // ========================================
    PrintSeparator("6. 恢复备份");
    
    RestoreOptions restoreOpts;
    restoreOpts.password = "";
    restoreOpts.overwrite = false;
    restoreOpts.preservePermissions = true;
    restoreOpts.preserveTimestamps = true;
    
    std::cout << "恢复选项:" << std::endl;
    std::cout << "  覆盖已存在文件: 否" << std::endl;
    std::cout << "  保留权限: 是" << std::endl;
    std::cout << "  保留时间戳: 是" << std::endl;
    
    RestoreResult restoreResult;
    // backupMgr.RestoreBackup("full_backup.7z", "C:\\Restore", restoreOpts, restoreResult);
    
    std::cout << "\n示例代码:" << std::endl;
    std::cout << "  RestoreResult restoreResult;" << std::endl;
    std::cout << "  backupMgr.RestoreBackup(\"full_backup.7z\", \"C:\\\\Restore\", restoreOpts, restoreResult);" << std::endl;
    
    // ========================================
    // 7. 恢复特定文件
    // ========================================
    PrintSeparator("7. 恢复特定文件");
    
    RestoreOptions selectiveRestore;
    selectiveRestore.filesToRestore = {
        "documents/important.docx",
        "config/settings.json",
        "data/database.db"
    };
    
    std::cout << "只恢复以下文件:" << std::endl;
    for (const auto& file : selectiveRestore.filesToRestore) {
        std::cout << "  - " << file << std::endl;
    }
    
    // backupMgr.RestoreBackup("full_backup.7z", "C:\\PartialRestore", selectiveRestore, restoreResult);
    
    // ========================================
    // 8. 备份链恢复
    // ========================================
    PrintSeparator("8. 备份链恢复 (完整 + 增量)");
    
    std::cout << "恢复顺序:" << std::endl;
    std::cout << "  1. 先恢复完整备份: full_backup.7z" << std::endl;
    std::cout << "  2. 再恢复增量备份: incremental_001.7z" << std::endl;
    std::cout << "  3. 继续恢复后续增量: incremental_002.7z, ..." << std::endl;
    
    std::cout << "\n示例代码:" << std::endl;
    std::cout << "  // 恢复完整备份" << std::endl;
    std::cout << "  backupMgr.RestoreBackup(\"full_backup.7z\", \"C:\\\\Restore\", opts, result);" << std::endl;
    std::cout << "  // 恢复增量备份 (覆盖更新的文件)" << std::endl;
    std::cout << "  restoreOpts.overwrite = true;" << std::endl;
    std::cout << "  backupMgr.RestoreBackup(\"incremental_001.7z\", \"C:\\\\Restore\", restoreOpts, result);" << std::endl;
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "   备份管理示例完成" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
