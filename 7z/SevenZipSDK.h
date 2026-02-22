#ifndef SEVENZIP_SDK_H
#define SEVENZIP_SDK_H

#ifdef _WIN32
#include <windows.h>
#endif

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <cstdint>
#include <map>
#include <chrono>

#ifdef SEVENZIP_EXPORTS
    #define SEVENZIP_API __declspec(dllexport)
#else
    #define SEVENZIP_API
#endif

namespace SevenZip {

enum class ArchiveFormat {
    SevenZip,
    Zip,
    GZip,
    BZip2,
    Tar,
    Xz,
    Wim,
    Rar,
    Cab,
    Iso,
    Vhd,
    Dmg,
    Auto
};

enum class CompressionLevel {
    Copy = 0,
    Fastest = 1,
    Fast = 3,
    Normal = 5,
    Maximum = 7,
    Ultra = 9
};

enum class CompressionMethod {
    Copy = 0,
    LZMA = 1,
    LZMA2 = 2,
    BZIP2 = 3,
    PPMD = 4,
    DEFLATE = 5,
    DEFLATE64 = 6,
    ZSTD = 7,
    LZ4 = 8,
    BROTLI = 9
};

struct ProgressInfo {
    uint64_t completedFiles;
    uint64_t totalFiles;
    uint64_t completedBytes;
    uint64_t totalBytes;
    uint32_t percent;
    std::string currentFile;
    uint32_t currentVolume;
    double speed;
};

struct FileInfo {
    std::string path;
    uint64_t size;
    uint64_t packedSize;
    uint32_t crc;
    uint32_t attributes;
    bool isDirectory;
    bool isEncrypted;
    std::chrono::system_clock::time_point modifiedTime;
};

struct ArchiveInfo {
    std::string path;
    uint64_t fileCount;
    uint64_t directoryCount;
    uint64_t uncompressedSize;
    uint64_t compressedSize;
    bool isSolid;
    bool isEncrypted;
    std::string method;
    std::vector<FileInfo> files;
};

struct CompressionOptions {
    ArchiveFormat format = ArchiveFormat::SevenZip;
    CompressionLevel level = CompressionLevel::Normal;
    CompressionMethod method = CompressionMethod::LZMA2;
    uint32_t dictionarySize = 0;
    uint32_t wordSize = 0;
    uint32_t solidBlockSize = 0;
    uint32_t threadCount = 0;
    std::string password;
    bool encryptHeaders = false;
    bool storeTimestamps = true;
    bool storeAttributes = true;
    bool followSymlinks = false;
    std::string rootFolderName;
    uint64_t volumeSize = 0;
    std::string filter;
    std::string excludeFilter;
};

struct ExtractOptions {
    std::string outputDir;
    std::string password;
    bool preservePaths = true;
    bool preserveTimestamps = true;
    bool preserveAttributes = true;
    bool createDirectories = true;
    bool overwriteExisting = true;
    std::string fileFilter;
};

struct HashResult {
    std::string algorithm;
    std::string hash;
    uint64_t fileSize;
};

struct VolumeInfo {
    uint32_t volumeCount;
    uint64_t totalSize;
    std::vector<std::string> volumePaths;
};

struct SfxOptions {
    std::string title;
    std::string installPath;
    std::string runProgram;
    bool silentMode = false;
    bool deleteAfterInstall = false;
    std::string sfxModule;
};

class SevenZipArchive {
public:
    SevenZipArchive(const std::string& dllPath = "7z.dll");
    ~SevenZipArchive();

    bool Initialize();
    bool IsInitialized() const;

    bool CompressDirectory(const std::string& archivePath, const std::string& sourceDir,
                          const CompressionOptions& options = {}, bool recursive = true);
    bool CompressFiles(const std::string& archivePath, const std::vector<std::string>& files,
                      const CompressionOptions& options = {});
    bool CompressWithRelativePath(const std::string& archivePath, const std::string& sourceDir,
                                 const std::string& basePath, const CompressionOptions& options = {},
                                 bool recursive = true);
    bool CompressFromMemory(const std::string& archivePath, const std::string& entryName,
                           const void* data, size_t size, const CompressionOptions& options = {});

    bool ExtractArchive(const std::string& archivePath, const ExtractOptions& options = {});
    bool ExtractFiles(const std::string& archivePath, const std::vector<std::string>& files,
                     const ExtractOptions& options = {});
    bool ExtractSingleFile(const std::string& archivePath, const std::string& filePath,
                          const std::string& outputPath, const std::string& password = "");
    bool ExtractSingleFileToMemory(const std::string& archivePath, const std::string& filePath,
                                   std::vector<uint8_t>& data, const std::string& password = "");

    bool ListArchive(const std::string& archivePath, ArchiveInfo& info, const std::string& password = "");
    bool TestArchive(const std::string& archivePath, const std::string& password = "");

