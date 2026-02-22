/*
 * SevenZip SDK - 智能分类示例
 * 
 * 本示例演示:
 * - 文件类型分类
 * - 内容识别
 * - 压缩包分类
 * - 自动归档整理
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
    std::cout << "   SevenZip SDK - 智能分类示例" << std::endl;
    std::cout << "========================================" << std::endl;
    
    SevenZipArchive archive;
    IntelligentClassifier classifier(archive);
    
    // ========================================
    // 1. 文件类型分类
    // ========================================
    PrintSeparator("1. 文件类型分类");
    
    std::vector<std::string> testFiles = {
        "document.pdf",
        "spreadsheet.xlsx",
        "presentation.pptx",
        "image.jpg",
        "photo.png",
        "video.mp4",
        "music.mp3",
        "source.cpp",
        "script.py",
        "archive.zip",
        "data.json",
        "program.exe"
    };
    
    std::cout << std::left;
    std::cout << std::setw(25) << "文件名" 
              << std::setw(15) << "类型" 
              << std::setw(10) << "置信度" << std::endl;
    std::cout << std::string(50, '-') << std::endl;
    
    for (const auto& file : testFiles) {
        auto result = classifier.ClassifyFile(file);
        
        std::string typeName;
        switch (result.type) {
            case IntelligentClassifier::FileType::Document: typeName = "Document"; break;
            case IntelligentClassifier::FileType::Image: typeName = "Image"; break;
            case IntelligentClassifier::FileType::Video: typeName = "Video"; break;
            case IntelligentClassifier::FileType::Audio: typeName = "Audio"; break;
            case IntelligentClassifier::FileType::Archive: typeName = "Archive"; break;
            case IntelligentClassifier::FileType::Code: typeName = "Code"; break;
            case IntelligentClassifier::FileType::Data: typeName = "Data"; break;
            case IntelligentClassifier::FileType::Executable: typeName = "Executable"; break;
            default: typeName = "Other"; break;
        }
        
        std::cout << std::setw(25) << file 
                  << std::setw(15) << typeName
                  << std::setw(10) << result.confidence << std::endl;
    }
    
    // ========================================
    // 2. 内容识别
    // ========================================
    PrintSeparator("2. 内容识别");
    
    // PDF 文件头
    std::vector<uint8_t> pdfData = {0x25, 0x50, 0x44, 0x46, 0x2D, 0x31, 0x2E, 0x34};
    auto pdfResult = classifier.ClassifyByContent(pdfData, ".bin");
    std::cout << "PDF 文件头数据 -> 类型: " << (int)pdfResult.type 
              << ", 置信度: " << pdfResult.confidence << std::endl;
    
    // PNG 文件头
    std::vector<uint8_t> pngData = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    auto pngResult = classifier.ClassifyByContent(pngData, ".bin");
    std::cout << "PNG 文件头数据 -> 类型: " << (int)pngResult.type 
              << ", 置信度: " << pngResult.confidence << std::endl;
    
    // 文本数据
    std::vector<uint8_t> textData = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd'};
    auto textResult = classifier.ClassifyByContent(textData, ".txt");
    std::cout << "文本数据 -> 类型: " << (int)textResult.type 
              << ", 置信度: " << textResult.confidence << std::endl;
    
    // ========================================
    // 3. 获取文件标签
    // ========================================
    PrintSeparator("3. 获取文件标签");
    
    auto codeResult = classifier.ClassifyFile("main.cpp");
    std::cout << "main.cpp 的标签:" << std::endl;
    for (const auto& tag : codeResult.tags) {
        std::cout << "  - " << tag << std::endl;
    }
    
    auto imageResult = classifier.ClassifyFile("photo.jpg");
    std::cout << "\nphoto.jpg 的标签:" << std::endl;
    for (const auto& tag : imageResult.tags) {
        std::cout << "  - " << tag << std::endl;
    }
    
    // ========================================
    // 4. 分类整个压缩包
    // ========================================
    PrintSeparator("4. 分类整个压缩包");
    
    // auto archiveClass = classifier.ClassifyArchive("mixed_content.7z");
    
    std::cout << "压缩包分类结果:" << std::endl;
    std::cout << "  主导类型: 根据文件大小占比确定" << std::endl;
    std::cout << "  类别标签: 合并所有文件的标签" << std::endl;
    std::cout << "  建议名称: 根据内容自动生成" << std::endl;
    
    std::cout << "\n示例代码:" << std::endl;
    std::cout << "  auto result = classifier.ClassifyArchive(\"archive.7z\");" << std::endl;
    std::cout << "  std::cout << \"主导类型: \" << (int)result.dominantType << std::endl;" << std::endl;
    std::cout << "  std::cout << \"类别: \" << result.categories.size() << std::endl;" << std::endl;
    
    // ========================================
    // 5. 自动归档整理
    // ========================================
    PrintSeparator("5. 自动归档整理");
    
    std::cout << "根据内容自动整理压缩包到对应目录:" << std::endl;
    std::cout << std::endl;
    std::cout << "  原始位置              整理后位置" << std::endl;
    std::cout << "  ----------------------------------------" << std::endl;
    std::cout << "  photos.zip       ->   Images/photos.zip" << std::endl;
    std::cout << "  movies.7z        ->   Videos/movies.7z" << std::endl;
    std::cout << "  documents.rar    ->   Documents/documents.rar" << std::endl;
    std::cout << "  source.zip       ->   SourceCode/source.zip" << std::endl;
    
    // classifier.OrganizeArchive("photos.zip", "C:\\Organized");
    
    std::cout << "\n示例代码:" << std::endl;
    std::cout << "  classifier.OrganizeArchive(\"photos.zip\", \"C:\\\\Organized\");" << std::endl;
    
    // ========================================
    // 6. 支持的文件类型
    // ========================================
    PrintSeparator("6. 支持的文件类型");
    
    std::cout << "文档类型:" << std::endl;
    std::cout << "  .pdf, .doc, .docx, .xls, .xlsx, .ppt, .pptx, .txt, .rtf, .odt" << std::endl;
    
    std::cout << "\n图像类型:" << std::endl;
    std::cout << "  .jpg, .jpeg, .png, .gif, .bmp, .tiff, .webp, .svg, .ico, .psd" << std::endl;
    
    std::cout << "\n视频类型:" << std::endl;
    std::cout << "  .mp4, .avi, .mkv, .mov, .wmv, .flv, .webm, .m4v" << std::endl;
    
    std::cout << "\n音频类型:" << std::endl;
    std::cout << "  .mp3, .wav, .flac, .aac, .ogg, .wma, .m4a" << std::endl;
    
    std::cout << "\n压缩类型:" << std::endl;
    std::cout << "  .7z, .zip, .rar, .tar, .gz, .bz2, .xz" << std::endl;
    
    std::cout << "\n代码类型:" << std::endl;
    std::cout << "  .c, .cpp, .h, .hpp, .cs, .java, .py, .js, .ts, .html, .css, .sql" << std::endl;
    
    std::cout << "\n数据类型:" << std::endl;
    std::cout << "  .json, .xml, .csv, .db, .sqlite, .mdb" << std::endl;
    
    std::cout << "\n可执行类型:" << std::endl;
    std::cout << "  .exe, .dll, .so, .msi, .bat, .cmd, .ps1" << std::endl;
    
    // ========================================
    // 7. Magic Number 检测
    // ========================================
    PrintSeparator("7. Magic Number 检测");
    
    std::cout << "支持的 Magic Number 检测:" << std::endl;
    std::cout << std::endl;
    std::cout << "  格式      Magic Number" << std::endl;
    std::cout << "  ----------------------------------------" << std::endl;
    std::cout << "  ZIP       50 4B 03 04" << std::endl;
    std::cout << "  7z        37 7A BC AF 27 1C" << std::endl;
    std::cout << "  RAR       52 61 72 21" << std::endl;
    std::cout << "  PDF       25 50 44 46" << std::endl;
    std::cout << "  PNG       89 50 4E 47" << std::endl;
    std::cout << "  JPEG      FF D8 FF" << std::endl;
    std::cout << "  GIF       47 49 46 38" << std::endl;
    std::cout << "  BMP       42 4D" << std::endl;
    std::cout << "  EXE       4D 5A" << std::endl;
    std::cout << "  MP3       49 44 33" << std::endl;
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "   智能分类示例完成" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
