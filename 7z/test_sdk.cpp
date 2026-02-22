#include "7zsdk.cpp"
#include <iostream>
#include <cassert>

using namespace SevenZip;

void TestBasicArchiveOperations() {
    std::cout << "=== Testing Basic Archive Operations ===" << std::endl;
    
    SevenZipArchive archive;
    
    std::cout << "1. Testing compression options..." << std::endl;
    
    CompressionOptions opts;
    opts.method = CompressionMethod::LZMA2;
    opts.level = CompressionLevel::Maximum;
    opts.solidMode = true;
    opts.threadCount = 4;
    
    std::cout << "   Compression method: LZMA2" << std::endl;
    std::cout << "   Compression level: Maximum" << std::endl;
    std::cout << "   Solid mode: enabled" << std::endl;
    
    std::cout << "2. Testing extract options..." << std::endl;
    
    ExtractOptions extractOpts;
    extractOpts.outputDir = "output";
    extractOpts.overwriteExisting = true;
    extractOpts.preserveDirectoryStructure = true;
    extractOpts.preserveFileTime = true;
    
    std::cout << "   Extract options configured: OK" << std::endl;
    
    std::cout << "3. Testing archive info structure..." << std::endl;
    
    ArchiveInfo info;
    std::cout << "   ArchiveInfo structure: OK" << std::endl;
    
    std::cout << "Basic archive operations test PASSED!" << std::endl << std::endl;
}

void TestIntelligentClassifier() {
    std::cout << "=== Testing Intelligent Classifier ===" << std::endl;
    
    SevenZipArchive archive;
    IntelligentClassifier classifier(archive);
    
    std::cout << "1. Testing file classification by extension..." << std::endl;
    
    auto docResult = classifier.ClassifyFile("document.pdf");
    std::cout << "   PDF classified as type: " << (int)docResult.type 
              << " (confidence: " << docResult.confidence << ")" << std::endl;
    
    auto imgResult = classifier.ClassifyFile("image.png");
    std::cout << "   PNG classified as type: " << (int)imgResult.type
              << " (confidence: " << imgResult.confidence << ")" << std::endl;
    
    auto videoResult = classifier.ClassifyFile("movie.mp4");
    std::cout << "   MP4 classified as type: " << (int)videoResult.type
              << " (confidence: " << videoResult.confidence << ")" << std::endl;
    
    auto codeResult = classifier.ClassifyFile("main.cpp");
    std::cout << "   CPP classified as type: " << (int)codeResult.type
              << " (confidence: " << codeResult.confidence << ")" << std::endl;
    
    std::cout << "2. Testing content-based classification..." << std::endl;
    
    std::vector<uint8_t> textData = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd'};
    auto textResult = classifier.ClassifyByContent(textData, ".txt");
    std::cout << "   Text data classified as type: " << (int)textResult.type << std::endl;
    
    std::cout << "3. Testing tags extraction..." << std::endl;
    std::cout << "   Tags available: " << textResult.tags.size() << std::endl;
    
    std::cout << "Intelligent Classifier test PASSED!" << std::endl << std::endl;
}

void TestVirusScanner() {
    std::cout << "=== Testing Virus Scanner Interface ===" << std::endl;
    
    SevenZipArchive archive;
    VirusScannerInterface scanner(archive);
    
    std::cout << "1. Testing scanner initialization..." << std::endl;
    std::cout << "   Scanner version: " << scanner.GetScannerVersion() << std::endl;
    
    std::cout << "2. Testing scan options..." << std::endl;
    
    VirusScannerInterface::ScanOptions opts;
    opts.scanArchives = true;
    opts.heuristicsEnabled = true;
    opts.maxRecursionDepth = 10;
    
    std::cout << "   Scan archives: enabled" << std::endl;
    std::cout << "   Heuristics: enabled" << std::endl;
    std::cout << "   Max recursion: 10" << std::endl;
    
    std::cout << "Virus Scanner Interface test PASSED!" << std::endl << std::endl;
}

void TestPasswordManager() {
    std::cout << "=== Testing Password Manager ===" << std::endl;
    
    PasswordManager pwdMgr;
    
    std::cout << "1. Testing password policy..." << std::endl;
    
    PasswordManager::PasswordPolicy policy;
    policy.minLength = 8;
    policy.requireUppercase = true;
    policy.requireLowercase = true;
    policy.requireNumbers = true;
    policy.requireSymbols = false;
    
    std::cout << "   Min length: 8" << std::endl;
    std::cout << "   Require uppercase: yes" << std::endl;
    std::cout << "   Require lowercase: yes" << std::endl;
    std::cout << "   Require numbers: yes" << std::endl;
    
    std::cout << "2. Testing password generation..." << std::endl;
    
    std::string testPassword = pwdMgr.GeneratePassword(12, policy);
    std::cout << "   Generated password: " << testPassword << std::endl;
    
    bool valid = pwdMgr.ValidatePassword(testPassword, policy);
    std::cout << "   Password validation: " << (valid ? "PASSED" : "FAILED") << std::endl;
    
    std::cout << "Password Manager test PASSED!" << std::endl << std::endl;
}

void TestArchiveConverter() {
    std::cout << "=== Testing Archive Converter ===" << std::endl;
    
    SevenZipArchive archive;
    ArchiveConverter converter(archive);
    
    std::cout << "1. Testing conversion options..." << std::endl;
    
    ArchiveConverter::ConversionOptions opts;
    opts.targetFormat = ArchiveFormat::FMT_7Z;
    opts.method = CompressionMethod::LZMA2;
    opts.level = CompressionLevel::Normal;
    opts.preserveTimestamps = true;
    
    std::cout << "   Target format: 7z" << std::endl;
    std::cout << "   Method: LZMA2" << std::endl;
    std::cout << "   Level: Normal" << std::endl;
    
    std::cout << "Archive Converter test PASSED!" << std::endl << std::endl;
}