    bool AddToArchive(const std::string& archivePath, const std::vector<std::string>& files,
                     const CompressionOptions& options = {});
    bool DeleteFromArchive(const std::string& archivePath, const std::vector<std::string>& files);
    bool RenameInArchive(const std::string& archivePath, const std::string& oldName, const std::string& newName);

    bool CalculateFileHash(const std::string& filePath, HashResult& result,
                          const std::string& algorithm = "CRC32");
    bool CalculateBufferHash(const void* data, size_t size, HashResult& result,
                            const std::string& algorithm = "CRC32");

    bool GetVolumeInfo(const std::string& firstVolumePath, VolumeInfo& info);
    bool MergeVolumes(const std::string& firstVolumePath, const std::string& outputPath);
    bool SplitArchive(const std::string& archivePath, const std::string& outputPattern, uint64_t volumeSize);

    bool CreateSfxArchive(const std::string& outputPath, const std::string& sourceDir,
                         const SfxOptions& options = {}, const CompressionOptions& compOptions = {});

    void SetProgressCallback(std::function<void(const ProgressInfo&)> callback);
    void SetCompleteCallback(std::function<void(bool, const std::string&)> callback);
    void SetVolumeCallback(std::function<bool(uint32_t, const std::string&)> callback);
    void SetPasswordCallback(std::function<std::string()> callback);

    void Cancel();
    bool IsCancelled() const;

    static std::string GetVersion();
    static std::string GetErrorMessage(int errorCode);
    static bool IsArchive(const std::string& filePath);
    static ArchiveFormat DetectFormat(const std::string& filePath);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

class CommandLineInterface {
public:
    CommandLineInterface();
    ~CommandLineInterface();

    int Execute(int argc, char* argv[]);
    void SetOutputCallback(std::function<void(const std::string&)> callback);
    void SetErrorCallback(std::function<void(const std::string&)> callback);

    static void PrintUsage();
    static void PrintVersion();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

class CloudStorageClient {
public:
    enum class Protocol { FTP, SFTP, WebDAV, S3 };

    struct ConnectionConfig {
        Protocol protocol;
        std::string host;
        uint16_t port;
        std::string username;
        std::string password;
        std::string basePath;
        bool useSSL;
        int timeout;
        int retryCount;
    };

    struct RemoteFile {
        std::string path;
        uint64_t size;
        time_t modifiedTime;
        bool isDirectory;
        std::string permissions;
    };

    struct TransferProgress {
        uint64_t bytesTransferred;
        uint64_t totalBytes;
        double speed;
        std::string currentFile;
        bool isUpload;
    };

    CloudStorageClient();
    ~CloudStorageClient();

    bool Connect(const ConnectionConfig& config);
    void Disconnect();

    bool UploadFile(const std::string& localPath, const std::string& remotePath);
    bool DownloadFile(const std::string& remotePath, const std::string& localPath);
    std::vector<RemoteFile> ListDirectory(const std::string& remotePath);
    bool CreateDirectory(const std::string& remotePath);
    bool DeleteFile(const std::string& remotePath);

    bool UploadArchive(const std::string& archivePath, const std::string& remotePath,
                      SevenZipArchive& archive, const std::string& sourceDir,
                      const CompressionOptions& options);
    bool DownloadAndExtract(const std::string& remotePath, const std::string& localPath,
                           SevenZipArchive& archive, const ExtractOptions& options);

    void SetProgressCallback(std::function<void(const TransferProgress&)> callback);
    void Cancel();
    bool IsConnected() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

class ArchiveRepair {
public:
    struct RepairResult {
        bool success;
        uint32_t filesRecovered;
        uint32_t filesLost;
        uint64_t bytesRecovered;
        std::vector<std::string> recoveredFiles;
        std::vector<std::string> lostFiles;
        std::string errorMessage;
    };

    struct RepairOptions {
        bool tryPartialRecovery;
        bool skipCorruptedFiles;
        bool rebuildHeaders;
        bool recoverDeleted;
        int maxRetries;
        std::string outputDir;
    };

    ArchiveRepair(SevenZipArchive& archive);

    RepairResult RepairArchive(const std::string& archivePath, const RepairOptions& options);
    bool ValidateArchive(const std::string& archivePath);
    std::vector<uint8_t> ExtractRawData(const std::string& archivePath, uint64_t offset, uint64_t size);
    bool RebuildArchive(const std::string& damagedPath, const std::string& outputPath);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

class DeduplicationEngine {
public:
    struct ChunkInfo {
        std::string hash;
        uint64_t offset;
        uint32_t size;
        uint32_t refCount;
    };

    struct DedupResult {
        uint64_t originalSize;
        uint64_t deduplicatedSize;
        uint64_t savedBytes;
        uint32_t totalChunks;
        uint32_t uniqueChunks;
        double deduplicationRatio;
    };

