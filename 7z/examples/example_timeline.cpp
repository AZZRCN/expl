/*
 * SevenZip SDK - 时间线备份示例
 * 
 * 本示例演示:
 * - 创建时间点
 * - 恢复到特定时间点
 * - 管理时间线
 */

#include "../7zsdk.cpp"
#include <iostream>

using namespace SevenZip;

void PrintSeparator(const std::string& title) {
    std::cout << "\n========== " << title << " ==========\n" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "   SevenZip SDK - 时间线备份示例" << std::endl;
    std::cout << "========================================" << std::endl;
    
    SevenZipArchive archive;
    TimelineBackup timeline(archive, "timeline_storage");
    
    // ========================================
    // 1. 创建时间点
    // ========================================
    PrintSeparator("1. 创建时间点");
    
    std::cout << "创建带有描述的时间点:" << std::endl;
    
    // std::string entryId = timeline.CreateEntry("C:\\Data", "每日备份 - 2024-01-15");
    
    std::cout << "\n示例代码:" << std::endl;
    std::cout << "  std::string entryId = timeline.CreateEntry(\"C:\\\\Data\", \"每日备份\");" << std::endl;
    std::cout << "  std::cout << \"时间点ID: \" << entryId << std::endl;" << std::endl;
    
    // ========================================
    // 2. 列出时间点
    // ========================================
    PrintSeparator("2. 列出时间点");
    
    // auto entries = timeline.ListEntries();
    
    std::cout << "时间点列表示例:" << std::endl;
    std::cout << std::endl;
    std::cout << "  ID                                    时间                  描述" << std::endl;
    std::cout << "  ----------------------------------------------------------------------------" << std::endl;
    std::cout << "  a1b2c3d4-...  2024-01-15 10:30:00  每日备份" << std::endl;
    std::cout << "  e5f6g7h8-...  2024-01-14 10:30:00  每日备份" << std::endl;
    std::cout << "  i9j0k1l2-...  2024-01-13 10:30:00  每日备份" << std::endl;
    
    std::cout << "\n示例代码:" << std::endl;
    std::cout << "  auto entries = timeline.ListEntries();" << std::endl;
    std::cout << "  for (const auto& entry : entries) {" << std::endl;
    std::cout << "      std::cout << entry.timestamp << \" - \" << entry.description << std::endl;" << std::endl;
    std::cout << "  }" << std::endl;
    
    // ========================================
    // 3. 恢复到时间点
    // ========================================
    PrintSeparator("3. 恢复到时间点");
    
    std::cout << "恢复到特定时间点:" << std::endl;
    
    // timeline.RestoreEntry(entryId, "C:\\Restore");
    
    std::cout << "\n示例代码:" << std::endl;
    std::cout << "  timeline.RestoreEntry(entryId, \"C:\\\\Restore\");" << std::endl;
    
    // ========================================
    // 4. 按时间恢复
    // ========================================
    PrintSeparator("4. 按时间恢复");
    
    std::cout << "恢复到最接近指定时间的版本:" << std::endl;
    
    // timeline.RestoreToTime("2024-01-14 12:00:00", "C:\\Restore");
    
    std::cout << "\n示例代码:" << std::endl;
    std::cout << "  timeline.RestoreToTime(\"2024-01-14 12:00:00\", \"C:\\\\Restore\");" << std::endl;
    
    // ========================================
    // 5. 删除时间点
    // ========================================
    PrintSeparator("5. 删除时间点");
    
    std::cout << "删除特定时间点:" << std::endl;
    
    // timeline.DeleteEntry(entryId);
    
    std::cout << "\n示例代码:" << std::endl;
    std::cout << "  timeline.DeleteEntry(entryId);" << std::endl;
    
    // ========================================
    // 6. 清理旧时间点
    // ========================================
    PrintSeparator("6. 清理旧时间点");
    
    std::cout << "自动清理策略:" << std::endl;
    std::cout << "  - 最多保留 30 个时间点" << std::endl;
    std::cout << "  - 删除超过 90 天的时间点" << std::endl;
    
    // timeline.PruneOldEntries(30, 90);
    
    std::cout << "\n示例代码:" << std::endl;
    std::cout << "  timeline.PruneOldEntries(30, 90);" << std::endl;
    
    // ========================================
    // 7. 时间线信息
    // ========================================
    PrintSeparator("7. 时间线信息");
    
    auto info = timeline.GetTimelineInfo();
    
    std::cout << "TimelineInfo 结构:" << std::endl;
    std::cout << "  - entryCount: 时间点数量" << std::endl;
    std::cout << "  - totalSize: 总存储大小" << std::endl;
    std::cout << "  - oldestEntry: 最早时间点" << std::endl;
    std::cout << "  - newestEntry: 最新时间点" << std::endl;
    
    std::cout << "\n当前状态:" << std::endl;
    std::cout << "  时间点数量: " << info.entryCount << std::endl;
    std::cout << "  总大小: " << info.totalSize << " bytes" << std::endl;
    
    // ========================================
    // 8. 差异恢复
    // ========================================
    PrintSeparator("8. 差异恢复");
    
    std::cout << "只恢复与当前版本不同的文件:" << std::endl;
    
    // timeline.RestoreChangedOnly(entryId, "C:\\Data");
    
    std::cout << "\n示例代码:" << std::endl;
    std::cout << "  timeline.RestoreChangedOnly(entryId, \"C:\\\\Data\");" << std::endl;
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "   时间线备份示例完成" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
