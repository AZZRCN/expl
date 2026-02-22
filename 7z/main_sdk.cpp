#include "SevenZipSDK.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <sstream>

using namespace SevenZip;

void PrintProgress(const ProgressInfo& info) {
    std::cout << "\rProgress: " << std::setw(3) << info.percent << "% | ";
    std::cout << "Files: " << std::setw(4) << info.completedFiles << "/" << info.totalFiles << " | ";
    std::cout << std::setw(30) << std::left << info.currentFile.substr(0, 30);
    if (info.currentVolume > 0) {
        std::cout << " Vol: " << info.currentVolume;
    }
    std::cout << std::flush;
}

void OnComplete(bool success, const std::string& archivePath) {
    std::cout << std::endl;
    if (success) {
        std::cout << "Completed: " << archivePath << std::endl;
    } else {
        std::cout << "Failed!" << std::endl;
    }
}

void CreateTestFiles(const std::string& baseDir) {
    CreateDirectoryA(baseDir.c_str(), NULL);
    CreateDirectoryA((baseDir + "\\subdir1").c_str(), NULL);
    CreateDirectoryA((baseDir + "\\subdir2").c_str(), NULL);
    CreateDirectoryA((baseDir + "\\subdir1\\deep").c_str(), NULL);
    
    HANDLE hFile;
    DWORD written;
    
    std::string file1 = baseDir + "\\file1.txt";
    hFile = CreateFileA(file1.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    const char* data1 = "This is test file 1 content for compression testing.";
    WriteFile(hFile, data1, (DWORD)strlen(data1), &written, NULL);
    CloseHandle(hFile);
    
    std::string file2 = baseDir + "\\file2.txt";
    hFile = CreateFileA(file2.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    const char* data2 = "This is test file 2 content with some different text.";
    WriteFile(hFile, data2, (DWORD)strlen(data2), &written, NULL);
    CloseHandle(hFile);
    
    std::string file3 = baseDir + "\\subdir1\\file3.txt";
    hFile = CreateFileA(file3.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    const char* data3 = "File in subdirectory 1.";
    WriteFile(hFile, data3, (DWORD)strlen(data3), &written, NULL);
    CloseHandle(hFile);
    
    std::string file4 = baseDir + "\\subdir1\\deep\\file4.txt";
    hFile = CreateFileA(file4.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    const char* data4 = "Deep nested file content.";
    WriteFile(hFile, data4, (DWORD)strlen(data4), &written, NULL);
    CloseHandle(hFile);
    
    std::string file5 = baseDir + "\\subdir2\\file5.txt";
    hFile = CreateFileA(file5.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    const char* data5 = "Another file in subdir2.";
    WriteFile(hFile, data5, (DWORD)strlen(data5), &written, NULL);
    CloseHandle(hFile);
    
    std::cout << "Created test files in: " << baseDir << std::endl;
}

int main() {
    std::cout << "=== 7-Zip SDK Full Test ===" << std::endl;
    
    char currentDir[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, currentDir);
    std::string baseDir = std::string(currentDir) + "\\temp";
    std::string testDir = baseDir + "\\testfiles";
    std::string outputDir = baseDir + "\\output";
    
    SevenZipArchive compressor("7z.dll");
    
    if (!compressor.Initialize()) {
        std::cerr << "Failed to initialize 7z.dll!" << std::endl;
        return 1;
    }
    
    compressor.SetProgressCallback(PrintProgress);
    compressor.SetCompleteCallback(OnComplete);
    
    CreateDirectoryA(baseDir.c_str(), NULL);
    CreateDirectoryA(outputDir.c_str(), NULL);
    CreateTestFiles(testDir);
    
    std::cout << "\n--- Test 1: Basic Directory Compression ---" << std::endl;
    CompressionOptions options;
    options.level = CompressionLevel::Normal;
    
    std::string archive1 = baseDir + "\\basic.7z";
    if (compressor.CompressDirectory(archive1, testDir, options, true)) {
        std::cout << "Success: basic.7z created!" << std::endl;
    } else {
        std::cout << "Failed to create basic.7z!" << std::endl;
    }
    
    std::cout << "\n--- Test 2: Password Protected Archive ---" << std::endl;
    options.password = "test123";
    options.encryptHeaders = true;
    
    std::string archive2 = baseDir + "\\encrypted.7z";
    if (compressor.CompressDirectory(archive2, testDir, options, true)) {
        std::cout << "Success: encrypted.7z created with password!" << std::endl;
    } else {
        std::cout << "Failed to create encrypted.7z!" << std::endl;
    }
    
    std::cout << "\n--- Test 3: Relative Path Compression ---" << std::endl;
    options.password = "";
    options.encryptHeaders = false;
    options.rootFolderName = "MyData";
    
    std::string archive3 = baseDir + "\\relative.7z";
    if (compressor.CompressWithRelativePath(archive3, testDir, testDir, options, true)) {
        std::cout << "Success: relative.7z created with root folder 'MyData'!" << std::endl;
    } else {
        std::cout << "Failed to create relative.7z!" << std::endl;
    }
    
    std::cout << "\n--- Test 4: Volume (Split) Archive ---" << std::endl;
    options.rootFolderName = "";
    options.volumeSize = 100;
    
    for (int i = 1; i <= 10; i++) {
        std::ostringstream oss;
        oss << baseDir << "\\split.7z." << std::setfill('0') << std::setw(3) << i;
        DeleteFileA(oss.str().c_str());
    }
    
    compressor.SetVolumeCallback([](uint32_t index, const std::string& path) {
        std::cout << "\n  Creating volume " << index << ": " << path;
        return true;
    });
    
    std::string archive4 = baseDir + "\\split.7z";
    if (compressor.CompressDirectory(archive4, testDir, options, true)) {
        std::cout << "\nSuccess: split volumes created!" << std::endl;
    } else {
        std::cout << "\nFailed to create split volumes!" << std::endl;
    }
    
    compressor.SetVolumeCallback(nullptr);
    
    std::cout << "\n--- Test 5: ZIP Format ---" << std::endl;
    options.volumeSize = 0;
    options.method = CompressionMethod::DEFLATE;
    
    std::string archive5 = baseDir + "\\archive.zip";
    if (compressor.CompressDirectory(archive5, testDir, options, true)) {
        std::cout << "Success: archive.zip created!" << std::endl;
    } else {
        std::cout << "Failed to create archive.zip!" << std::endl;
    }
    
    std::cout << "\n--- Test 6: List Archive Contents ---" << std::endl;
    ArchiveInfo info;
    if (compressor.ListArchive(archive1, info)) {
        std::cout << "Archive: " << info.path << std::endl;
        std::cout << "Files: " << info.fileCount << ", Directories: " << info.directoryCount << std::endl;
        std::cout << "Uncompressed: " << info.uncompressedSize << " bytes" << std::endl;
        std::cout << "Contents:" << std::endl;
        for (const auto& file : info.files) {
            std::cout << "  " << (file.isDirectory ? "[DIR] " : "      ") << file.path;
            if (!file.isDirectory) {
                std::cout << " (" << file.size << " bytes)";
            }
            std::cout << std::endl;
        }
    }
    
    std::cout << "\n--- Test 7: Extract Archive ---" << std::endl;
    ExtractOptions extractOpts;
    extractOpts.outputDir = outputDir;
    
    if (compressor.ExtractArchive(archive1, extractOpts)) {
        std::cout << "Success: Extracted to output!" << std::endl;
    } else {
        std::cout << "Failed to extract!" << std::endl;
    }
    
    std::cout << "\n--- Test 8: Extract Encrypted Archive ---" << std::endl;
    extractOpts.password = "test123";
    extractOpts.outputDir = baseDir + "\\output_enc";
    
    if (compressor.ExtractArchive(archive2, extractOpts)) {
        std::cout << "Success: Extracted encrypted archive!" << std::endl;
    } else {
        std::cout << "Failed to extract encrypted archive!" << std::endl;
    }
    
    std::cout << "\n--- Test 9: Test Archive Integrity ---" << std::endl;
    if (compressor.TestArchive(archive1)) {
        std::cout << "Archive integrity test passed!" << std::endl;
    } else {
        std::cout << "Archive integrity test failed!" << std::endl;
    }
    
    std::cout << "\n--- Test 10: Get Volume Info ---" << std::endl;
    VolumeInfo volInfo;
    std::string volPath = baseDir + "\\split.7z.001";
    if (compressor.GetVolumeInfo(volPath, volInfo)) {
        std::cout << "Volume count: " << volInfo.volumeCount << std::endl;
        std::cout << "Volumes:" << std::endl;
        for (const auto& vol : volInfo.volumePaths) {
            std::cout << "  " << vol << std::endl;
        }
    } else {
        std::cout << "No volumes found or single archive." << std::endl;
    }
    
    std::cout << "\n=== All Tests Completed ===" << std::endl;
    std::cout << "Check the 'temp' folder for results." << std::endl;
    
    return 0;
}