    struct DedupOptions {
        uint32_t chunkSize;
        uint32_t chunkSizeMin;
        uint32_t chunkSizeMax;
        std::string hashAlgorithm;
        bool variableSizeChunks;
        double similarityThreshold;
    };

    DeduplicationEngine();

    DedupResult DeduplicateFiles(const std::vector<std::string>& files);
    bool StoreDeduplicatedArchive(const std::string& archivePath,
                                  const std::vector<std::string>& files,
                                  SevenZipArchive& archive);
    std::vector<std::string> FindDuplicates(const std::vector<std::string>& files);
    uint64_t CalculateSavedSpace(const std::vector<std::string>& files);
    void ClearChunkStore();
    void SetOptions(const DedupOptions& options);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

class SfxScriptBuilder {
public:
    struct SfxConfig {
        std::string title;
        std::string beginPrompt;
        std::string extractDialogText;
        std::string extractPathText;
        std::string extractTitle;
        std::string errorTitle;
        std::string errorMessage;
        std::string installPath;
        std::string shortcutPath;
        std::string shortcutName;
        std::string runProgram;
        std::string runProgramArgs;
        std::string deleteAfterInstall;
        bool showExtractDialog;
        bool overwriteMode;
        bool guiMode;
        bool silentMode;
        bool createShortcut;
        bool runAfterExtract;
        bool deleteArchive;
    };

    SfxScriptBuilder();

    void SetConfig(const SfxConfig& config);
    SfxConfig& GetConfig();
    void SetSfxModule(const std::string& module);

    bool BuildSfxArchive(const std::string& outputPath,
                        const std::string& archivePath,
                        SevenZipArchive& archive);
    bool BuildSfxFromDirectory(const std::string& outputPath,
                              const std::string& sourceDir,
                              SevenZipArchive& archive,
                              const CompressionOptions& options);

    std::string GenerateConfigFile();
    std::string GenerateBatchScript(const std::string& archivePath);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

class MultiVolumeRecovery {
public:
    struct VolumeInfo {
        std::string path;
        uint64_t size;
        uint32_t index;
        bool isComplete;
        uint32_t crc;
    };

    struct RecoveryResult {
        bool success;
        uint32_t volumesRecovered;
        uint32_t volumesMissing;
        uint64_t bytesRecovered;
        std::vector<std::string> missingVolumes;
        std::string errorMessage;
    };

    MultiVolumeRecovery(SevenZipArchive& archive);

    std::vector<VolumeInfo> AnalyzeVolumes(const std::string& firstVolumePath);
    RecoveryResult RecoverMissingVolumes(const std::string& firstVolumePath,
                                        const std::string& parityPath);
    bool CreateParityFile(const std::string& firstVolumePath,
                         const std::string& parityPath,
                         uint32_t parityCount);
    bool MergeVolumes(const std::string& firstVolumePath,
                     const std::string& outputPath);
    bool SplitArchive(const std::string& archivePath,
                     const std::string& outputPattern,
                     uint64_t volumeSize);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

class ArchiveSearchEngine {
public:
    struct SearchResult {
        std::string archivePath;
        std::string filePath;
        uint64_t offset;
        uint64_t size;
        std::string context;
        double relevance;
    };

    struct SearchOptions {
        std::string query;
        bool caseSensitive;
        bool wholeWord;
        bool regex;
        bool searchInArchives;
        bool searchContent;
        bool searchFilenames;
        uint32_t maxResults;
        uint32_t contextLines;
    };

    ArchiveSearchEngine(SevenZipArchive& archive);

    std::vector<SearchResult> Search(const std::string& archivePath,
                                    const SearchOptions& options);
    std::vector<SearchResult> SearchMultiple(const std::vector<std::string>& archivePaths,
                                            const SearchOptions& options);
    void BuildIndex(const std::string& archivePath);
    void ClearIndex();
    std::vector<std::string> FindSimilarFiles(const std::string& archivePath,
                                              const std::string& referenceFile,
                                              double threshold);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

class CompressionAnalyzer {
public:
    struct AnalysisResult {
        uint64_t uncompressedSize;
        uint64_t compressedSize;
        double compressionRatio;
        std::string bestMethod;
        std::string bestLevel;
        uint32_t estimatedTime;
        std::map<std::string, double> methodRatios;
        std::map<std::string, uint32_t> methodTimes;
        std::vector<std::string> recommendations;
    };

    struct FileInfo {
        std::string path;
        uint64_t size;
        std::string extension;
        std::string type;
        double entropy;
        bool isCompressible;
    };

    CompressionAnalyzer(SevenZipArchive& archive);