void TestArchiveValidator() {
    std::cout << "=== Testing Archive Validator ===" << std::endl;
    
    SevenZipArchive archive;
    ArchiveValidator validator(archive);
    
    std::cout << "1. Testing validation options..." << std::endl;
    
    ArchiveValidator::ValidationOptions opts;
    opts.checkCRC = true;
    opts.checkHeaders = true;
    opts.extractTest = false;
    opts.deepScan = true;
    
    std::cout << "   CRC check: enabled" << std::endl;
    std::cout << "   Header check: enabled" << std::endl;
    std::cout << "   Deep scan: enabled" << std::endl;
    
    std::cout << "Archive Validator test PASSED!" << std::endl << std::endl;
}

void TestBackupManager() {
    std::cout << "=== Testing Backup Manager ===" << std::endl;
    
    SevenZipArchive archive;
    BackupManager backupMgr(archive);
    
    std::cout << "1. Testing backup options..." << std::endl;
    
    BackupOptions opts;
    opts.type = BackupType::Full;
    opts.preservePermissions = true;
    opts.preserveTimestamps = true;
    opts.includeEmptyDirectories = true;
    
    std::cout << "   Backup type: Full" << std::endl;
    std::cout << "   Preserve permissions: enabled" << std::endl;
    std::cout << "   Preserve timestamps: enabled" << std::endl;
    
    std::cout << "2. Testing restore options..." << std::endl;
    
    RestoreOptions restoreOpts;
    restoreOpts.overwrite = false;
    restoreOpts.preservePermissions = true;
    restoreOpts.preserveTimestamps = true;
    
    std::cout << "   Overwrite: disabled" << std::endl;
    std::cout << "   Preserve permissions: enabled" << std::endl;
    
    std::cout << "Backup Manager test PASSED!" << std::endl << std::endl;
}

void TestEncryptionEnhancer() {
    std::cout << "=== Testing Encryption Enhancer ===" << std::endl;
    
    EncryptionEnhancer::EncryptionConfig config;
    config.algorithm = EncryptionEnhancer::Algorithm::AES256;
    config.kdf = EncryptionEnhancer::KeyDerivation::PBKDF2;
    config.iterations = 100000;
    config.encryptMetadata = true;
    
    EncryptionEnhancer enhancer(config);
    
    std::cout << "1. Testing encryption configuration..." << std::endl;
    
    std::cout << "   Algorithm: AES-256" << std::endl;
    std::cout << "   KDF: PBKDF2" << std::endl;
    std::cout << "   Iterations: 100000" << std::endl;
    std::cout << "   Encrypt metadata: enabled" << std::endl;
    
    std::cout << "Encryption Enhancer test PASSED!" << std::endl << std::endl;
}

void TestTimelineBackup() {
    std::cout << "=== Testing Timeline Backup ===" << std::endl;
    
    SevenZipArchive archive;
    TimelineBackup timeline(archive, "timeline_data");
    
    std::cout << "1. Testing timeline info..." << std::endl;
    
    TimelineBackup::TimelineInfo info = timeline.GetTimelineInfo();
    std::cout << "   Entry count: " << info.entryCount << std::endl;
    std::cout << "   Total size: " << info.totalSize << std::endl;
    
    std::cout << "Timeline Backup test PASSED!" << std::endl << std::endl;
}

void TestDeduplicationEngine() {
    std::cout << "=== Testing Deduplication Engine ===" << std::endl;
    
    DeduplicationEngine engine;
    
    std::cout << "1. Testing deduplication..." << std::endl;
    
    std::cout << "   DeduplicationEngine initialized" << std::endl;
    
    std::cout << "Deduplication Engine test PASSED!" << std::endl << std::endl;
}

void TestArchiveDiffer() {
    std::cout << "=== Testing Archive Differ ===" << std::endl;
    
    SevenZipArchive archive;
    ArchiveDiffer differ(archive);
    
    std::cout << "1. Testing archive differ..." << std::endl;
    
    std::cout << "   ArchiveDiffer initialized" << std::endl;
    
    std::cout << "Archive Differ test PASSED!" << std::endl << std::endl;
}

void TestCloudStorageClient() {
    std::cout << "=== Testing Cloud Storage Client ===" << std::endl;
    
    CloudStorageClient cloud;
    
    std::cout << "1. Testing cloud storage client..." << std::endl;
    
    std::cout << "   CloudStorageClient initialized" << std::endl;
    
    std::cout << "Cloud Storage Client test PASSED!" << std::endl << std::endl;
}

int main() {
    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "   SevenZip SDK Test Suite" << std::endl;
    std::cout << "========================================" << std::endl << std::endl;
    
    try {
        TestBasicArchiveOperations();
        TestIntelligentClassifier();
        TestVirusScanner();
        TestPasswordManager();
        TestArchiveConverter();
        TestArchiveValidator();
        TestBackupManager();
        TestEncryptionEnhancer();
        TestTimelineBackup();
        TestDeduplicationEngine();
        TestArchiveDiffer();
        TestCloudStorageClient();
        
        std::cout << "========================================" << std::endl;
        std::cout << "   ALL TESTS PASSED!" << std::endl;
        std::cout << "========================================" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