    AnalysisResult Analyze(const std::string& sourcePath);
    FileInfo AnalyzeFile(const std::string& filePath);
    std::map<std::string, AnalysisResult> BenchmarkMethods(const std::string& sourcePath,
                                                           bool createTestArchives);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

class NtfsStreamHandler {
public:
    struct StreamInfo {
        std::string name;
        uint64_t size;
        std::string type;
    };

    struct SecurityDescriptor {
        std::string owner;
        std::string group;
        std::vector<std::string> dacl;
        std::vector<std::string> sacl;
    };

    NtfsStreamHandler(SevenZipArchive& archive);

    std::vector<StreamInfo> EnumerateStreams(const std::string& filePath);
    bool ReadAlternateStream(const std::string& filePath, const std::string& streamName,
                            std::vector<uint8_t>& data);
    bool WriteAlternateStream(const std::string& filePath, const std::string& streamName,
                             const std::vector<uint8_t>& data);
    bool DeleteAlternateStream(const std::string& filePath, const std::string& streamName);
    SecurityDescriptor GetSecurityDescriptor(const std::string& filePath);
    bool SetSecurityDescriptor(const std::string& filePath, const SecurityDescriptor& sd);
    bool ArchiveWithStreams(const std::string& archivePath, const std::string& sourcePath,
                           const CompressionOptions& options);
    bool ExtractWithStreams(const std::string& archivePath, const std::string& outputPath,
                           const ExtractOptions& options);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

class BatchProcessor {
public:
    struct BatchJob {
        std::string id;
        std::string sourcePath;
        std::string archivePath;
        std::string operation;
        std::string status;
        double progress;
        std::string errorMessage;
        time_t startTime;
        time_t endTime;
    };

    struct BatchResult {
        uint32_t totalJobs;
        uint32_t successfulJobs;
        uint32_t failedJobs;
        uint64_t totalBytesProcessed;
        double totalTime;
        std::vector<BatchJob> jobs;
    };

    BatchProcessor(SevenZipArchive& archive, size_t threads = 0);

    std::string AddCompressJob(const std::string& sourcePath, const std::string& archivePath,
                              const CompressionOptions& options = {});
    std::string AddExtractJob(const std::string& archivePath, const std::string& outputPath,
                             const ExtractOptions& options = {});
    std::string AddConvertJob(const std::string& sourceArchive, const std::string& targetArchive,
                             ArchiveFormat targetFormat);

    BatchResult ExecuteAll();
    void Cancel();
    void ClearJobs();
    std::vector<BatchJob> GetPendingJobs();
    void SetJobCallback(std::function<void(const BatchJob&)> callback);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

class BackupManager {
public:
    enum class BackupType { Full, Incremental, Differential };

    struct BackupOptions {
        BackupType type = BackupType::Full;
        std::string password;
        std::string previousBackupPath;
        bool compress = true;
        bool verifyAfterBackup = true;
        std::vector<std::string> excludePatterns;
    };

    struct BackupResult {
        bool success;
        uint64_t totalFiles;
        uint64_t totalBytes;
        uint64_t backedUpFiles;
        uint64_t backedUpBytes;
        std::chrono::seconds duration;
        std::string backupPath;
        std::string errorMessage;
    };

    struct RestoreOptions {
        std::string password;
        bool overwriteExisting = true;
        bool restoreTimestamps = true;
        bool restorePermissions = false;
    };

    struct RestoreResult {
        bool success;
        uint64_t restoredFiles;
        uint64_t restoredBytes;
        uint64_t skippedFiles;
        std::chrono::seconds duration;
        std::string errorMessage;
    };

    BackupManager(SevenZipArchive& archive);

    bool CreateBackup(const std::string& archivePath, const std::string& sourcePath,
                     const BackupOptions& options, BackupResult& result);
    bool RestoreBackup(const std::string& archivePath, const std::string& outputPath,
                      const RestoreOptions& options, RestoreResult& result);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

class StreamPipeline {
public:
    struct PipelineStats {
        uint64_t bytesProcessed;
        uint64_t bytesCompressed;
        double compressionRatio;
        double throughput;
    };

    StreamPipeline();
    ~StreamPipeline();

    bool CompressStream(const void* input, size_t inputSize,
                       void* output, size_t& outputSize,
                       const CompressionOptions& options = {});
    bool DecompressStream(const void* input, size_t inputSize,
                         void* output, size_t& outputSize);
    PipelineStats GetStats() const;
    void ResetStats();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

class MemoryMappedFile {
public:
    MemoryMappedFile();
    ~MemoryMappedFile();

    bool Open(const std::string& filePath, uint64_t maxSize = 0);
    void Close();
    void* GetData();
    const void* GetData() const;
    uint64_t GetSize() const;
    bool Flush();
    bool Resize(uint64_t newSize);
    bool IsOpen() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

class FileSystemWatcher {
public:
    struct ChangeEvent {
        std::string path;
        std::string oldPath;
        enum class Type { Added, Removed, Modified, Renamed } type;
        bool isDirectory;
        std::chrono::system_clock::time_point timestamp;
    };

    FileSystemWatcher();
    ~FileSystemWatcher();

    bool Watch(const std::string& directory, bool recursive = true);
    void Stop();
    void SetCallback(std::function<void(const ChangeEvent&)> callback);
    bool IsWatching() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

class DigitalSignature {
public:
    struct SignatureInfo {
        std::string signerName;
        std::string issuerName;
        std::chrono::system_clock::time_point validFrom;
        std::chrono::system_clock::time_point validTo;
        bool isValid;
        bool isTrusted;
        std::string serialNumber;
        std::string thumbprint;
    };

    DigitalSignature();
    ~DigitalSignature();

    bool SignFile(const std::string& filePath, const std::string& certPath,
                 const std::string& password = "");
    bool VerifyFile(const std::string& filePath, SignatureInfo& info);
    bool SignArchive(const std::string& archivePath, const std::string& certPath,
                    const std::string& password = "");
    bool VerifyArchive(const std::string& archivePath, SignatureInfo& info);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

class KeyFileEncryption {
public:
    struct KeyInfo {
        std::string keyId;
        std::string algorithm;
        uint32_t keySize;
        std::chrono::system_clock::time_point created;
        std::chrono::system_clock::time_point expires;
        bool isValid;
    };

    KeyFileEncryption();
    ~KeyFileEncryption();

    bool GenerateKeyFile(const std::string& keyPath, uint32_t keySize = 256);
    bool LoadKeyFile(const std::string& keyPath);
    bool EncryptFile(const std::string& inputPath, const std::string& outputPath);
    bool DecryptFile(const std::string& inputPath, const std::string& outputPath);
    bool EncryptArchive(const std::string& archivePath);
    bool DecryptArchive(const std::string& archivePath);
    KeyInfo GetKeyInfo() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

class LinkHandler {
public:
    struct LinkInfo {
        std::string linkPath;
        std::string targetPath;
        bool isSymbolic;
        bool isHard;
        bool isJunction;
        bool targetExists;
    };

    LinkHandler();

    bool CreateSymbolicLink(const std::string& linkPath, const std::string& targetPath,
                           bool isDirectory = false);
    bool CreateHardLink(const std::string& linkPath, const std::string& targetPath);
    bool CreateJunction(const std::string& junctionPath, const std::string& targetPath);
    LinkInfo GetLinkInfo(const std::string& path);
    bool IsSymbolicLink(const std::string& path);
    bool IsHardLink(const std::string& path);
    uint32_t GetHardLinkCount(const std::string& path);
    bool DeleteLink(const std::string& path);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

class VersionControl {
public:
    struct Version {
        std::string id;
        std::string message;
        std::string author;
        std::chrono::system_clock::time_point timestamp;
        std::vector<std::string> files;
        std::map<std::string, std::string> fileHashes;
    };

    struct DiffEntry {
        std::string path;
        enum Type { Added, Modified, Deleted } type;
        std::string oldHash;
        std::string newHash;
    };

    VersionControl(const std::string& archivePath, const std::string& password = "");

    bool Initialize();
    std::string Commit(const std::string& sourcePath, const std::string& message,
                      const std::string& author);
    std::vector<DiffEntry> Diff(const std::string& versionId1, const std::string& versionId2);
    bool Checkout(const std::string& versionId, const std::string& outputPath);
    std::vector<Version> GetHistory();
    Version* FindVersion(const std::string& versionId);
    std::string GetCurrentVersionId();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

class ThreadPool {
public:
    ThreadPool(size_t threads = 0);
    ~ThreadPool();

    template<class F>
    auto Enqueue(F&& f) -> std::future<decltype(f())>;

    void WaitAll();
    size_t GetThreadCount() const;
    int GetActiveTaskCount() const;
    size_t GetPendingTaskCount() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

class MultiThreadedCompressor {
public:
    MultiThreadedCompressor(SevenZipArchive& archive, size_t threads = 0);

    bool CompressFilesParallel(const std::string& archivePath,
                              const std::vector<std::string>& files,
                              const CompressionOptions& options);
    bool ExtractFilesParallel(const std::string& archivePath,
                             const std::string& outputDir,
                             const std::string& password = "");
    void Cancel();
    size_t GetThreadCount() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

class EncryptionEnhancer {
public:
    enum class EncryptionAlgorithm { AES256, ChaCha20, Twofish, Serpent, Camellia };
    enum class KeyDerivationFunction { PBKDF2, Argon2, Scrypt, BCrypt };

    struct EncryptionConfig {
        EncryptionAlgorithm algorithm = EncryptionAlgorithm::AES256;
        KeyDerivationFunction kdf = KeyDerivationFunction::PBKDF2;
        uint32_t iterations = 100000;
        uint32_t memoryCost = 65536;
        uint32_t parallelism = 4;
        std::string password;
        bool encryptHeaders = true;
        bool useMultipleLayers = false;
        std::vector<EncryptionAlgorithm> layerAlgorithms;
    };

    struct DecryptionInfo {
        EncryptionAlgorithm algorithm;
        KeyDerivationFunction kdf;
        bool isEncrypted;
        bool isHeaderEncrypted;
        uint32_t keySize;
    };

    EncryptionEnhancer(SevenZipArchive& archive);

    bool EncryptArchive(const std::string& archivePath, const EncryptionConfig& config);
    bool DecryptArchive(const std::string& archivePath, const std::string& password);
    bool ReEncryptArchive(const std::string& archivePath, const std::string& oldPassword,
                         const EncryptionConfig& newConfig);
    DecryptionInfo AnalyzeEncryption(const std::string& archivePath);
    bool AddEncryptionLayer(const std::string& archivePath, EncryptionAlgorithm algorithm,
                           const std::string& password);
    bool RemoveEncryptionLayer(const std::string& archivePath, const std::string& password);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

class ArchiveDiffer {
public:
    struct DiffEntry {
        std::string path;
        enum class Type { Added, Removed, Modified, Renamed } type;
        std::string oldPath;
        uint64_t oldSize;
        uint64_t newSize;
        std::string oldHash;
        std::string newHash;
    };

    struct DiffResult {
        uint32_t addedCount;
        uint32_t removedCount;
        uint32_t modifiedCount;
        uint32_t renamedCount;
        uint64_t addedSize;
        uint64_t removedSize;
        std::vector<DiffEntry> entries;
    };

    struct DeltaOptions {
        bool includeContent;
        bool includeMetadata;
        bool compressDelta;
        uint32_t chunkSize;
        std::string password;
    };

    ArchiveDiffer(SevenZipArchive& archive);

    DiffResult CompareArchives(const std::string& archive1, const std::string& archive2);
    bool CreateDeltaArchive(const std::string& baseArchive, const std::string& newArchive,
                           const std::string& deltaPath, const DeltaOptions& options);
    bool ApplyDeltaArchive(const std::string& baseArchive, const std::string& deltaPath,
                          const std::string& outputPath);
    std::vector<uint8_t> ComputeFileDelta(const std::vector<uint8_t>& oldData,
                                          const std::vector<uint8_t>& newData);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

class ArchivePreviewer {
public:
    struct PreviewResult {
        std::string filePath;
        std::string preview;
        std::string encoding;
        uint64_t previewSize;
        uint64_t totalSize;
        bool isText;
        bool isImage;
        bool isMedia;
        std::string mimeType;
    };

    struct PreviewOptions {
        uint32_t maxPreviewSize;
        bool detectEncoding;
        bool generateThumbnails;
        uint32_t thumbnailSize;
    };

    ArchivePreviewer(SevenZipArchive& archive);

    PreviewResult PreviewFile(const std::string& archivePath, const std::string& filePath,
                             const PreviewOptions& options);
    std::vector<PreviewResult> PreviewMultiple(const std::string& archivePath,
                                               const std::vector<std::string>& files,
                                               const PreviewOptions& options);
    std::string DetectEncoding(const std::vector<uint8_t>& data);
    std::vector<uint8_t> GenerateThumbnail(const std::string& archivePath,
                                           const std::string& imagePath,
                                           uint32_t size);
    bool IsTextFile(const std::string& archivePath, const std::string& filePath);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

class ArchiveOptimizer {
public:
    struct OptimizationResult {
        uint64_t originalSize;
        uint64_t optimizedSize;
        uint64_t savedBytes;
        double compressionRatio;
        uint32_t filesProcessed;
        uint32_t filesOptimized;
        std::vector<std::string> optimizedFiles;
    };

    struct OptimizationOptions {
        bool recompress;
        bool deduplicate;
        bool removeRedundant;
        bool optimizeSolid;
        bool defragment;
        CompressionMethod targetMethod;
        CompressionLevel targetLevel;
        uint32_t threadCount;
    };

    ArchiveOptimizer(SevenZipArchive& archive);

    OptimizationResult OptimizeArchive(const std::string& archivePath,
                                       const OptimizationOptions& options);
    bool DefragmentArchive(const std::string& archivePath, const std::string& outputPath);
    bool RecompressArchive(const std::string& archivePath, const std::string& outputPath,
                          CompressionMethod method, CompressionLevel level);
    bool RemoveRedundantData(const std::string& archivePath, const std::string& outputPath);
    bool OptimizeSolidSettings(const std::string& archivePath, const std::string& outputPath,
                              uint32_t solidBlockSize);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

class MetadataEditor {
public:
    struct ArchiveMetadata {
        std::string name;
        std::string comment;
        std::string author;
        std::chrono::system_clock::time_point createdTime;
        std::chrono::system_clock::time_point modifiedTime;
        std::map<std::string, std::string> customFields;
    };

    struct FileMetadata {
        std::string path;
        std::string comment;
        std::chrono::system_clock::time_point modifiedTime;
        std::chrono::system_clock::time_point createdTime;
        uint32_t attributes;
        std::map<std::string, std::string> customFields;
    };

    MetadataEditor(SevenZipArchive& archive);

    bool GetArchiveMetadata(const std::string& archivePath, ArchiveMetadata& metadata);
    bool SetArchiveMetadata(const std::string& archivePath, const ArchiveMetadata& metadata);
    bool GetFileMetadata(const std::string& archivePath, const std::string& filePath,
                        FileMetadata& metadata);
    bool SetFileMetadata(const std::string& archivePath, const std::string& filePath,
                        const FileMetadata& metadata);
    bool SetArchiveComment(const std::string& archivePath, const std::string& comment);
    bool SetFileComment(const std::string& archivePath, const std::string& filePath,
                       const std::string& comment);
    bool AddCustomField(const std::string& archivePath, const std::string& key,
                       const std::string& value);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

class ArchiveSynchronizer {
public:
    struct SyncResult {
        uint32_t filesCopied;
        uint32_t filesUpdated;
        uint32_t filesDeleted;
        uint32_t filesSkipped;
        uint64_t bytesTransferred;
        std::vector<std::string> copiedFiles;
        std::vector<std::string> updatedFiles;
        std::vector<std::string> deletedFiles;
    };

    struct SyncOptions {
        bool bidirectional;
        bool deleteOrphaned;
        bool preserveTimestamps;
        bool skipExisting;
        std::string excludePattern;
        std::string password;
    };

    struct SyncPoint {
        std::string id;
        std::string archivePath;
        std::chrono::system_clock::time_point syncTime;
        std::string checksum;
    };

    ArchiveSynchronizer(SevenZipArchive& archive);

    SyncResult SyncToArchive(const std::string& sourceDir, const std::string& archivePath,
                            const SyncOptions& options);
    SyncResult SyncFromArchive(const std::string& archivePath, const std::string& targetDir,
                              const SyncOptions& options);
    SyncResult BidirectionalSync(const std::string& archivePath, const std::string& dir,
                                const SyncOptions& options);
    bool CreateSyncPoint(const std::string& archivePath, const std::string& pointId);
    bool RestoreToSyncPoint(const std::string& archivePath, const std::string& pointId,
                           const std::string& outputPath);
    std::vector<SyncPoint> GetSyncPoints(const std::string& archivePath);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

class TimelineBackup {
public:
    struct TimelineEntry {
        std::string id;
        std::string archivePath;
        std::chrono::system_clock::time_point timestamp;
        std::string description;
        uint64_t size;
        uint32_t fileCount;
        std::string parentEntry;
    };

    struct TimelineInfo {
        std::vector<TimelineEntry> entries;
        uint64_t totalSize;
        uint32_t entryCount;
        std::chrono::system_clock::time_point oldestEntry;
        std::chrono::system_clock::time_point newestEntry;
    };

    TimelineBackup(SevenZipArchive& archive, const std::string& timelinePath);

    std::string CreateEntry(const std::string& sourcePath, const std::string& description = "",
                           const CompressionOptions& options = {});
    bool RestoreEntry(const std::string& entryId, const std::string& outputPath,
                     const std::string& password = "");
    bool DeleteEntry(const std::string& entryId);
    TimelineEntry* FindEntry(const std::string& entryId);
    TimelineInfo GetTimelineInfo();
    std::vector<TimelineEntry> GetEntriesInRange(std::chrono::system_clock::time_point start,
                                                  std::chrono::system_clock::time_point end);
    std::vector<TimelineEntry> GetEntriesByDescription(const std::string& keyword);
    bool PruneOldEntries(uint32_t maxEntries, uint32_t maxAgeDays = 0);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

class IntelligentClassifier {
public:
    enum class FileType { Document, Image, Video, Audio, Archive, Code, Data, Executable, Other };

    struct ClassificationResult {
        FileType type;
        std::string subType;
        double confidence;
        std::vector<std::string> tags;
        std::string description;
    };

    struct ArchiveClassification {
        std::map<FileType, uint32_t> typeCounts;
        std::map<FileType, uint64_t> typeSizes;
        FileType dominantType;
        std::vector<std::string> categories;
        std::string suggestedName;
    };

    IntelligentClassifier(SevenZipArchive& archive);

    ClassificationResult ClassifyFile(const std::string& filePath);
    ClassificationResult ClassifyByContent(const std::vector<uint8_t>& data,
                                           const std::string& extension);
    ArchiveClassification ClassifyArchive(const std::string& archivePath);
    std::vector<std::string> ExtractTags(const std::string& archivePath);
    std::string GenerateCategoryPath(const std::string& archivePath);
    bool OrganizeArchive(const std::string& archivePath, const std::string& outputDir);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

class VirusScannerInterface {
public:
    enum class ScanResult { Clean, Infected, Suspicious, Error, PasswordProtected };

    struct ThreatInfo {
        std::string filePath;
        std::string threatName;
        std::string threatType;
        uint32_t severity;
        std::string action;
    };

    struct ScanReport {
        ScanResult overallResult;
        uint32_t filesScanned;
        uint32_t threatsFound;
        uint32_t suspiciousFiles;
        uint64_t bytesScanned;
        std::chrono::seconds duration;
        std::vector<ThreatInfo> threats;
    };

    struct ScanOptions {
        bool scanArchives;
        bool heuristicsEnabled;
        bool scanMemory;
        uint32_t maxRecursionDepth;
        std::vector<std::string> excludePatterns;
        std::string password;
    };

    VirusScannerInterface(SevenZipArchive& archive);

    ScanReport ScanArchive(const std::string& archivePath, const ScanOptions& options);
    ScanResult ScanFile(const std::string& archivePath, const std::string& filePath,
                       ThreatInfo& threat);
    bool QuarantineFile(const std::string& archivePath, const std::string& filePath,
                       const std::string& quarantinePath);
    bool SetExternalScanner(const std::string& scannerPath);
    std::string GetScannerVersion();
    bool UpdateDefinitions();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

class ArchiveConverter {
public:
    struct ConversionOptions {
        ArchiveFormat targetFormat;
        CompressionMethod method;
        CompressionLevel level;
        bool preserveTimestamps;
        bool preserveAttributes;
        std::string password;
        std::string newPassword;
        uint32_t threadCount;
    };

    struct ConversionResult {
        bool success;
        uint64_t originalSize;
        uint64_t convertedSize;
        uint32_t filesConverted;
        std::string errorMessage;
    };

    ArchiveConverter(SevenZipArchive& archive);

    ConversionResult ConvertArchive(const std::string& sourcePath, const std::string& targetPath,
                                   const ConversionOptions& options);
    ConversionResult ConvertTo7z(const std::string& sourcePath, const std::string& targetPath,
                                CompressionLevel level = CompressionLevel::Normal);
    ConversionResult ConvertToZip(const std::string& sourcePath, const std::string& targetPath,
                                 CompressionLevel level = CompressionLevel::Normal);
    bool BatchConvert(const std::vector<std::string>& sources, const std::string& outputDir,
                     const ConversionOptions& options,
                     std::function<void(const std::string&, const ConversionResult&)> callback);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

class PasswordManager {
public:
    struct PasswordEntry {
        std::string id;
        std::string archivePath;
        std::string password;
        std::chrono::system_clock::time_point addedTime;
        std::chrono::system_clock::time_point lastUsedTime;
        uint32_t useCount;
    };

    struct PasswordPolicy {
        uint32_t minLength;
        bool requireUppercase;
        bool requireLowercase;
        bool requireNumbers;
        bool requireSymbols;
        uint32_t expirationDays;
    };

    PasswordManager();

    bool AddPassword(const std::string& archivePath, const std::string& password);
    bool RemovePassword(const std::string& archivePath);
    std::string GetPassword(const std::string& archivePath);
    std::vector<PasswordEntry> GetAllPasswords();
    std::string GeneratePassword(uint32_t length, const PasswordPolicy& policy);
    bool ValidatePassword(const std::string& password, const PasswordPolicy& policy);
    bool ExportPasswords(const std::string& exportPath, const std::string& masterPassword);
    bool ImportPasswords(const std::string& importPath, const std::string& masterPassword);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

class ArchiveValidator {
public:
    struct ValidationResult {
        bool isValid;
        bool headersValid;
        bool dataValid;
        bool checksumsValid;
        uint32_t corruptedFiles;
        uint64_t corruptedBytes;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
    };

    struct ValidationOptions {
        bool checkCRC;
        bool checkHeaders;
        bool extractTest;
        bool deepScan;
        uint32_t maxErrors;
    };

    ArchiveValidator(SevenZipArchive& archive);

    ValidationResult ValidateArchive(const std::string& archivePath,
                                    const ValidationOptions& options);
    bool QuickValidate(const std::string& archivePath);
    bool ValidateFile(const std::string& archivePath, const std::string& filePath);
    std::string GenerateChecksum(const std::string& archivePath, const std::string& algorithm);
    bool VerifyChecksum(const std::string& archivePath, const std::string& expectedChecksum,
                       const std::string& algorithm);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

}

#endif
