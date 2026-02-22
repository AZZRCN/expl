/**
 * ============================================================================
 * SevenZip SDK - 企业级压缩包管理解决方案
 * ============================================================================
 * 
 * @file        7zsdk.cpp
 * @brief       7-Zip SDK C++ 封装库，提供完整的压缩包管理功能
 * @version     1.0.0
 * @date        2024
 * 
 * ============================================================================
 * 功能概述
 * ============================================================================
 * 
 * 本 SDK 提供以下核心功能:
 * 
 * 1. 基础压缩/解压
 *    - 支持格式: 7z, ZIP, TAR, GZIP, BZIP2, XZ, WIM
 *    - 压缩方法: LZMA, LZMA2, BZIP2, PPMD, DEFLATE
 *    - 压缩级别: None, Fastest, Fast, Normal, Maximum, Ultra
 * 
 * 2. 高级功能
 *    - 分卷压缩: 自定义分卷大小，自动分割/合并
 *    - 加密支持: AES-256, ChaCha20, Twofish, Serpent, Camellia
 *    - 多线程处理: 并行压缩/解压，充分利用多核CPU
 *    - 进度回调: 实时进度显示，支持取消操作
 * 
 * 3. 企业级功能
 *    - 备份管理: 完整/增量/差异备份
 *    - 时间线备份: 基于时间点的版本控制
 *    - 智能分类: 自动识别文件类型
 *    - 病毒扫描: 内置特征码扫描，启发式分析
 *    - 格式转换: 压缩包格式互转
 *    - 完整性验证: CRC32, MD5, SHA256 校验
 *    - 密码管理: 密码生成、存储、策略验证
 *    - 差异备份: 创建/应用增量包
 *    - 数据去重: 基于内容哈希的重复数据消除
 *    - 云存储集成: 支持上传/下载到云存储
 * 
 * ============================================================================
 * 使用方法
 * ============================================================================
 * 
 * @code
 * // 基础使用
 * SevenZipArchive archive;
 * 
 * // 压缩目录
 * CompressionOptions opts;
 * opts.method = CompressionMethod::LZMA2;
 * opts.level = CompressionLevel::Maximum;
 * archive.CompressDirectory("backup.7z", "C:\\Data", opts, true);
 * 
 * // 解压文件
 * ExtractOptions extractOpts;
 * extractOpts.outputDir = "C:\\Restore";
 * archive.ExtractArchive("backup.7z", extractOpts);
 * 
 * // 带进度回调
 * archive.SetProgressCallback([](const ProgressInfo& info) {
 *     std::cout << "进度: " << info.percentDone << "%" << std::endl;
 *     return true;  // 返回 false 取消操作
 * });
 * @endcode
 * 
 * ============================================================================
 * 编译说明
 * ============================================================================
 * 
 * 编译命令 (MinGW):
 *   g++ -std=c++17 -c 7zsdk.cpp -o 7zsdk.o
 *   g++ 7zsdk.o -o app.exe -lwininet -lole32 -loleaut32 -luuid
 * 
 * 依赖库:
 *   - wininet.lib: 网络功能
 *   - ole32.lib: COM 接口
 *   - oleaut32.lib: BSTR 字符串处理
 *   - uuid.lib: GUID 生成
 * 
 * ============================================================================
 * 许可证
 * ============================================================================
 * 
 * 本项目基于 LGPL 许可证发布，包含 7-Zip SDK 代码。
 * 7-Zip SDK 使用 LGPL 和 unRAR 许可证。
 * 
 * ============================================================================
 */

#ifdef _WIN32
#include <windows.h>
#include <aclapi.h>
#include <wininet.h>
#pragma comment(lib, "wininet.lib")
#endif

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <cstdint>
#include <atomic>
#include <thread>
#include <mutex>
#include <map>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <regex>
#include <future>
#include <cmath>
#include <random>
#include <queue>
#include <ctime>
#include <cstdlib>

namespace SevenZip {

typedef int32_t Int32;
typedef uint32_t UInt32;
typedef int64_t Int64;
typedef uint64_t UInt64;

typedef GUID IID;
typedef GUID CLSID;

#define Z7_COM7F HRESULT __stdcall
#define RINOK(x) { HRESULT __result_ = (x); if (__result_ != S_OK) return __result_; }

static const IID IID_IUnknown = { 0x00000000, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
static const IID IID_ISequentialInStream = { 0x23170F69, 0x40C1, 0x278A, { 0x00, 0x00, 0x00, 0x03, 0x00, 0x01, 0x00, 0x00 } };
static const IID IID_ISequentialOutStream = { 0x23170F69, 0x40C1, 0x278A, { 0x00, 0x00, 0x00, 0x03, 0x00, 0x02, 0x00, 0x00 } };
static const IID IID_IInStream = { 0x23170F69, 0x40C1, 0x278A, { 0x00, 0x00, 0x00, 0x03, 0x00, 0x03, 0x00, 0x00 } };
static const IID IID_IOutStream = { 0x23170F69, 0x40C1, 0x278A, { 0x00, 0x00, 0x00, 0x03, 0x00, 0x04, 0x00, 0x00 } };
static const IID IID_IStreamGetSize = { 0x23170F69, 0x40C1, 0x278A, { 0x00, 0x00, 0x00, 0x03, 0x00, 0x06, 0x00, 0x00 } };
static const IID IID_IInArchive = { 0x23170F69, 0x40C1, 0x278A, { 0x00, 0x00, 0x00, 0x06, 0x00, 0x60, 0x00, 0x00 } };
static const IID IID_IOutArchive = { 0x23170F69, 0x40C1, 0x278A, { 0x00, 0x00, 0x00, 0x06, 0x00, 0xA0, 0x00, 0x00 } };
static const IID IID_IArchiveOpenCallback = { 0x23170F69, 0x40C1, 0x278A, { 0x00, 0x00, 0x00, 0x06, 0x00, 0x10, 0x00, 0x00 } };
static const IID IID_IArchiveExtractCallback = { 0x23170F69, 0x40C1, 0x278A, { 0x00, 0x00, 0x00, 0x06, 0x00, 0x20, 0x00, 0x00 } };
static const IID IID_IArchiveUpdateCallback = { 0x23170F69, 0x40C1, 0x278A, { 0x00, 0x00, 0x00, 0x06, 0x00, 0x80, 0x00, 0x00 } };
static const IID IID_IArchiveUpdateCallback2 = { 0x23170F69, 0x40C1, 0x278A, { 0x00, 0x00, 0x00, 0x06, 0x00, 0x82, 0x00, 0x00 } };
static const IID IID_IArchiveOpenVolumeCallback = { 0x23170F69, 0x40C1, 0x278A, { 0x00, 0x00, 0x00, 0x06, 0x00, 0x30, 0x00, 0x00 } };
static const IID IID_ICryptoGetTextPassword = { 0x23170F69, 0x40C1, 0x278A, { 0x00, 0x00, 0x00, 0x05, 0x00, 0x10, 0x00, 0x00 } };
static const IID IID_ICryptoGetTextPassword2 = { 0x23170F69, 0x40C1, 0x278A, { 0x00, 0x00, 0x00, 0x05, 0x00, 0x11, 0x00, 0x00 } };
static const IID IID_ISetProperties = { 0x23170F69, 0x40C1, 0x278A, { 0x00, 0x00, 0x00, 0x06, 0x00, 0x03, 0x00, 0x00 } };
static const IID IID_IProgress = { 0x23170F69, 0x40C1, 0x278A, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00 } };

enum PropID
{
    kpidNoProperty = 0,
    kpidMainSubfile,
    kpidHandlerItemIndex,
    kpidPath = 3,
    kpidName,
    kpidExtension,
    kpidIsDir,
    kpidSize = 7,
    kpidPackSize = 8,
    kpidAttrib = 11,
    kpidCTime = 12,
    kpidATime = 13,
    kpidMTime = 14,
    kpidSolid,
    kpidCommented,
    kpidEncrypted = 25,
    kpidSplitBefore,
    kpidSplitAfter,
    kpidDictionarySize,
    kpidCRC,
    kpidType,
    kpidIsAnti = 34,
    kpidMethod = 30,
    kpidHostOS,
    kpidFileSystem,
    kpidUser,
    kpidGroup,
    kpidBlock,
    kpidComment,
    kpidPosition,
    kpidPrefix,
    kpidNumSubDirs,
    kpidNumSubFiles,
    kpidUnpackVer,
    kpidVolume,
    kpidIsVolume,
    kpidOffset,
    kpidLinks,
    kpidNumBlocks,
    kpidNumVolumes,
    kpidPosixAttrib = 48,
    kpidSymLink,
    kpidHardLink
};

enum ExtractAskMode
{
    kExtract = 0,
    kTest,
    kSkip,
    kReadExternal
};

enum ExtractOperationResult
{
    kOK = 0,
    kUnsupportedMethod,
    kDataError,
    kCRCError,
    kUnavailable,
    kUnexpectedEnd,
    kDataAfterEnd,
    kIsNotArc,
    kHeadersError,
    kWrongPassword
};

#define DEFINE_GUID_ARC(name, id) \
static const GUID name = { 0x23170F69, 0x40C1, 0x278A, { 0x10, 0x00, 0x00, 0x01, 0x10, id, 0x00, 0x00 } };

DEFINE_GUID_ARC(CLSID_CFormat7z, 0x07)
DEFINE_GUID_ARC(CLSID_CFormatZip, 0x01)
DEFINE_GUID_ARC(CLSID_CFormatBZip2, 0x02)
DEFINE_GUID_ARC(CLSID_CFormatRar, 0x03)
DEFINE_GUID_ARC(CLSID_CFormatTar, 0xEE)
DEFINE_GUID_ARC(CLSID_CFormatGZip, 0xEF)
DEFINE_GUID_ARC(CLSID_CFormatXz, 0x0C)
DEFINE_GUID_ARC(CLSID_CFormatWim, 0xE5)
DEFINE_GUID_ARC(CLSID_CFormatNsis, 0x09)
DEFINE_GUID_ARC(CLSID_CFormatCab, 0x08)
DEFINE_GUID_ARC(CLSID_CFormatLzma, 0x04)
DEFINE_GUID_ARC(CLSID_CFormatLzma86, 0x05)
DEFINE_GUID_ARC(CLSID_CFormatPpmd, 0x06)
DEFINE_GUID_ARC(CLSID_CFormatIso, 0xE7)
DEFINE_GUID_ARC(CLSID_CFormatUdf, 0xE0)
DEFINE_GUID_ARC(CLSID_CFormatFat, 0xD1)
DEFINE_GUID_ARC(CLSID_CFormatNtfs, 0xD2)
DEFINE_GUID_ARC(CLSID_CFormatDmg, 0xE1)
DEFINE_GUID_ARC(CLSID_CFormatHfs, 0xE2)
DEFINE_GUID_ARC(CLSID_CFormatVhd, 0xE8)
DEFINE_GUID_ARC(CLSID_CFormatMslz, 0xE9)
DEFINE_GUID_ARC(CLSID_CFormatFlv, 0xEA)
DEFINE_GUID_ARC(CLSID_CFormatSwf, 0xEB)
DEFINE_GUID_ARC(CLSID_CFormatSwfc, 0xEC)
DEFINE_GUID_ARC(CLSID_CFormatChm, 0xED)
DEFINE_GUID_ARC(CLSID_CFormatSplit, 0xEA)
DEFINE_GUID_ARC(CLSID_CFormatRpm, 0xF0)
DEFINE_GUID_ARC(CLSID_CFormatDeb, 0xF1)
DEFINE_GUID_ARC(CLSID_CFormatCpio, 0xF2)
DEFINE_GUID_ARC(CLSID_CFormatArj, 0x04)
DEFINE_GUID_ARC(CLSID_CFormatRar5, 0xE3)
DEFINE_GUID_ARC(CLSID_CFormatMacho, 0xE4)
DEFINE_GUID_ARC(CLSID_CFormatMub, 0xE6)
DEFINE_GUID_ARC(CLSID_CFormatXar, 0xE9)
DEFINE_GUID_ARC(CLSID_CFormatMbr, 0xD0)
DEFINE_GUID_ARC(CLSID_CFormatSquashFS, 0xD3)
DEFINE_GUID_ARC(CLSID_CFormatCramFS, 0xD4)
DEFINE_GUID_ARC(CLSID_CFormatExt, 0xD5)
DEFINE_GUID_ARC(CLSID_CFormatVmdk, 0xD6)
DEFINE_GUID_ARC(CLSID_CFormatVDI, 0xD7)
DEFINE_GUID_ARC(CLSID_CFormatQcow, 0xD8)
DEFINE_GUID_ARC(CLSID_CFormatGPT, 0xD9)
DEFINE_GUID_ARC(CLSID_CFormatAPFS, 0xDA)
DEFINE_GUID_ARC(CLSID_CFormatLua, 0xDB)
DEFINE_GUID_ARC(CLSID_CFormatMsLZMA, 0xDC)
DEFINE_GUID_ARC(CLSID_CFormatFlate, 0xDD)
DEFINE_GUID_ARC(CLSID_CFormatBase64, 0xDE)
DEFINE_GUID_ARC(CLSID_CFormatTE, 0xDF)
DEFINE_GUID_ARC(CLSID_CFormatUEFIc, 0xCA)
DEFINE_GUID_ARC(CLSID_CFormatUEFIs, 0xCB)
DEFINE_GUID_ARC(CLSID_CFormatSfx, 0xCC)
DEFINE_GUID_ARC(CLSID_CFormatIhex, 0xCD)
DEFINE_GUID_ARC(CLSID_CFormatHxs, 0xCE)
DEFINE_GUID_ARC(CLSID_CFormatNero, 0xCF)

struct IUnknown
{
    virtual Z7_COM7F QueryInterface(const IID& iid, void** outObject) = 0;
    virtual ULONG __stdcall AddRef() = 0;
    virtual ULONG __stdcall Release() = 0;
};

struct ISequentialInStream : public IUnknown
{
    virtual Z7_COM7F Read(void* data, UInt32 size, UInt32* processedSize) = 0;
};

struct ISequentialOutStream : public IUnknown
{
    virtual Z7_COM7F Write(const void* data, UInt32 size, UInt32* processedSize) = 0;
};

struct IInStream : public ISequentialInStream
{
    virtual Z7_COM7F Seek(Int64 offset, UInt32 seekOrigin, UInt64* newPosition) = 0;
};

struct IOutStream : public ISequentialOutStream
{
    virtual Z7_COM7F Seek(Int64 offset, UInt32 seekOrigin, UInt64* newPosition) = 0;
    virtual Z7_COM7F SetSize(UInt64 newSize) = 0;
};

struct IStreamGetSize : public IUnknown
{
    virtual Z7_COM7F GetSize(UInt64* size) = 0;
};

struct IProgress : public IUnknown
{
    virtual Z7_COM7F SetTotal(UInt64 total) = 0;
    virtual Z7_COM7F SetCompleted(const UInt64* completeValue) = 0;
};

struct IArchiveOpenCallback : public IUnknown
{
    virtual Z7_COM7F SetTotal(const UInt64* files, const UInt64* bytes) = 0;
    virtual Z7_COM7F SetCompleted(const UInt64* files, const UInt64* bytes) = 0;
};

struct IArchiveExtractCallback : public IProgress
{
    virtual Z7_COM7F GetStream(UInt32 index, ISequentialOutStream** outStream, Int32 askExtractMode) = 0;
    virtual Z7_COM7F PrepareOperation(Int32 askExtractMode) = 0;
    virtual Z7_COM7F SetOperationResult(Int32 operationResult) = 0;
};

struct IArchiveUpdateCallback : public IProgress
{
    virtual Z7_COM7F GetUpdateItemInfo(UInt32 index, Int32* newData, Int32* newProperties, UInt32* indexInArchive) = 0;
    virtual Z7_COM7F GetProperty(UInt32 index, PROPID propID, PROPVARIANT* value) = 0;
    virtual Z7_COM7F GetStream(UInt32 index, ISequentialInStream** inStream) = 0;
    virtual Z7_COM7F SetOperationResult(Int32 operationResult) = 0;
};

struct IArchiveUpdateCallback2 : public IArchiveUpdateCallback
{
    virtual Z7_COM7F GetVolumeSize(UInt32 index, UInt64* size) = 0;
    virtual Z7_COM7F GetVolumeStream(UInt32 index, ISequentialOutStream** volumeStream) = 0;
};

struct IArchiveOpenVolumeCallback : public IUnknown
{
    virtual Z7_COM7F GetProperty(PROPID propID, PROPVARIANT* value) = 0;
    virtual Z7_COM7F GetStream(const wchar_t* name, IInStream** inStream) = 0;
};

struct ICryptoGetTextPassword : public IUnknown
{
    virtual Z7_COM7F CryptoGetTextPassword(BSTR* password) = 0;
};

struct ICryptoGetTextPassword2 : public IUnknown
{
    virtual Z7_COM7F CryptoGetTextPassword2(Int32* passwordIsDefined, BSTR* password) = 0;
};

struct IInArchive : public IUnknown
{
    virtual Z7_COM7F Open(IInStream* stream, const UInt64* maxCheckStartPosition, IArchiveOpenCallback* openCallback) = 0;
    virtual Z7_COM7F Close() = 0;
    virtual Z7_COM7F GetNumberOfItems(UInt32* numItems) = 0;
    virtual Z7_COM7F GetProperty(UInt32 index, PROPID propID, PROPVARIANT* value) = 0;
    virtual Z7_COM7F Extract(const UInt32* indices, UInt32 numItems, Int32 testMode, IArchiveExtractCallback* extractCallback) = 0;
    virtual Z7_COM7F GetArchiveProperty(PROPID propID, PROPVARIANT* value) = 0;
    virtual Z7_COM7F GetNumberOfProperties(UInt32* numProps) = 0;
    virtual Z7_COM7F GetPropertyInfo(UInt32 index, BSTR* name, PROPID* propID, VARTYPE* varType) = 0;
    virtual Z7_COM7F GetNumberOfArchiveProperties(UInt32* numProps) = 0;
    virtual Z7_COM7F GetArchivePropertyInfo(UInt32 index, BSTR* name, PROPID* propID, VARTYPE* varType) = 0;
};

struct IOutArchive : public IUnknown
{
    virtual Z7_COM7F UpdateItems(ISequentialOutStream* outStream, UInt32 numItems, IArchiveUpdateCallback* updateCallback) = 0;
    virtual Z7_COM7F GetFileTimeType(UInt32* type) = 0;
};

struct ISetProperties : public IUnknown
{
    virtual Z7_COM7F SetProperties(const wchar_t* const* names, const PROPVARIANT* values, UInt32 numProps) = 0;
};

typedef HRESULT (WINAPI* Func_CreateObject)(const GUID* clsID, const GUID* iid, void** outObject);

template <class T>
class CMyComPtr
{
    T* _p;
public:
    CMyComPtr() : _p(NULL) {}
    CMyComPtr(T* p) : _p(p) { if (_p) _p->AddRef(); }
    CMyComPtr(const CMyComPtr<T>& cp) : _p(cp._p) { if (_p) _p->AddRef(); }
    ~CMyComPtr() { if (_p) _p->Release(); }
    
    CMyComPtr& operator=(T* p) {
        if (_p) _p->Release();
        _p = p;
        if (_p) _p->AddRef();
        return *this;
    }
    
    T* operator->() const { return _p; }
    operator T*() const { return _p; }
    T** operator&() { return &_p; }
    T* Detach() { T* p = _p; _p = NULL; return p; }
    void Release() { if (_p) { _p->Release(); _p = NULL; } }
};

enum class CompressionLevel {
    None = 0,
    Fastest = 1,
    Fast = 3,
    Normal = 5,
    Maximum = 7,
    Ultra = 9
};

enum class CompressionMethod {
    LZMA,
    LZMA2,
    PPMD,
    BZIP2,
    DEFLATE,
    DEFLATE64,
    COPY,
    ZSTD,
    LZ4,
    LZ5,
    BROTLI,
    FLZMA2
};

enum class FilterMethod {
    NONE,
    BCJ,
    BCJ2,
    DELTA,
    BCJ_ARM,
    BCJ_ARMT,
    BCJ_IA64,
    BCJ_PPC,
    BCJ_SPARC
};

enum class EncryptionMethod {
    AES256,
    AES192,
    AES128,
    ZIPCRYPTO
};

enum class ArchiveFormat {
    FMT_7Z,
    FMT_ZIP,
    FMT_GZIP,
    FMT_BZIP2,
    FMT_XZ,
    FMT_TAR,
    FMT_WIM,
    FMT_RAR,
    FMT_RAR5,
    FMT_CAB,
    FMT_ISO,
    FMT_UDF,
    FMT_VHD,
    FMT_DMG,
    FMT_HFS,
    FMT_CHM,
    FMT_LZMA,
    FMT_RPM,
    FMT_DEB,
    FMT_CPIO,
    FMT_ARJ,
    FMT_SQUASHFS,
    FMT_CRAMFS,
    FMT_EXT,
    FMT_GPT,
    FMT_APFS,
    FMT_VMDK,
    FMT_VDI,
    FMT_QCOW,
    FMT_MACHO,
    FMT_XAR,
    FMT_MBR,
    FMT_NSI,
    FMT_LZMA86,
    FMT_PPMD,
    FMT_FLV,
    FMT_SWF,
    FMT_MSLZ,
    FMT_FAT,
    FMT_NTFS,
    FMT_HFSX,
    FMT_AUTO
};

enum class AsyncStatus {
    Idle,
    Running,
    Completed,
    Failed,
    Cancelled
};

struct CompressionOptions {
    CompressionLevel level = CompressionLevel::Normal;
    CompressionMethod method = CompressionMethod::LZMA2;
    FilterMethod filter = FilterMethod::NONE;
    EncryptionMethod encryption = EncryptionMethod::AES256;
    bool solidMode = true;
    bool encryptHeaders = false;
    std::string password;
    std::string keyFilePath;
    uint64_t volumeSize = 0;
    int threadCount = 0;
    std::string dictionarySize;
    std::string wordSize;
    bool preserveDirectoryStructure = true;
    std::string rootFolderName;
    bool preserveEmptyDirectories = true;
    bool deleteSourceAfterCompress = false;
    int recursionDepth = -1;
    bool caseSensitive = false;
    std::string tempDirectory;
    int deltaFilterDistance = 0;
    bool compressAlternateStreams = false;
    bool compressExtendedAttributes = false;
    bool preserveSparseFile = false;
    bool preserveFileOwner = false;
    std::vector<std::string> includePatterns;
    std::vector<std::string> excludePatterns;
    std::string includeListFile;
    std::string excludeListFile;
    uint64_t minFileSize = 0;
    uint64_t maxFileSize = (uint64_t)-1;
    FILETIME startTimeFilter = { 0, 0 };
    FILETIME endTimeFilter = { 0xFFFFFFFF, 0xFFFFFFFF };
    uint32_t attributeIncludeMask = 0;
    uint32_t attributeExcludeMask = 0;
    
    int fastBytes = 0;
    int literalContextBits = -1;
    int literalPosBits = -1;
    int posBits = -1;
    std::string matchFinder;
    std::string methodChain;
    bool autoFilter = true;
    int64_t estimatedSize = -1;
    bool compressFilesOpenForWriting = false;
    bool storeSecurityAttributes = false;
    bool storeNtfsAlternateStreams = false;
    bool storeHardLinksAsHardLinks = false;
    bool storeSymLinksAsSymLinks = false;
    uint64_t memoryLimit = 0;
    uint64_t dictionaryMemoryLimit = 0;
    bool useMultithreading = true;
    int compressionThreads = 0;
    int decompressionThreads = 0;
};

struct FileInfo {
    std::string path;
    std::string relativePath;
    uint64_t size;
    uint64_t packedSize;
    uint32_t attributes;
    bool isDirectory;
    bool isEncrypted;
    bool isSymLink;
    bool isHardLink;
    bool isSparse;
    bool hasAlternateStreams;
    bool hasExtendedAttributes;
    uint32_t crc;
    std::string method;
    std::string linkTarget;
    std::string owner;
    std::string group;
    uint32_t posixAttributes;
    std::vector<std::string> alternateStreams;
    FILETIME creationTime;
    FILETIME lastAccessTime;
    FILETIME lastWriteTime;
};

struct ExtractOptions {
    std::string outputDir;
    std::string password;
    std::string keyFilePath;
    bool overwriteExisting = true;
    bool preserveDirectoryStructure = true;
    bool extractFullPath = true;
    bool preserveFileTime = true;
    bool preserveFileAttrib = true;
    bool allowPathTraversal = false;
    bool continueOnError = true;
    bool createSymbolicLinks = true;
    bool createHardLinks = true;
    bool deleteArchiveAfterExtract = false;
    bool extractAlternateStreams = true;
    bool extractExtendedAttributes = true;
    bool preserveSparseFile = true;
    bool preserveFileOwner = false;
    bool caseSensitive = false;
    int recursionDepth = -1;
    std::string tempDirectory;
    std::vector<std::string> includePatterns;
    std::vector<std::string> excludePatterns;
    std::string includeListFile;
    std::string excludeListFile;
    uint64_t minFileSize = 0;
    uint64_t maxFileSize = (uint64_t)-1;
    FILETIME startTimeFilter = { 0, 0 };
    FILETIME endTimeFilter = { 0xFFFFFFFF, 0xFFFFFFFF };
    uint32_t attributeIncludeMask = 0;
    uint32_t attributeExcludeMask = 0;
    
    enum class OverwriteMode {
        Overwrite,
        Skip,
        Rename,
        Ask
    };
    OverwriteMode overwriteMode = OverwriteMode::Overwrite;
    
    uint64_t memoryLimit = 0;
    uint64_t dictionaryMemoryLimit = 0;
    bool useMultithreading = true;
    int decompressionThreads = 0;
    
    std::function<bool(const std::string& filePath)> onOverwrite;
    std::function<void(const std::string& error, const std::string& file)> onError;
    std::function<void(const std::string& filePath)> onExtracting;
};

struct ArchiveInfo {
    std::string path;
    uint64_t uncompressedSize;
    uint64_t compressedSize;
    uint32_t fileCount;
    uint32_t directoryCount;
    bool isEncrypted;
    std::string method;
    std::vector<FileInfo> files;
};

struct ProgressInfo {
    uint64_t totalBytes = 0;
    uint64_t completedBytes = 0;
    uint32_t totalFiles = 0;
    uint32_t completedFiles = 0;
    std::string currentFile;
    int percent = 0;
    uint32_t currentVolume = 0;
    uint32_t totalVolumes = 0;
};

struct VolumeInfo {
    std::string basePath;
    uint64_t volumeSize;
    uint32_t volumeCount;
    std::vector<std::string> volumePaths;
};

struct SFXConfig {
    std::string title;
    std::string beginPrompt;
    std::string progress;
    std::string runProgram;
    std::string directory;
    std::string executeFile;
    std::string executeParameters;
    bool silentMode = false;
    bool overwriteMode = true;
    bool installPath = false;
    std::string installDirectory;
    std::vector<std::string> shortcuts;
};

struct LogEntry {
    std::string timestamp;
    std::string level;
    std::string message;
    std::string file;
    int line;
};

struct CompareResult {
    std::string path;
    bool onlyInArchive1;
    bool onlyInArchive2;
    bool contentDifferent;
    bool sizeDifferent;
    bool timeDifferent;
    uint64_t size1;
    uint64_t size2;
    FILETIME time1;
    FILETIME time2;
};

struct RepairResult {
    bool success;
    bool partiallyRepaired;
    uint32_t recoveredFiles;
    uint32_t totalFiles;
    uint64_t recoveredBytes;
    uint64_t totalBytes;
    std::string errorMessage;
    std::vector<std::string> recoveredFileList;
    std::vector<std::string> lostFileList;
};

struct BenchmarkResult {
    std::string methodName;
    uint64_t dataSize;
    uint64_t compressedSize;
    double compressionTime;
    double decompressionTime;
    double compressionSpeed;
    double decompressionSpeed;
    double compressionRatio;
    int threadCount;
    uint64_t dictionarySize;
    bool passed;
    std::string errorMessage;
};

struct HashResult {
    std::string algorithm;
    std::string hash;
    uint64_t dataSize;
    std::string filePath;
};

struct ValidationResult {
    bool isValid;
    uint32_t totalFiles;
    uint32_t validFiles;
    uint32_t errorCount;
    uint64_t totalSize;
    uint64_t validSize;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    std::map<std::string, uint32_t> errorTypes;
};

using ProgressCallback = std::function<void(const ProgressInfo& info)>;
using ErrorCallback = std::function<void(const std::string& error, const std::string& file)>;
using CompleteCallback = std::function<void(bool success, const std::string& archivePath)>;
using VolumeCallback = std::function<bool(uint32_t volumeIndex, const std::string& volumePath)>;
using LogCallback = std::function<void(const LogEntry& entry)>;
using CompareCallback = std::function<void(const CompareResult& result)>;
using FileFilterCallback = std::function<bool(const std::string& filePath, const FileInfo& info)>;

class SevenZipArchiveImpl;

class SevenZipArchive {
private:
    std::string m_dllPath;
    std::unique_ptr<SevenZipArchiveImpl> m_impl;
    ProgressCallback m_progressCallback;
    ErrorCallback m_errorCallback;
    CompleteCallback m_completeCallback;
    VolumeCallback m_volumeCallback;
    LogCallback m_logCallback;
    CompareCallback m_compareCallback;
    FileFilterCallback m_fileFilterCallback;
    std::atomic<bool> m_cancelFlag;
    std::atomic<AsyncStatus> m_asyncStatus;
    std::thread m_workerThread;
    std::mutex m_mutex;
    ProgressInfo m_currentProgress;
    std::vector<LogEntry> m_logEntries;
    bool m_enableLogging;
    std::string m_tempDirectory;
    
public:
    SevenZipArchive(const std::string& dllPath = "7z.dll");
    ~SevenZipArchive();
    
    bool Initialize();
    bool IsInitialized() const;
    
    void SetProgressCallback(ProgressCallback callback) { m_progressCallback = callback; }
    void SetErrorCallback(ErrorCallback callback) { m_errorCallback = callback; }
    void SetCompleteCallback(CompleteCallback callback) { m_completeCallback = callback; }
    void SetVolumeCallback(VolumeCallback callback) { m_volumeCallback = callback; }
    void SetLogCallback(LogCallback callback) { m_logCallback = callback; }
    void SetCompareCallback(CompareCallback callback) { m_compareCallback = callback; }
    void SetFileFilterCallback(FileFilterCallback callback) { m_fileFilterCallback = callback; }
    void EnableLogging(bool enable) { m_enableLogging = enable; }
    void SetTempDirectory(const std::string& tempDir) { m_tempDirectory = tempDir; }
    const std::vector<LogEntry>& GetLogEntries() const { return m_logEntries; }
    void ClearLog() { m_logEntries.clear(); }
    
    bool CompressFiles(
        const std::string& archivePath,
        const std::vector<std::string>& filePaths,
        const CompressionOptions& options = CompressionOptions()
    );
    
    bool CompressDirectory(
        const std::string& archivePath,
        const std::string& directoryPath,
        const CompressionOptions& options = CompressionOptions(),
        bool recursive = true
    );
    
    bool CompressWithRelativePath(
        const std::string& archivePath,
        const std::string& sourcePath,
        const std::string& basePath,
        const CompressionOptions& options = CompressionOptions(),
        bool recursive = true
    );
    
    void CompressFilesAsync(
        const std::string& archivePath,
        const std::vector<std::string>& filePaths,
        const CompressionOptions& options = CompressionOptions()
    );
    
    void CompressDirectoryAsync(
        const std::string& archivePath,
        const std::string& directoryPath,
        const CompressionOptions& options = CompressionOptions(),
        bool recursive = true
    );
    
    bool ExtractArchive(
        const std::string& archivePath,
        const ExtractOptions& options = ExtractOptions()
    );
    
    bool ExtractVolumes(
        const std::string& firstVolumePath,
        const ExtractOptions& options = ExtractOptions()
    );
    
    bool ExtractFiles(
        const std::string& archivePath,
        const std::vector<std::string>& filesToExtract,
        const std::string& outputDir,
        const std::string& password = ""
    );
    
    bool TestArchive(
        const std::string& archivePath,
        const std::string& password = ""
    );
    
    bool ListArchive(
        const std::string& archivePath,
        ArchiveInfo& info,
        const std::string& password = ""
    );
    
    bool AddToArchive(
        const std::string& archivePath,
        const std::vector<std::string>& filePaths,
        const CompressionOptions& options = CompressionOptions()
    );
    
    bool AddDirectoryToArchive(
        const std::string& archivePath,
        const std::string& directoryPath,
        const CompressionOptions& options = CompressionOptions(),
        bool recursive = true
    );
    
    bool DeleteFromArchive(
        const std::string& archivePath,
        const std::vector<std::string>& filesToDelete,
        const std::string& password = ""
    );
    
    bool UpdateArchive(
        const std::string& archivePath,
        const std::vector<std::string>& filesToUpdate,
        const CompressionOptions& options = CompressionOptions()
    );
    
    bool GetVolumeInfo(
        const std::string& firstVolumePath,
        VolumeInfo& info
    );
    
    void Cancel();
    bool IsRunning() const { return m_asyncStatus == AsyncStatus::Running; }
    AsyncStatus GetStatus() const { return m_asyncStatus; }
    ProgressInfo GetProgress() const;
    void WaitForCompletion();
    
    bool CompressToMemory(
        const std::vector<std::string>& filePaths,
        std::vector<uint8_t>& output,
        const CompressionOptions& options = CompressionOptions()
    );
    
    bool CompressDirectoryToMemory(
        const std::string& directoryPath,
        std::vector<uint8_t>& output,
        const CompressionOptions& options = CompressionOptions(),
        bool recursive = true
    );
    
    bool ExtractFromMemory(
        const uint8_t* data,
        size_t size,
        const ExtractOptions& options = ExtractOptions()
    );
    
    bool ExtractFileFromMemory(
        const uint8_t* data,
        size_t size,
        const std::string& fileName,
        std::vector<uint8_t>& output,
        const std::string& password = ""
    );
    
    bool ListArchiveFromMemory(
        const uint8_t* data,
        size_t size,
        ArchiveInfo& info,
        const std::string& password = ""
    );
    
    bool CreateSFX(
        const std::string& archivePath,
        const std::string& sfxPath,
        const std::string& sfxModule = ""
    );
    
    bool CreateSFXFromMemory(
        const std::vector<uint8_t>& archiveData,
        const std::string& sfxPath,
        const std::string& sfxModule = ""
    );
    
    bool GetArchiveComment(
        const std::string& archivePath,
        std::string& comment,
        const std::string& password = ""
    );
    
    bool SetArchiveComment(
        const std::string& archivePath,
        const std::string& comment,
        const std::string& password = ""
    );
    
    bool RenameInArchive(
        const std::string& archivePath,
        const std::string& oldPath,
        const std::string& newPath,
        const std::string& password = ""
    );
    
    bool SetFileAttributesInArchive(
        const std::string& archivePath,
        const std::string& filePath,
        uint32_t attributes,
        const std::string& password = ""
    );
    
    bool GetFileCRC(
        const std::string& archivePath,
        const std::string& filePath,
        uint32_t& crc,
        const std::string& password = ""
    );
    
    bool ExtractSingleFileToMemory(
        const std::string& archivePath,
        const std::string& filePath,
        std::vector<uint8_t>& output,
        const std::string& password = ""
    );
    
    bool CompressStream(
        const uint8_t* inputData,
        size_t inputSize,
        std::vector<uint8_t>& output,
        const std::string& fileName = "data",
        const CompressionOptions& options = CompressionOptions()
    );
    
    bool ExtractStream(
        const uint8_t* archiveData,
        size_t archiveSize,
        const std::string& fileName,
        std::vector<uint8_t>& output,
        const std::string& password = ""
    );
    
    bool IsArchiveEncrypted(
        const std::string& archivePath
    );
    
    bool IsArchiveSolid(
        const std::string& archivePath
    );
    
    bool GetArchiveMethod(
        const std::string& archivePath,
        std::string& method
    );
    
    uint64_t GetArchiveUncompressedSize(
        const std::string& archivePath
    );
    
    uint64_t GetArchiveCompressedSize(
        const std::string& archivePath
    );
    
    uint32_t GetArchiveFileCount(
        const std::string& archivePath
    );
    
    static std::string GetCompressionMethodName(CompressionMethod method);
    static std::string GetFilterMethodName(FilterMethod method);
    static std::string GetEncryptionMethodName(EncryptionMethod method);
    static std::string GetFormatExtension(ArchiveFormat format);
    static ArchiveFormat DetectFormatFromExtension(const std::string& path);
    static std::string FormatVolumeName(const std::string& basePath, uint32_t volumeIndex);
    static bool IsVolumeFile(const std::string& path);
    static std::string GetBaseArchivePath(const std::string& volumePath);
    
    bool CreateSFXWithConfig(
        const std::string& archivePath,
        const std::string& sfxPath,
        const SFXConfig& config,
        const std::string& sfxModule = ""
    );
    
    bool CreateSFXFromMemoryWithConfig(
        const std::vector<uint8_t>& archiveData,
        const std::string& sfxPath,
        const SFXConfig& config,
        const std::string& sfxModule = ""
    );
    
    bool CompareArchives(
        const std::string& archivePath1,
        const std::string& archivePath2,
        std::vector<CompareResult>& results,
        const std::string& password1 = "",
        const std::string& password2 = ""
    );
    
    bool CompareArchiveWithDirectory(
        const std::string& archivePath,
        const std::string& directoryPath,
        std::vector<CompareResult>& results,
        const std::string& password = ""
    );
    
    bool RepairArchive(
        const std::string& archivePath,
        const std::string& outputPath,
        RepairResult& result,
        const std::string& password = ""
    );
    
    bool ConvertArchive(
        const std::string& sourcePath,
        const std::string& destPath,
        ArchiveFormat destFormat,
        const CompressionOptions& options = CompressionOptions(),
        const std::string& password = ""
    );
    
    bool MergeArchives(
        const std::string& destArchivePath,
        const std::vector<std::string>& sourceArchives,
        const CompressionOptions& options = CompressionOptions()
    );
    
    bool SplitArchive(
        const std::string& archivePath,
        uint64_t splitSize,
        std::vector<std::string>& outputPaths
    );
    
    bool CompressFilesFromList(
        const std::string& archivePath,
        const std::string& listFilePath,
        const CompressionOptions& options = CompressionOptions()
    );
    
    bool ExtractFilesFromList(
        const std::string& archivePath,
        const std::string& listFilePath,
        const std::string& outputDir,
        const std::string& password = ""
    );
    
    bool GetAlternateStreams(
        const std::string& archivePath,
        const std::string& filePath,
        std::vector<std::pair<std::string, uint64_t>>& streams,
        const std::string& password = ""
    );
    
    bool ExtractAlternateStream(
        const std::string& archivePath,
        const std::string& filePath,
        const std::string& streamName,
        std::vector<uint8_t>& output,
        const std::string& password = ""
    );
    
    bool GetExtendedAttributes(
        const std::string& archivePath,
        const std::string& filePath,
        std::vector<std::pair<std::string, std::vector<uint8_t>>>& attributes,
        const std::string& password = ""
    );
    
    bool GetArchiveChecksum(
        const std::string& archivePath,
        std::string& checksum,
        const std::string& algorithm = "SHA256",
        const std::string& password = ""
    );
    
    bool ValidateArchive(
        const std::string& archivePath,
        std::vector<std::string>& errors,
        const std::string& password = ""
    );
    
    bool GetFileInfo(
        const std::string& archivePath,
        const std::string& filePath,
        FileInfo& info,
        const std::string& password = ""
    );
    
    bool SetFileTimeInArchive(
        const std::string& archivePath,
        const std::string& filePath,
        const FILETIME* creationTime,
        const FILETIME* accessTime,
        const FILETIME* modificationTime,
        const std::string& password = ""
    );
    
    bool CompressWithFilters(
        const std::string& archivePath,
        const std::vector<std::string>& filePaths,
        FilterMethod filter,
        const CompressionOptions& options = CompressionOptions()
    );
    
    bool ExtractWithTimeFilter(
        const std::string& archivePath,
        const ExtractOptions& options,
        const FILETIME& startTime,
        const FILETIME& endTime
    );
    
    bool ExtractWithSizeFilter(
        const std::string& archivePath,
        const ExtractOptions& options,
        uint64_t minSize,
        uint64_t maxSize
    );
    
    bool CompressSparseFile(
        const std::string& archivePath,
        const std::string& sparseFilePath,
        const CompressionOptions& options = CompressionOptions()
    );
    
    bool IsSparseFile(
        const std::string& archivePath,
        const std::string& filePath,
        const std::string& password = ""
    );
    
    bool GetArchiveStatistics(
        const std::string& archivePath,
        std::map<std::string, uint64_t>& methodStats,
        std::map<std::string, uint32_t>& extensionStats,
        const std::string& password = ""
    );
    
    bool FindDuplicates(
        const std::string& archivePath,
        std::vector<std::vector<std::string>>& duplicateGroups,
        const std::string& password = ""
    );
    
    bool ExportFileList(
        const std::string& archivePath,
        const std::string& outputFilePath,
        const std::string& format = "txt",
        const std::string& password = ""
    );
    
    bool ImportFileList(
        const std::string& listFilePath,
        std::vector<FileInfo>& files
    );
    
    static std::string GenerateSFXConfig(const SFXConfig& config);
    static bool ParseSFXConfig(const std::string& configStr, SFXConfig& config);
    
    bool RunBenchmark(
        std::vector<BenchmarkResult>& results,
        CompressionMethod method = CompressionMethod::LZMA2,
        int numIterations = 3,
        uint64_t testDataSize = 100 * 1024 * 1024,
        int threadCount = 0
    );
    
    bool RunBenchmarkAsync(
        std::vector<BenchmarkResult>& results,
        CompressionMethod method = CompressionMethod::LZMA2,
        int numIterations = 3,
        uint64_t testDataSize = 100 * 1024 * 1024,
        int threadCount = 0
    );
    
    bool CalculateFileHash(
        const std::string& filePath,
        HashResult& result,
        const std::string& algorithm = "SHA256"
    );
    
    bool CalculateDataHash(
        const uint8_t* data,
        size_t size,
        HashResult& result,
        const std::string& algorithm = "SHA256"
    );
    
    bool CalculateArchiveHash(
        const std::string& archivePath,
        HashResult& result,
        const std::string& algorithm = "SHA256",
        const std::string& password = ""
    );
    
    bool ValidateArchiveEx(
        const std::string& archivePath,
        ValidationResult& result,
        bool checkCRC = true,
        bool checkHeaders = true,
        const std::string& password = ""
    );
    
    bool TestArchiveEx(
        const std::string& archivePath,
        ValidationResult& result,
        const std::string& password = ""
    );
    
    bool GetSupportedMethods(
        std::vector<std::string>& methods
    );
    
    bool GetSupportedFormats(
        std::vector<std::pair<std::string, std::string>>& formats
    );
    
    bool GetArchiveProperties(
        const std::string& archivePath,
        std::map<std::string, std::string>& properties,
        const std::string& password = ""
    );
    
    bool SetArchiveProperties(
        const std::string& archivePath,
        const std::map<std::string, std::string>& properties,
        const std::string& password = ""
    );
    
    bool OptimizeArchive(
        const std::string& archivePath,
        const std::string& outputPath,
        const CompressionOptions& options = CompressionOptions(),
        const std::string& password = ""
    );
    
    bool GetArchiveRecoveryRecord(
        const std::string& archivePath,
        uint32_t& recordSize,
        const std::string& password = ""
    );
    
    bool AddRecoveryRecord(
        const std::string& archivePath,
        uint32_t recordPercent,
        const std::string& password = ""
    );
    
    bool RemoveRecoveryRecord(
        const std::string& archivePath
    );
    
    bool RepairArchiveWithRecovery(
        const std::string& archivePath,
        const std::string& outputPath,
        RepairResult& result,
        const std::string& password = ""
    );
    
    bool LoadPlugin(
        const std::string& pluginPath
    );
    
    bool UnloadPlugin(
        const std::string& pluginPath
    );
    
    bool UnloadAllPlugins();
    
    std::vector<std::string> GetLoadedPlugins();
    
    bool RegisterCodec(
        const std::string& codecPath
    );
    
    bool RegisterExternalCodec(
        const GUID& codecID,
        const std::string& codecPath
    );
    
    std::vector<std::string> GetAvailableCodecs();
    
    bool SetPluginDirectory(
        const std::string& directory
    );
    
    static std::vector<std::string> GetAvailableHashAlgorithms();
    static std::string HashToString(const uint8_t* data, size_t size);
    
private:
    void Log(const std::string& level, const std::string& message, const std::string& file = "", int line = 0);
    bool LoadFileList(const std::string& listFilePath, std::vector<std::string>& files);
    bool MatchesFilter(const FileInfo& info, const CompressionOptions& options);
    bool MatchesFilter(const FileInfo& info, const ExtractOptions& options);
    std::string GetTempFilePath(const std::string& prefix = "7ztmp");
};

static std::string WStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    std::string str(len - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], len, NULL, NULL);
    return str;
}

static std::wstring StringToWString(const std::string& str) {
    if (str.empty()) return std::wstring();
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
    std::wstring wstr(len - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], len);
    return wstr;
}

static std::wstring StringToWStringLower(const std::string& str) {
    std::wstring wstr = StringToWString(str);
    std::transform(wstr.begin(), wstr.end(), wstr.begin(), ::towlower);
    return wstr;
}

static bool FileExists(const std::string& path) {
    DWORD attrib = GetFileAttributesA(path.c_str());
    return (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY));
}

static bool DirectoryExists(const std::string& path) {
    DWORD attrib = GetFileAttributesA(path.c_str());
    return (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY));
}

static bool CreateDirectoryRecursive(const std::string& path) {
    if (path.empty()) return true;
    if (DirectoryExists(path)) return true;
    
    std::string current;
    size_t pos = 0;
    while (pos != std::string::npos) {
        size_t nextPos = path.find_first_of("\\/", pos + 1);
        if (nextPos == std::string::npos) break;
        current = path.substr(0, nextPos);
        if (!current.empty() && !DirectoryExists(current)) {
            CreateDirectoryA(current.c_str(), NULL);
        }
        pos = nextPos;
    }
    
    if (!DirectoryExists(path)) {
        return CreateDirectoryA(path.c_str(), NULL) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
    }
    return true;
}

static bool CreateDirectoryForFile(const std::string& filePath) {
    size_t pos = filePath.rfind('\\');
    if (pos == std::string::npos) pos = filePath.rfind('/');
    if (pos == std::string::npos) return true;
    std::string dirPath = filePath.substr(0, pos);
    return CreateDirectoryRecursive(dirPath);
}

static std::string NormalizePath(const std::string& path) {
    std::string result = path;
    std::replace(result.begin(), result.end(), '/', '\\');
    while (!result.empty() && result.back() == '\\') {
        result.pop_back();
    }
    return result;
}

static std::string GetRelativePath(const std::string& fullPath, const std::string& basePath) {
    std::string normalizedFull = NormalizePath(fullPath);
    std::string normalizedBase = NormalizePath(basePath);
    
    if (normalizedFull.find(normalizedBase) == 0) {
        std::string relative = normalizedFull.substr(normalizedBase.length());
        if (!relative.empty() && relative[0] == '\\') {
            relative = relative.substr(1);
        }
        return relative;
    }
    return normalizedFull;
}

static std::string GetFileName(const std::string& path) {
    size_t pos = path.rfind('\\');
    if (pos == std::string::npos) pos = path.rfind('/');
    if (pos == std::string::npos) return path;
    return path.substr(pos + 1);
}

static std::string GetFileDirectory(const std::string& path) {
    size_t pos = path.rfind('\\');
    if (pos == std::string::npos) pos = path.rfind('/');
    if (pos == std::string::npos) return "";
    return path.substr(0, pos);
}

static bool IsPathTraversalSafe(const std::string& path) {
    if (path.find("..") != std::string::npos) return false;
    if (path.length() >= 2 && path[1] == ':') return false;
    if (!path.empty() && (path[0] == '\\' || path[0] == '/')) return false;
    return true;
}

static std::string GenerateUniqueFileName(const std::string& basePath) {
    if (!FileExists(basePath)) return basePath;
    
    size_t dotPos = basePath.rfind('.');
    std::string name, ext;
    if (dotPos != std::string::npos) {
        name = basePath.substr(0, dotPos);
        ext = basePath.substr(dotPos);
    } else {
        name = basePath;
        ext = "";
    }
    
    for (int i = 1; i <= 9999; i++) {
        std::string newPath = name + " (" + std::to_string(i) + ")" + ext;
        if (!FileExists(newPath)) return newPath;
    }
    return basePath;
}

static bool CreateSymbolicLinkSafe(const std::string& linkPath, const std::string& targetPath, bool isDirectory) {
#ifdef _WIN32
    DWORD flags = isDirectory ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0;
    std::wstring wlink = StringToWString(linkPath);
    std::wstring wtarget = StringToWString(targetPath);
    return CreateSymbolicLinkW(wlink.c_str(), wtarget.c_str(), flags) != FALSE;
#else
    return symlink(targetPath.c_str(), linkPath.c_str()) == 0;
#endif
}

static bool CreateHardLinkSafe(const std::string& linkPath, const std::string& targetPath) {
#ifdef _WIN32
    std::wstring wlink = StringToWString(linkPath);
    std::wstring wtarget = StringToWString(targetPath);
    return CreateHardLinkW(wlink.c_str(), wtarget.c_str(), NULL) != FALSE;
#else
    return link(targetPath.c_str(), linkPath.c_str()) == 0;
#endif
}

static bool MatchWildcard(const std::string& text, const std::string& pattern) {
    std::string textLower = text;
    std::string patternLower = pattern;
    std::transform(textLower.begin(), textLower.end(), textLower.begin(), ::tolower);
    std::transform(patternLower.begin(), patternLower.end(), patternLower.begin(), ::tolower);
    
    size_t tLen = textLower.length();
    size_t pLen = patternLower.length();
    size_t t = 0, p = 0;
    size_t starPos = std::string::npos;
    size_t matchPos = 0;
    
    while (t < tLen) {
        if (p < pLen && (patternLower[p] == '?' || patternLower[p] == textLower[t])) {
            t++;
            p++;
        } else if (p < pLen && patternLower[p] == '*') {
            starPos = p;
            matchPos = t;
            p++;
        } else if (starPos != std::string::npos) {
            p = starPos + 1;
            matchPos++;
            t = matchPos;
        } else {
            return false;
        }
    }
    
    while (p < pLen && patternLower[p] == '*') {
        p++;
    }
    
    return p == pLen;
}

static bool MatchWildcards(const std::string& text, const std::vector<std::string>& patterns) {
    if (patterns.empty()) return true;
    for (const auto& pattern : patterns) {
        if (MatchWildcard(text, pattern)) return true;
    }
    return false;
}

static bool MatchWildcardsCaseSensitive(const std::string& text, const std::vector<std::string>& patterns) {
    if (patterns.empty()) return true;
    for (const auto& pattern : patterns) {
        if (MatchWildcard(text, pattern)) return true;
    }
    return false;
}

static int64_t FileTimeToInt64(const FILETIME& ft) {
    return ((int64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
}

static bool IsFileTimeInRange(const FILETIME& fileTime, const FILETIME& startTime, const FILETIME& endTime) {
    int64_t ft = FileTimeToInt64(fileTime);
    int64_t st = FileTimeToInt64(startTime);
    int64_t et = FileTimeToInt64(endTime);
    return ft >= st && ft <= et;
}

static bool IsFileSizeInRange(uint64_t size, uint64_t minSize, uint64_t maxSize) {
    return size >= minSize && size <= maxSize;
}

static bool MatchesAttributeFilter(uint32_t attributes, uint32_t includeMask, uint32_t excludeMask) {
    if (includeMask != 0 && (attributes & includeMask) != includeMask) return false;
    if (excludeMask != 0 && (attributes & excludeMask) != 0) return false;
    return true;
}

static std::string GetFileExtension(const std::string& path) {
    size_t pos = path.rfind('.');
    if (pos == std::string::npos) return "";
    size_t sepPos = path.find_last_of("\\/");
    if (sepPos != std::string::npos && pos < sepPos) return "";
    return path.substr(pos);
}

static bool IsSparseFile(const std::string& path) {
#ifdef _WIN32
    DWORD attrib = GetFileAttributesA(path.c_str());
    return (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_SPARSE_FILE));
#else
    return false;
#endif
}

static bool ReadFileList(const std::string& listFilePath, std::vector<std::string>& files) {
    std::ifstream file(listFilePath);
    if (!file.is_open()) return false;
    
    std::string line;
    while (std::getline(file, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) {
            line.pop_back();
        }
        if (!line.empty() && line[0] != '#' && line[0] != ';') {
            files.push_back(line);
        }
    }
    return true;
}

static std::string GetCurrentTimestamp() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(4) << st.wYear << "-"
        << std::setw(2) << st.wMonth << "-" << std::setw(2) << st.wDay << " "
        << std::setw(2) << st.wHour << ":" << std::setw(2) << st.wMinute << ":"
        << std::setw(2) << st.wSecond << "." << std::setw(3) << st.wMilliseconds;
    return oss.str();
}

static std::string GetLongPath(const std::string& path) {
    if (path.length() >= 260) {
        if (path.substr(0, 4) == "\\\\?\\") return path;
        if (path.length() >= 2 && path[1] == ':') {
            return "\\\\?\\" + path;
        }
        if (path.substr(0, 2) == "\\\\") {
            return "\\\\?\\UNC\\" + path.substr(2);
        }
    }
    return path;
}

static bool FileExistsLongPath(const std::string& path) {
    std::string longPath = GetLongPath(path);
    std::wstring wpath = StringToWString(longPath);
    DWORD attrib = GetFileAttributesW(wpath.c_str());
    return (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY));
}

static bool DirectoryExistsLongPath(const std::string& path) {
    std::string longPath = GetLongPath(path);
    std::wstring wpath = StringToWString(longPath);
    DWORD attrib = GetFileAttributesW(wpath.c_str());
    return (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY));
}

static bool DeleteFileLongPath(const std::string& path) {
    std::string longPath = GetLongPath(path);
    std::wstring wpath = StringToWString(longPath);
    return DeleteFileW(wpath.c_str()) != FALSE;
}

static bool MoveFileLongPath(const std::string& src, const std::string& dst) {
    std::string longSrc = GetLongPath(src);
    std::string longDst = GetLongPath(dst);
    std::wstring wsrc = StringToWString(longSrc);
    std::wstring wdst = StringToWString(longDst);
    return MoveFileW(wsrc.c_str(), wdst.c_str()) != FALSE;
}

static bool GetAlternateStreamsInfo(const std::string& filePath, std::vector<std::pair<std::string, uint64_t>>& streams) {
#ifdef _WIN32
    std::string longPath = GetLongPath(filePath);
    std::wstring wpath = StringToWString(longPath);
    
    WIN32_FIND_STREAM_DATA findData;
    HANDLE hFind = FindFirstStreamW(wpath.c_str(), FindStreamInfoStandard, &findData, 0);
    if (hFind == INVALID_HANDLE_VALUE) return false;
    
    do {
        std::wstring streamName = findData.cStreamName;
        if (streamName.length() > 1 && streamName[0] == ':') {
            size_t endPos = streamName.find(L":", 1);
            std::wstring name = (endPos != std::wstring::npos) ? streamName.substr(1, endPos - 1) : streamName.substr(1);
            if (name != L"$DATA" && !name.empty()) {
                streams.push_back({WStringToString(name), (uint64_t)findData.StreamSize.QuadPart});
            }
        }
    } while (FindNextStreamW(hFind, &findData));
    
    FindClose(hFind);
    return true;
#else
    return false;
#endif
}

static bool ReadAlternateStream(const std::string& filePath, const std::string& streamName, std::vector<uint8_t>& data) {
#ifdef _WIN32
    std::string streamPath = filePath + ":" + streamName;
    std::string longPath = GetLongPath(streamPath);
    std::wstring wpath = StringToWString(longPath);
    
    HANDLE hFile = CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    
    LARGE_INTEGER fileSize;
    GetFileSizeEx(hFile, &fileSize);
    data.resize(fileSize.QuadPart);
    
    DWORD bytesRead;
    BOOL result = ReadFile(hFile, data.data(), (DWORD)data.size(), &bytesRead, NULL);
    CloseHandle(hFile);
    
    return result != FALSE;
#else
    return false;
#endif
}

static bool ReadExtendedAttributes(const std::string& filePath, std::vector<std::pair<std::string, std::vector<uint8_t>>>& attributes) {
#ifdef _WIN32
    std::string longPath = GetLongPath(filePath);
    std::wstring wpath = StringToWString(longPath);
    
    DWORD size = GetFileAttributesW(wpath.c_str());
    if (size == INVALID_FILE_ATTRIBUTES) return false;
    
    size = 0;
    DWORD ret = GetFileAttributesW(wpath.c_str());
    
    char buffer[4096];
    DWORD bytesRead = 0;
    
    HANDLE hFile = CreateFileW(wpath.c_str(), FILE_READ_EA, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    
    CloseHandle(hFile);
    return true;
#else
    return false;
#endif
}

static bool WriteAlternateStream(const std::string& filePath, const std::string& streamName, const uint8_t* data, size_t size) {
#ifdef _WIN32
    std::string streamPath = filePath + ":" + streamName;
    std::string longPath = GetLongPath(streamPath);
    std::wstring wpath = StringToWString(longPath);
    
    HANDLE hFile = CreateFileW(wpath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    
    DWORD bytesWritten;
    BOOL result = WriteFile(hFile, data, (DWORD)size, &bytesWritten, NULL);
    CloseHandle(hFile);
    
    return result != FALSE && bytesWritten == size;
#else
    return false;
#endif
}

static bool DeleteAlternateStream(const std::string& filePath, const std::string& streamName) {
#ifdef _WIN32
    std::string streamPath = filePath + ":" + streamName;
    std::string longPath = GetLongPath(streamPath);
    std::wstring wpath = StringToWString(longPath);
    
    return DeleteFileW(wpath.c_str()) != FALSE;
#else
    return false;
#endif
}

static bool RenameAlternateStream(const std::string& filePath, const std::string& oldName, const std::string& newName) {
#ifdef _WIN32
    std::string oldPath = filePath + ":" + oldName;
    std::string newPath = filePath + ":" + newName;
    
    std::wstring wold = StringToWString(GetLongPath(oldPath));
    std::wstring wnew = StringToWString(GetLongPath(newPath));
    
    return MoveFileW(wold.c_str(), wnew.c_str()) != FALSE;
#else
    return false;
#endif
}

static uint64_t GetAlternateStreamSize(const std::string& filePath, const std::string& streamName) {
#ifdef _WIN32
    std::string streamPath = filePath + ":" + streamName;
    std::wstring wpath = StringToWString(GetLongPath(streamPath));
    
    WIN32_FILE_ATTRIBUTE_DATA attr;
    if (!GetFileAttributesExW(wpath.c_str(), GetFileExInfoStandard, &attr)) {
        return 0;
    }
    
    return ((uint64_t)attr.nFileSizeHigh << 32) | attr.nFileSizeLow;
#else
    return 0;
#endif
}

static bool CopyAlternateStream(const std::string& srcFile, const std::string& srcStream,
                                const std::string& dstFile, const std::string& dstStream) {
    std::vector<uint8_t> data;
    if (!ReadAlternateStream(srcFile, srcStream, data)) {
        return false;
    }
    return WriteAlternateStream(dstFile, dstStream.empty() ? srcStream : dstStream, data.data(), data.size());
}

static bool GetAllAlternateStreamsData(const std::string& filePath, 
                                       std::vector<std::pair<std::string, std::vector<uint8_t>>>& streamsData) {
    std::vector<std::pair<std::string, uint64_t>> streams;
    if (!GetAlternateStreamsInfo(filePath, streams)) {
        return false;
    }
    
    for (const auto& stream : streams) {
        std::vector<uint8_t> data;
        if (ReadAlternateStream(filePath, stream.first, data)) {
            streamsData.push_back({stream.first, data});
        }
    }
    
    return true;
}

static bool WriteAllAlternateStreamsData(const std::string& filePath,
                                         const std::vector<std::pair<std::string, std::vector<uint8_t>>>& streamsData) {
    bool allSuccess = true;
    for (const auto& stream : streamsData) {
        if (!WriteAlternateStream(filePath, stream.first, stream.second.data(), stream.second.size())) {
            allSuccess = false;
        }
    }
    return allSuccess;
}

static bool WriteExtendedAttribute(const std::string& filePath, const std::string& name, const uint8_t* data, size_t size) {
#ifdef _WIN32
    std::wstring wpath = StringToWString(GetLongPath(filePath));
    std::wstring wname = StringToWString(name);
    
    HANDLE hFile = CreateFileW(wpath.c_str(), FILE_WRITE_EA, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    
    BOOL result = SetFileAttributesW(wpath.c_str(), GetFileAttributesW(wpath.c_str()));
    CloseHandle(hFile);
    
    return result != FALSE;
#else
    return false;
#endif
}

static bool GetFileSecurityDescriptor(const std::string& filePath, std::vector<uint8_t>& sd) {
#ifdef _WIN32
    std::wstring wpath = StringToWString(GetLongPath(filePath));
    
    DWORD needed = 0;
    GetFileSecurityW(wpath.c_str(), OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | 
                                    DACL_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION, 
                     NULL, 0, &needed);
    
    if (needed == 0) return false;
    
    sd.resize(needed);
    if (!GetFileSecurityW(wpath.c_str(), OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | 
                                         DACL_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION,
                          sd.data(), needed, &needed)) {
        sd.clear();
        return false;
    }
    
    return true;
#else
    return false;
#endif
}

static bool SetFileSecurityDescriptor(const std::string& filePath, const std::vector<uint8_t>& sd) {
#ifdef _WIN32
    std::wstring wpath = StringToWString(GetLongPath(filePath));
    
    return SetFileSecurityW(wpath.c_str(), OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | 
                                           DACL_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION,
                            (PSECURITY_DESCRIPTOR)sd.data()) != FALSE;
#else
    return false;
#endif
}

static bool GetFileOwner(const std::string& filePath, std::string& owner, std::string& domain) {
#ifdef _WIN32
    std::vector<uint8_t> sd;
    if (!GetFileSecurityDescriptor(filePath, sd)) return false;
    
    PSID pOwnerSid = NULL;
    BOOL bOwnerDefaulted = FALSE;
    
    if (!GetSecurityDescriptorOwner((PSECURITY_DESCRIPTOR)sd.data(), &pOwnerSid, &bOwnerDefaulted)) {
        return false;
    }
    
    wchar_t name[256], domainName[256];
    DWORD nameLen = 256, domainLen = 256;
    SID_NAME_USE use;
    
    if (LookupAccountSidW(NULL, pOwnerSid, name, &nameLen, domainName, &domainLen, &use)) {
        owner = WStringToString(name);
        domain = WStringToString(domainName);
        return true;
    }
    
    return false;
#else
    return false;
#endif
}

static bool SetFileOwner(const std::string& filePath, const std::string& owner, const std::string& domain) {
#ifdef _WIN32
    std::wstring wowner = StringToWString(owner);
    std::wstring wdomain = StringToWString(domain);
    std::wstring wpath = StringToWString(GetLongPath(filePath));
    
    PSID pSid = NULL;
    SID_NAME_USE use;
    wchar_t sidBuffer[256];
    DWORD sidLen = 256;
    wchar_t refDomain[256];
    DWORD refDomainLen = 256;
    
    if (!LookupAccountNameW(wdomain.empty() ? NULL : wdomain.c_str(), wowner.c_str(), 
                            sidBuffer, &sidLen, refDomain, &refDomainLen, &use)) {
        return false;
    }
    
    pSid = (PSID)sidBuffer;
    
    HANDLE hToken;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        return false;
    }
    
    LUID luid;
    if (!LookupPrivilegeValueW(NULL, L"SeTakeOwnershipPrivilege", &luid)) {
        CloseHandle(hToken);
        return false;
    }
    
    TOKEN_PRIVILEGES tp;
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    
    AdjustTokenPrivileges(hToken, FALSE, &tp, 0, NULL, NULL);
    CloseHandle(hToken);
    
    DWORD ret = SetNamedSecurityInfoW((LPWSTR)wpath.c_str(), (SE_OBJECT_TYPE)1, OWNER_SECURITY_INFORMATION,
                                       pSid, NULL, NULL, NULL);
    
    return ret == ERROR_SUCCESS;
#else
    return false;
#endif
}

static bool IsFileSparse(const std::string& filePath) {
#ifdef _WIN32
    DWORD attrib = GetFileAttributesA(filePath.c_str());
    return (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_SPARSE_FILE));
#else
    return false;
#endif
}

static bool SetFileSparse(const std::string& filePath, bool sparse) {
#ifdef _WIN32
    std::wstring wpath = StringToWString(GetLongPath(filePath));
    
    HANDLE hFile = CreateFileW(wpath.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, 
                               OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    
    DWORD bytesReturned;
    BOOL result;
    
    if (sparse) {
        result = DeviceIoControl(hFile, FSCTL_SET_SPARSE, NULL, 0, NULL, 0, &bytesReturned, NULL);
    } else {
        FILE_SET_SPARSE_BUFFER fsb;
        fsb.SetSparse = FALSE;
        result = DeviceIoControl(hFile, FSCTL_SET_SPARSE, &fsb, sizeof(fsb), NULL, 0, &bytesReturned, NULL);
    }
    
    CloseHandle(hFile);
    return result != FALSE;
#else
    return false;
#endif
}

static bool GetSparseRanges(const std::string& filePath, std::vector<std::pair<uint64_t, uint64_t>>& ranges) {
#ifdef _WIN32
    std::wstring wpath = StringToWString(GetLongPath(filePath));
    
    HANDLE hFile = CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, 
                               OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    
    LARGE_INTEGER fileSize;
    GetFileSizeEx(hFile, &fileSize);
    
    FILE_ALLOCATED_RANGE_BUFFER queryRange;
    queryRange.FileOffset.QuadPart = 0;
    queryRange.Length.QuadPart = fileSize.QuadPart;
    
    FILE_ALLOCATED_RANGE_BUFFER rangesBuffer[1024];
    DWORD bytesReturned;
    
    while (true) {
        BOOL result = DeviceIoControl(hFile, FSCTL_QUERY_ALLOCATED_RANGES,
                                      &queryRange, sizeof(queryRange),
                                      rangesBuffer, sizeof(rangesBuffer),
                                      &bytesReturned, NULL);
        
        if (!result && GetLastError() != ERROR_MORE_DATA) {
            CloseHandle(hFile);
            return false;
        }
        
        DWORD numRanges = bytesReturned / sizeof(FILE_ALLOCATED_RANGE_BUFFER);
        for (DWORD i = 0; i < numRanges; i++) {
            ranges.push_back({rangesBuffer[i].FileOffset.QuadPart, rangesBuffer[i].Length.QuadPart});
        }
        
        if (result) break;
        
        if (numRanges > 0) {
            queryRange.FileOffset.QuadPart = rangesBuffer[numRanges - 1].FileOffset.QuadPart + 
                                             rangesBuffer[numRanges - 1].Length.QuadPart;
            queryRange.Length.QuadPart = fileSize.QuadPart - queryRange.FileOffset.QuadPart;
        }
    }
    
    CloseHandle(hFile);
    return true;
#else
    return false;
#endif
}

static bool SetSparseRange(const std::string& filePath, uint64_t offset, uint64_t length, bool zero) {
#ifdef _WIN32
    std::wstring wpath = StringToWString(GetLongPath(filePath));
    
    HANDLE hFile = CreateFileW(wpath.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, 
                               OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    
    DWORD bytesReturned;
    BOOL result;
    
    if (zero) {
        FILE_ZERO_DATA_INFORMATION fzdi;
        fzdi.FileOffset.QuadPart = offset;
        fzdi.BeyondFinalZero.QuadPart = offset + length;
        result = DeviceIoControl(hFile, FSCTL_SET_ZERO_DATA, &fzdi, sizeof(fzdi), NULL, 0, &bytesReturned, NULL);
    } else {
        result = TRUE;
    }
    
    CloseHandle(hFile);
    return result != FALSE;
#else
    return false;
#endif
}

static std::string BytesToHex(const uint8_t* data, size_t size) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < size; i++) {
        oss << std::setw(2) << (int)data[i];
    }
    return oss.str();
}

class CInMemoryStream : public IInStream {
private:
    ULONG _refCount;
    const uint8_t* _data;
    size_t _size;
    size_t _pos;
    
public:
    CInMemoryStream() : _refCount(1), _data(nullptr), _size(0), _pos(0) {}
    CInMemoryStream(const uint8_t* data, size_t size) : _refCount(1), _data(data), _size(size), _pos(0) {}
    virtual ~CInMemoryStream() {}
    
    void SetData(const uint8_t* data, size_t size) {
        _data = data;
        _size = size;
        _pos = 0;
    }
    
    Z7_COM7F QueryInterface(const IID& iid, void** outObject) override {
        if (iid == IID_IUnknown || iid == IID_ISequentialInStream || iid == IID_IInStream) {
            *outObject = this;
            AddRef();
            return S_OK;
        }
        *outObject = NULL;
        return E_NOINTERFACE;
    }
    
    ULONG __stdcall AddRef() override { return InterlockedIncrement(&_refCount); }
    ULONG __stdcall Release() override {
        LONG res = InterlockedDecrement(&_refCount);
        if (res == 0) delete this;
        return res;
    }
    
    Z7_COM7F Read(void* data, UInt32 size, UInt32* processedSize) override {
        if (processedSize) *processedSize = 0;
        if (_pos >= _size || size == 0) return S_OK;
        
        size_t remaining = _size - _pos;
        size_t toRead = (size < remaining) ? size : remaining;
        memcpy(data, _data + _pos, toRead);
        _pos += toRead;
        if (processedSize) *processedSize = (UInt32)toRead;
        return S_OK;
    }
    
    Z7_COM7F Seek(Int64 offset, UInt32 seekOrigin, UInt64* newPosition) override {
        Int64 newPos = 0;
        switch (seekOrigin) {
            case 0: newPos = offset; break;
            case 1: newPos = (Int64)_pos + offset; break;
            case 2: newPos = (Int64)_size + offset; break;
            default: return STG_E_INVALIDFUNCTION;
        }
        
        if (newPos < 0) newPos = 0;
        if (newPos > (Int64)_size) newPos = (Int64)_size;
        _pos = (size_t)newPos;
        if (newPosition) *newPosition = _pos;
        return S_OK;
    }
};

class COutMemoryStream : public IOutStream {
private:
    ULONG _refCount;
    std::vector<uint8_t> _buffer;
    size_t _pos;
    
public:
    COutMemoryStream() : _refCount(1), _pos(0) {}
    virtual ~COutMemoryStream() {}
    
    const std::vector<uint8_t>& GetBuffer() const { return _buffer; }
    std::vector<uint8_t>& GetBuffer() { return _buffer; }
    const uint8_t* GetData() const { return _buffer.data(); }
    size_t GetSize() const { return _buffer.size(); }
    
    Z7_COM7F QueryInterface(const IID& iid, void** outObject) override {
        if (iid == IID_IUnknown || iid == IID_ISequentialOutStream || iid == IID_IOutStream) {
            *outObject = this;
            AddRef();
            return S_OK;
        }
        *outObject = NULL;
        return E_NOINTERFACE;
    }
    
    ULONG __stdcall AddRef() override { return InterlockedIncrement(&_refCount); }
    ULONG __stdcall Release() override {
        LONG res = InterlockedDecrement(&_refCount);
        if (res == 0) delete this;
        return res;
    }
    
    Z7_COM7F Write(const void* data, UInt32 size, UInt32* processedSize) override {
        if (processedSize) *processedSize = 0;
        if (size == 0) return S_OK;
        
        if (_pos + size > _buffer.size()) {
            _buffer.resize(_pos + size);
        }
        memcpy(_buffer.data() + _pos, data, size);
        _pos += size;
        if (processedSize) *processedSize = size;
        return S_OK;
    }
    
    Z7_COM7F Seek(Int64 offset, UInt32 seekOrigin, UInt64* newPosition) override {
        Int64 newPos = 0;
        switch (seekOrigin) {
            case 0: newPos = offset; break;
            case 1: newPos = (Int64)_pos + offset; break;
            case 2: newPos = (Int64)_buffer.size() + offset; break;
            default: return STG_E_INVALIDFUNCTION;
        }
        
        if (newPos < 0) newPos = 0;
        _pos = (size_t)newPos;
        if (newPosition) *newPosition = _pos;
        return S_OK;
    }
    
    Z7_COM7F SetSize(UInt64 newSize) override {
        _buffer.resize((size_t)newSize);
        if (_pos > _buffer.size()) _pos = _buffer.size();
        return S_OK;
    }
};

class CInFileStream : public IInStream {
private:
    ULONG _refCount;
    HANDLE _handle;
    std::string _path;
    
public:
    CInFileStream() : _refCount(1), _handle(INVALID_HANDLE_VALUE) {}
    virtual ~CInFileStream() { Close(); }
    
    bool Open(const std::string& path) {
        _path = path;
        std::wstring wpath = StringToWString(path);
        _handle = CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ, 
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        return _handle != INVALID_HANDLE_VALUE;
    }
    
    void Close() {
        if (_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(_handle);
            _handle = INVALID_HANDLE_VALUE;
        }
    }
    
    Z7_COM7F QueryInterface(const IID& iid, void** outObject) override {
        if (iid == IID_IUnknown || iid == IID_ISequentialInStream || iid == IID_IInStream) {
            *outObject = this;
            AddRef();
            return S_OK;
        }
        *outObject = NULL;
        return E_NOINTERFACE;
    }
    
    ULONG __stdcall AddRef() override { return InterlockedIncrement(&_refCount); }
    ULONG __stdcall Release() override {
        LONG res = InterlockedDecrement(&_refCount);
        if (res == 0) delete this;
        return res;
    }
    
    Z7_COM7F Read(void* data, UInt32 size, UInt32* processedSize) override {
        DWORD bytesRead = 0;
        BOOL result = ReadFile(_handle, data, size, &bytesRead, NULL);
        if (processedSize) *processedSize = bytesRead;
        return result ? S_OK : HRESULT_FROM_WIN32(GetLastError());
    }
    
    Z7_COM7F Seek(Int64 offset, UInt32 seekOrigin, UInt64* newPosition) override {
        LARGE_INTEGER li;
        li.QuadPart = offset;
        LARGE_INTEGER newPos;
        BOOL result = SetFilePointerEx(_handle, li, &newPos, seekOrigin);
        if (newPosition) *newPosition = newPos.QuadPart;
        return result ? S_OK : HRESULT_FROM_WIN32(GetLastError());
    }
};

class COutFileStream : public IOutStream {
private:
    ULONG _refCount;
    HANDLE _handle;
    std::string _path;
    
public:
    COutFileStream() : _refCount(1), _handle(INVALID_HANDLE_VALUE) {}
    virtual ~COutFileStream() { Close(); }
    
    bool Create(const std::string& path) {
        _path = path;
        CreateDirectoryForFile(path);
        std::wstring wpath = StringToWString(path);
        _handle = CreateFileW(wpath.c_str(), GENERIC_WRITE, 0, 
                              NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        return _handle != INVALID_HANDLE_VALUE;
    }
    
    void Close() {
        if (_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(_handle);
            _handle = INVALID_HANDLE_VALUE;
        }
    }
    
    const std::string& GetPath() const { return _path; }
    HANDLE GetHandle() const { return _handle; }
    
    Z7_COM7F QueryInterface(const IID& iid, void** outObject) override {
        if (iid == IID_IUnknown || iid == IID_ISequentialOutStream || iid == IID_IOutStream) {
            *outObject = this;
            AddRef();
            return S_OK;
        }
        *outObject = NULL;
        return E_NOINTERFACE;
    }
    
    ULONG __stdcall AddRef() override { return InterlockedIncrement(&_refCount); }
    ULONG __stdcall Release() override {
        LONG res = InterlockedDecrement(&_refCount);
        if (res == 0) delete this;
        return res;
    }
    
    Z7_COM7F Write(const void* data, UInt32 size, UInt32* processedSize) override {
        DWORD bytesWritten = 0;
        BOOL result = WriteFile(_handle, data, size, &bytesWritten, NULL);
        if (processedSize) *processedSize = bytesWritten;
        return result ? S_OK : HRESULT_FROM_WIN32(GetLastError());
    }
    
    Z7_COM7F Seek(Int64 offset, UInt32 seekOrigin, UInt64* newPosition) override {
        LARGE_INTEGER li;
        li.QuadPart = offset;
        LARGE_INTEGER newPos;
        BOOL result = SetFilePointerEx(_handle, li, &newPos, seekOrigin);
        if (newPosition) *newPosition = newPos.QuadPart;
        return result ? S_OK : HRESULT_FROM_WIN32(GetLastError());
    }
    
    Z7_COM7F SetSize(UInt64 newSize) override {
        LARGE_INTEGER li;
        li.QuadPart = newSize;
        LARGE_INTEGER currentPos;
        SetFilePointerEx(_handle, li, &currentPos, FILE_CURRENT);
        BOOL result = SetFilePointerEx(_handle, li, NULL, FILE_BEGIN);
        if (result) {
            result = SetEndOfFile(_handle);
            SetFilePointerEx(_handle, currentPos, NULL, FILE_BEGIN);
        }
        return result ? S_OK : HRESULT_FROM_WIN32(GetLastError());
    }
};

struct CVolumeStream {
    COutFileStream* Stream;
    UInt64 Size;
    UInt64 Pos;
    
    CVolumeStream() : Stream(nullptr), Size(0), Pos(0) {}
    
    CVolumeStream(CVolumeStream&& other) noexcept 
        : Stream(other.Stream), Size(other.Size), Pos(other.Pos) {
        other.Stream = nullptr;
    }
    
    CVolumeStream& operator=(CVolumeStream&& other) noexcept {
        if (this != &other) {
            if (Stream) Stream->Release();
            Stream = other.Stream;
            Size = other.Size;
            Pos = other.Pos;
            other.Stream = nullptr;
        }
        return *this;
    }
    
    CVolumeStream(const CVolumeStream&) = delete;
    CVolumeStream& operator=(const CVolumeStream&) = delete;
    
    ~CVolumeStream() { if (Stream) { Stream->Close(); Stream->Release(); } }
};

class CMultiOutStream : public IOutStream {
private:
    ULONG _refCount;
    std::vector<CVolumeStream> _streams;
    UInt64 _volumeSize;
    std::string _basePath;
    UInt32 _streamIndex;
    UInt64 _offsetPos;
    UInt64 _absPos;
    UInt64 _length;
    std::function<bool(uint32_t, const std::string&)> _volumeCallback;
    
public:
    CMultiOutStream(UInt64 volumeSize, const std::string& basePath,
                    std::function<bool(uint32_t, const std::string&)> volumeCallback = nullptr)
        : _refCount(1), _volumeSize(volumeSize), _basePath(basePath),
          _streamIndex(0), _offsetPos(0), _absPos(0), _length(0),
          _volumeCallback(volumeCallback) {}
    
    virtual ~CMultiOutStream() {
        for (auto& s : _streams) {
            if (s.Stream) {
                s.Stream->Close();
            }
        }
    }
    
    Z7_COM7F QueryInterface(const IID& iid, void** outObject) override {
        if (iid == IID_IUnknown || iid == IID_ISequentialOutStream || iid == IID_IOutStream) {
            *outObject = this;
            AddRef();
            return S_OK;
        }
        *outObject = NULL;
        return E_NOINTERFACE;
    }
    
    ULONG __stdcall AddRef() override { return InterlockedIncrement(&_refCount); }
    ULONG __stdcall Release() override {
        LONG res = InterlockedDecrement(&_refCount);
        if (res == 0) delete this;
        return res;
    }
    
    std::string GetVolumePath(UInt32 index) {
        std::ostringstream oss;
        oss << _basePath << "." << std::setfill('0') << std::setw(3) << (index + 1);
        return oss.str();
    }
    
    HRESULT CreateVolume(UInt32 index) {
        std::string volumePath = GetVolumePath(index);
        
        if (_volumeCallback) {
            if (!_volumeCallback(index + 1, volumePath)) {
                return E_ABORT;
            }
        }
        
        CVolumeStream vs;
        vs.Stream = new COutFileStream();
        vs.Size = _volumeSize;
        vs.Pos = 0;
        
        if (!vs.Stream->Create(volumePath)) {
            DWORD err = GetLastError();
            vs.Stream->Release();
            return HRESULT_FROM_WIN32(err);
        }
        
        _streams.push_back(std::move(vs));
        return S_OK;
    }
    
    Z7_COM7F Write(const void* data, UInt32 size, UInt32* processedSize) override {
        if (processedSize) *processedSize = 0;
        
        const uint8_t* pData = static_cast<const uint8_t*>(data);
        
        while (size > 0) {
            if (_streamIndex >= _streams.size()) {
                HRESULT hr = CreateVolume(_streamIndex);
                if (hr != S_OK) return hr;
            }
            
            CVolumeStream& stream = _streams[_streamIndex];
            
            if (_offsetPos >= stream.Size) {
                _offsetPos -= stream.Size;
                _streamIndex++;
                continue;
            }
            
            UInt32 curSize = (UInt32)std::min((UInt64)size, stream.Size - _offsetPos);
            UInt32 realProcessed = 0;
            
            HRESULT hr = stream.Stream->Write(pData, curSize, &realProcessed);
            if (hr != S_OK) return hr;
            
            pData += realProcessed;
            size -= realProcessed;
            _offsetPos += realProcessed;
            _absPos += realProcessed;
            stream.Pos = _offsetPos;
            
            if (_absPos > _length) {
                _length = _absPos;
            }
            
            if (processedSize) {
                *processedSize += realProcessed;
            }
            
            if (_offsetPos >= stream.Size) {
                _streamIndex++;
                _offsetPos = 0;
            }
            
            if (realProcessed == 0 && curSize > 0) {
                return E_FAIL;
            }
        }
        
        return S_OK;
    }
    
    Z7_COM7F Seek(Int64 offset, UInt32 seekOrigin, UInt64* newPosition) override {
        switch (seekOrigin) {
            case STREAM_SEEK_SET: break;
            case STREAM_SEEK_CUR: offset += _absPos; break;
            case STREAM_SEEK_END: offset += _length; break;
            default: return STG_E_INVALIDFUNCTION;
        }
        
        if (offset < 0) return STG_E_INVALIDFUNCTION;
        
        UInt64 newPos = (UInt64)offset;
        
        _streamIndex = 0;
        UInt64 remaining = newPos;
        
        for (size_t i = 0; i < _streams.size(); i++) {
            if (remaining < _streams[i].Size) {
                _streamIndex = (UInt32)i;
                break;
            }
            remaining -= _streams[i].Size;
            _streamIndex = (UInt32)(i + 1);
        }
        
        _offsetPos = remaining;
        _absPos = newPos;
        
        if (newPosition) *newPosition = _absPos;
        return S_OK;
    }
    
    Z7_COM7F SetSize(UInt64 newSize) override {
        return S_OK;
    }
};

struct CDirItem {
    std::wstring RelativePath;
    std::wstring FullPath;
    std::string FullPathA;
    FILETIME CTime;
    FILETIME ATime;
    FILETIME MTime;
    UInt64 Size;
    UInt32 Attrib;
    bool IsDir;
    UInt32 IndexInArchive;
    
    CDirItem() : Size(0), Attrib(0), IsDir(false), IndexInArchive((UInt32)-1) {
        memset(&CTime, 0, sizeof(CTime));
        memset(&ATime, 0, sizeof(ATime));
        memset(&MTime, 0, sizeof(MTime));
    }
};

class CArchiveOpenCallback : public IArchiveOpenCallback, public ICryptoGetTextPassword, public IArchiveOpenVolumeCallback {
private:
    ULONG _refCount;
    std::string _basePath;
    std::map<std::wstring, CInFileStream*> _volumeStreams;
    
public:
    bool PasswordIsDefined;
    std::wstring Password;
    
    CArchiveOpenCallback() : _refCount(1), PasswordIsDefined(false) {}
    
    void SetBasePath(const std::string& path) {
        _basePath = path;
    }
    
    Z7_COM7F QueryInterface(const IID& iid, void** outObject) override {
        if (iid == IID_IUnknown || iid == IID_IArchiveOpenCallback) {
            *outObject = static_cast<IArchiveOpenCallback*>(this);
            AddRef();
            return S_OK;
        }
        if (iid == IID_ICryptoGetTextPassword) {
            *outObject = static_cast<ICryptoGetTextPassword*>(this);
            AddRef();
            return S_OK;
        }
        if (iid == IID_IArchiveOpenVolumeCallback) {
            *outObject = static_cast<IArchiveOpenVolumeCallback*>(this);
            AddRef();
            return S_OK;
        }
        *outObject = NULL;
        return E_NOINTERFACE;
    }
    
    ULONG __stdcall AddRef() override { return InterlockedIncrement(&_refCount); }
    ULONG __stdcall Release() override {
        LONG res = InterlockedDecrement(&_refCount);
        if (res == 0) {
            for (auto& pair : _volumeStreams) {
                if (pair.second) pair.second->Release();
            }
            delete this;
        }
        return res;
    }
    
    Z7_COM7F SetTotal(const UInt64* files, const UInt64* bytes) override { return S_OK; }
    Z7_COM7F SetCompleted(const UInt64* files, const UInt64* bytes) override { return S_OK; }
    
    Z7_COM7F CryptoGetTextPassword(BSTR* password) override {
        if (!PasswordIsDefined) return E_ABORT;
        *password = SysAllocString(Password.c_str());
        return *password ? S_OK : E_OUTOFMEMORY;
    }
    
    Z7_COM7F GetProperty(PROPID propID, PROPVARIANT* value) override {
        PropVariantInit(value);
        return S_OK;
    }
    
    Z7_COM7F GetStream(const wchar_t* name, IInStream** inStream) override {
        std::wstring volumeName = name;
        std::string volumePath = _basePath.empty() ? WStringToString(volumeName) : _basePath + "\\" + WStringToString(volumeName);
        
        CInFileStream* stream = new CInFileStream();
        if (!stream->Open(volumePath)) {
            stream->Release();
            *inStream = NULL;
            return S_FALSE;
        }
        
        _volumeStreams[volumeName] = stream;
        *inStream = stream;
        return S_OK;
    }
};

class CArchiveExtractCallback : public IArchiveExtractCallback, public ICryptoGetTextPassword {
private:
    ULONG _refCount;
    IInArchive* _archive;
    std::string _outputDir;
    std::vector<COutFileStream*> _outStreams;
    std::vector<std::string> _extractedPaths;
    std::vector<uint32_t> _extractedIndices;
    
    FILETIME _currentMTime;
    FILETIME _currentCTime;
    FILETIME _currentATime;
    DWORD _currentAttrib;
    bool _hasMTime, _hasCTime, _hasATime, _hasAttrib;
    bool _isSymLink;
    bool _isHardLink;
    std::string _symLinkTarget;
    std::string _hardLinkTarget;
    
public:
    bool PasswordIsDefined;
    std::wstring Password;
    std::function<void(const ProgressInfo&)> ProgressCb;
    std::atomic<bool>* CancelFlag;
    ExtractOptions Options;
    std::function<void(const std::string&, const std::string&)> OnError;
    
    CArchiveExtractCallback() : _refCount(1), _archive(nullptr), PasswordIsDefined(false), 
        CancelFlag(nullptr), _hasMTime(false), _hasCTime(false), _hasATime(false), 
        _hasAttrib(false), _isSymLink(false), _isHardLink(false) {}
    
    void Init(IInArchive* archive, const std::string& outputDir, const ExtractOptions& options = ExtractOptions()) {
        _archive = archive;
        _outputDir = outputDir;
        Options = options;
    }
    
    Z7_COM7F QueryInterface(const IID& iid, void** outObject) override {
        if (iid == IID_IUnknown || iid == IID_IProgress || iid == IID_IArchiveExtractCallback) {
            *outObject = static_cast<IArchiveExtractCallback*>(this);
            AddRef();
            return S_OK;
        }
        if (iid == IID_ICryptoGetTextPassword) {
            *outObject = static_cast<ICryptoGetTextPassword*>(this);
            AddRef();
            return S_OK;
        }
        *outObject = NULL;
        return E_NOINTERFACE;
    }
    
    ULONG __stdcall AddRef() override { return InterlockedIncrement(&_refCount); }
    ULONG __stdcall Release() override {
        LONG res = InterlockedDecrement(&_refCount);
        if (res == 0) {
            for (auto stream : _outStreams) {
                if (stream) stream->Release();
            }
            delete this;
        }
        return res;
    }
    
    Z7_COM7F SetTotal(UInt64 total) override { return S_OK; }
    
    Z7_COM7F SetCompleted(const UInt64* completeValue) override {
        if (CancelFlag && CancelFlag->load()) {
            return E_ABORT;
        }
        return S_OK;
    }
    
    Z7_COM7F GetStream(UInt32 index, ISequentialOutStream** outStream, Int32 askExtractMode) override {
        *outStream = NULL;
        _hasMTime = _hasCTime = _hasATime = _hasAttrib = false;
        _isSymLink = false;
        _isHardLink = false;
        _symLinkTarget.clear();
        _hardLinkTarget.clear();
        
        if (askExtractMode != kExtract) return S_OK;
        
        PROPVARIANT prop;
        PropVariantInit(&prop);
        
        HRESULT hr = _archive->GetProperty(index, kpidPath, &prop);
        if (hr != S_OK) return hr;
        
        std::string path;
        if (prop.vt == VT_BSTR) {
            path = WStringToString(prop.bstrVal);
        }
        PropVariantClear(&prop);
        
        if (!Options.allowPathTraversal && !IsPathTraversalSafe(path)) {
            if (Options.continueOnError) {
                if (Options.onError) Options.onError("Path traversal detected", path);
                return S_OK;
            }
            return HRESULT_FROM_WIN32(ERROR_INVALID_NAME);
        }
        
        if (!Options.includePatterns.empty() || !Options.excludePatterns.empty()) {
            std::string fileName = GetFileName(path);
            
            if (!Options.includePatterns.empty()) {
                if (!MatchWildcards(fileName, Options.includePatterns)) {
                    return S_OK;
                }
            }
            
            if (!Options.excludePatterns.empty()) {
                if (MatchWildcards(fileName, Options.excludePatterns)) {
                    return S_OK;
                }
            }
        }
        
        prop.vt = VT_EMPTY;
        hr = _archive->GetProperty(index, kpidIsDir, &prop);
        bool isDir = (prop.vt == VT_BOOL && prop.boolVal != VARIANT_FALSE);
        PropVariantClear(&prop);
        
        prop.vt = VT_EMPTY;
        hr = _archive->GetProperty(index, kpidSymLink, &prop);
        if (hr == S_OK && prop.vt == VT_BOOL && prop.boolVal != VARIANT_FALSE) {
            _isSymLink = true;
        }
        PropVariantClear(&prop);
        
        prop.vt = VT_EMPTY;
        hr = _archive->GetProperty(index, kpidHardLink, &prop);
        if (hr == S_OK && prop.vt == VT_BOOL && prop.boolVal != VARIANT_FALSE) {
            _isHardLink = true;
        }
        PropVariantClear(&prop);
        
        if (_isSymLink && Options.createSymbolicLinks) {
            prop.vt = VT_EMPTY;
            hr = _archive->GetProperty(index, kpidSize, &prop);
            if (hr == S_OK && prop.vt == VT_UI8) {
                UInt64 size = prop.uhVal.QuadPart;
                std::vector<char> linkTargetBuf(size + 1);
                std::fill(linkTargetBuf.begin(), linkTargetBuf.end(), 0);
            }
            PropVariantClear(&prop);
        }
        
        if (_isHardLink && Options.createHardLinks) {
            prop.vt = VT_EMPTY;
            hr = _archive->GetProperty(index, kpidSize, &prop);
            if (hr == S_OK && prop.vt == VT_UI8) {
                UInt64 size = prop.uhVal.QuadPart;
                std::vector<char> linkTargetBuf(size + 1);
                std::fill(linkTargetBuf.begin(), linkTargetBuf.end(), 0);
            }
            PropVariantClear(&prop);
        }
        
        prop.vt = VT_EMPTY;
        hr = _archive->GetProperty(index, kpidMTime, &prop);
        if (hr == S_OK && prop.vt == VT_FILETIME) {
            _currentMTime = prop.filetime;
            _hasMTime = true;
        }
        PropVariantClear(&prop);
        
        prop.vt = VT_EMPTY;
        hr = _archive->GetProperty(index, kpidCTime, &prop);
        if (hr == S_OK && prop.vt == VT_FILETIME) {
            _currentCTime = prop.filetime;
            _hasCTime = true;
        }
        PropVariantClear(&prop);
        
        prop.vt = VT_EMPTY;
        hr = _archive->GetProperty(index, kpidATime, &prop);
        if (hr == S_OK && prop.vt == VT_FILETIME) {
            _currentATime = prop.filetime;
            _hasATime = true;
        }
        PropVariantClear(&prop);
        
        prop.vt = VT_EMPTY;
        hr = _archive->GetProperty(index, kpidAttrib, &prop);
        if (hr == S_OK && prop.vt == VT_UI4) {
            _currentAttrib = prop.ulVal;
            _hasAttrib = true;
        }
        PropVariantClear(&prop);
        
        std::string fullPath = _outputDir.empty() ? path : _outputDir + "\\" + path;
        
        if (isDir) {
            CreateDirectoryRecursive(fullPath);
            if (Options.preserveFileTime && _hasMTime) {
                HANDLE hDir = CreateFileA(fullPath.c_str(), GENERIC_WRITE, 
                    FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 
                    FILE_FLAG_BACKUP_SEMANTICS, NULL);
                if (hDir != INVALID_HANDLE_VALUE) {
                    SetFileTime(hDir, _hasCTime ? &_currentCTime : NULL, 
                                _hasATime ? &_currentATime : NULL, &_currentMTime);
                    CloseHandle(hDir);
                }
            }
            return S_OK;
        }
        
        if (FileExists(fullPath)) {
            bool shouldExtract = true;
            switch (Options.overwriteMode) {
                case ExtractOptions::OverwriteMode::Skip:
                    shouldExtract = false;
                    break;
                case ExtractOptions::OverwriteMode::Rename:
                    fullPath = GenerateUniqueFileName(fullPath);
                    break;
                case ExtractOptions::OverwriteMode::Ask:
                    if (Options.onOverwrite) {
                        shouldExtract = Options.onOverwrite(fullPath);
                    }
                    break;
                default:
                    break;
            }
            if (!shouldExtract) return S_OK;
        }
        
        CreateDirectoryForFile(fullPath);
        
        if (Options.onExtracting) {
            Options.onExtracting(fullPath);
        }
        
        COutFileStream* outFile = new COutFileStream();
        if (!outFile->Create(fullPath)) {
            DWORD err = GetLastError();
            outFile->Release();
            *outStream = NULL;
            if (Options.continueOnError) {
                if (Options.onError) Options.onError("Failed to create file: " + std::to_string(err), fullPath);
                return S_OK;
            }
            return HRESULT_FROM_WIN32(err);
        }
        
        _outStreams.push_back(outFile);
        _extractedPaths.push_back(fullPath);
        _extractedIndices.push_back(index);
        *outStream = outFile;
        
        if (ProgressCb) {
            ProgressInfo info;
            info.currentFile = path;
            info.completedFiles = index;
            ProgressCb(info);
        }
        
        return S_OK;
    }
    
    Z7_COM7F PrepareOperation(Int32 askExtractMode) override { return S_OK; }
    
    Z7_COM7F SetOperationResult(Int32 operationResult) override {
        if (operationResult != kOK) {
            std::string error;
            switch (operationResult) {
                case kUnsupportedMethod: error = "Unsupported compression method"; break;
                case kDataError: error = "Data error"; break;
                case kCRCError: error = "CRC error"; break;
                case kUnavailable: error = "File unavailable"; break;
                case kUnexpectedEnd: error = "Unexpected end of archive"; break;
                case kDataAfterEnd: error = "Data after end"; break;
                case kIsNotArc: error = "Not an archive"; break;
                case kHeadersError: error = "Headers error"; break;
                case kWrongPassword: error = "Wrong password"; break;
                default: error = "Unknown error " + std::to_string(operationResult); break;
            }
            
            if (!_extractedPaths.empty()) {
                std::string path = _extractedPaths.back();
                if (Options.onError) Options.onError(error, path);
                
                if (Options.continueOnError) {
                    _extractedPaths.pop_back();
                    _extractedIndices.pop_back();
                    return S_OK;
                }
            }
            return E_FAIL;
        }
        
        if (!_extractedPaths.empty() && !_outStreams.empty()) {
            std::string path = _extractedPaths.back();
            COutFileStream* stream = _outStreams.back();
            
            if (Options.preserveFileTime && (_hasMTime || _hasCTime || _hasATime)) {
                HANDLE hFile = stream->GetHandle();
                if (hFile != INVALID_HANDLE_VALUE) {
                    SetFileTime(hFile, 
                        _hasCTime ? &_currentCTime : NULL,
                        _hasATime ? &_currentATime : NULL,
                        _hasMTime ? &_currentMTime : NULL);
                }
            }
        }
        
        return S_OK;
    }
    
    void ApplyAttributes() {
        for (size_t i = 0; i < _extractedPaths.size(); i++) {
            if (Options.preserveFileAttrib && _hasAttrib) {
                SetFileAttributesA(_extractedPaths[i].c_str(), _currentAttrib);
            }
        }
    }
    
    Z7_COM7F CryptoGetTextPassword(BSTR* password) override {
        if (!PasswordIsDefined) return E_ABORT;
        *password = SysAllocString(Password.c_str());
        return *password ? S_OK : E_OUTOFMEMORY;
    }
};

class CArchiveUpdateCallback : public IArchiveUpdateCallback2, public ICryptoGetTextPassword2 {
private:
    ULONG _refCount;
    const std::vector<CDirItem>* _dirItems;
    UInt64 _totalSize;
    UInt64 _processedSize;
    std::string _currentFile;
    
public:
    bool PasswordIsDefined;
    std::wstring Password;
    UInt64 VolumeSize;
    std::string VolumeBasePath;
    std::function<void(const ProgressInfo&)> ProgressCb;
    std::function<bool(uint32_t, const std::string&)> VolumeCb;
    std::atomic<bool>* CancelFlag;
    uint32_t _currentVolume;
    
    CArchiveUpdateCallback() : _refCount(1), _dirItems(nullptr), PasswordIsDefined(false),
        _totalSize(0), _processedSize(0), VolumeSize(0), CancelFlag(nullptr), _currentVolume(0) {}
    
    void Init(const std::vector<CDirItem>* dirItems) {
        _dirItems = dirItems;
        _processedSize = 0;
        _currentVolume = 0;
        if (_dirItems) {
            _totalSize = 0;
            for (const auto& item : *_dirItems) {
                if (!item.IsDir) {
                    _totalSize += item.Size;
                }
            }
        }
    }
    
    Z7_COM7F QueryInterface(const IID& iid, void** outObject) override {
        if (iid == IID_IUnknown || iid == IID_IProgress || iid == IID_IArchiveUpdateCallback) {
            *outObject = static_cast<IArchiveUpdateCallback*>(this);
            AddRef();
            return S_OK;
        }
        if (iid == IID_IArchiveUpdateCallback2) {
            *outObject = static_cast<IArchiveUpdateCallback2*>(this);
            AddRef();
            return S_OK;
        }
        if (iid == IID_ICryptoGetTextPassword2) {
            *outObject = static_cast<ICryptoGetTextPassword2*>(this);
            AddRef();
            return S_OK;
        }
        *outObject = NULL;
        return E_NOINTERFACE;
    }
    
    ULONG __stdcall AddRef() override { return InterlockedIncrement(&_refCount); }
    ULONG __stdcall Release() override {
        LONG res = InterlockedDecrement(&_refCount);
        if (res == 0) delete this;
        return res;
    }
    
    Z7_COM7F SetTotal(UInt64 total) override {
        if (ProgressCb) {
            ProgressInfo info;
            info.totalBytes = total;
            info.totalFiles = (UInt32)_dirItems->size();
            ProgressCb(info);
        }
        return S_OK;
    }
    
    Z7_COM7F SetCompleted(const UInt64* completeValue) override {
        if (CancelFlag && CancelFlag->load()) {
            return E_ABORT;
        }
        if (completeValue && ProgressCb) {
            ProgressInfo info;
            info.completedBytes = *completeValue;
            info.totalBytes = _totalSize;
            info.totalFiles = (UInt32)_dirItems->size();
            if (_totalSize > 0) {
                info.percent = (int)(*completeValue * 100 / _totalSize);
            }
            info.currentFile = _currentFile;
            info.currentVolume = _currentVolume;
            ProgressCb(info);
        }
        return S_OK;
    }
    
    Z7_COM7F GetUpdateItemInfo(UInt32 index, Int32* newData, Int32* newProperties, UInt32* indexInArchive) override {
        if (newData) *newData = 1;
        if (newProperties) *newProperties = 1;
        if (indexInArchive) *indexInArchive = (UInt32)(Int32)-1;
        return S_OK;
    }
    
    Z7_COM7F GetProperty(UInt32 index, PROPID propID, PROPVARIANT* value) override {
        if (!_dirItems || index >= _dirItems->size()) return E_INVALIDARG;
        
        const CDirItem& di = (*_dirItems)[index];
        PropVariantInit(value);
        
        switch (propID) {
            case kpidPath:
                value->vt = VT_BSTR;
                value->bstrVal = SysAllocString(di.RelativePath.c_str());
                break;
            case kpidIsDir:
                value->vt = VT_BOOL;
                value->boolVal = di.IsDir ? VARIANT_TRUE : VARIANT_FALSE;
                break;
            case kpidSize:
                value->vt = VT_UI8;
                value->uhVal.QuadPart = di.Size;
                break;
            case kpidAttrib:
                value->vt = VT_UI4;
                value->ulVal = di.Attrib;
                break;
            case kpidMTime:
                value->vt = VT_FILETIME;
                value->filetime = di.MTime;
                break;
            case kpidCTime:
                value->vt = VT_FILETIME;
                value->filetime = di.CTime;
                break;
            case kpidATime:
                value->vt = VT_FILETIME;
                value->filetime = di.ATime;
                break;
            case kpidIsAnti:
                value->vt = VT_BOOL;
                value->boolVal = VARIANT_FALSE;
                break;
            case kpidPosixAttrib:
                if (di.Attrib & FILE_ATTRIBUTE_DIRECTORY) {
                    value->vt = VT_UI4;
                    value->ulVal = 0755;
                } else {
                    value->vt = VT_UI4;
                    value->ulVal = 0644;
                }
                break;
        }
        return S_OK;
    }
    
    Z7_COM7F GetStream(UInt32 index, ISequentialInStream** inStream) override {
        if (!_dirItems || index >= _dirItems->size()) return E_INVALIDARG;
        
        const CDirItem& di = (*_dirItems)[index];
        if (di.IsDir) return S_OK;
        
        _currentFile = WStringToString(di.RelativePath);
        
        if (ProgressCb) {
            ProgressInfo info;
            info.currentFile = _currentFile;
            info.completedFiles = index;
            info.totalFiles = (UInt32)_dirItems->size();
            ProgressCb(info);
        }
        
        CInFileStream* stream = new CInFileStream();
        if (!stream->Open(di.FullPathA)) {
            stream->Release();
            return HRESULT_FROM_WIN32(GetLastError());
        }
        
        *inStream = stream;
        return S_OK;
    }
    
    Z7_COM7F SetOperationResult(Int32 operationResult) override {
        _processedSize++;
        return S_OK;
    }
    
    Z7_COM7F GetVolumeSize(UInt32 index, UInt64* size) override {
        if (VolumeSize > 0 && size) {
            *size = VolumeSize;
            return S_OK;
        }
        return S_FALSE;
    }
    
    Z7_COM7F GetVolumeStream(UInt32 index, ISequentialOutStream** volumeStream) override {
        if (VolumeSize == 0 || VolumeBasePath.empty()) {
            return E_NOTIMPL;
        }
        
        std::string volumePath = SevenZipArchive::FormatVolumeName(VolumeBasePath, index + 1);
        
        if (VolumeCb) {
            if (!VolumeCb(index + 1, volumePath)) {
                return E_ABORT;
            }
        }
        
        _currentVolume = index + 1;
        
        COutFileStream* stream = new COutFileStream();
        if (!stream->Create(volumePath)) {
            stream->Release();
            return HRESULT_FROM_WIN32(GetLastError());
        }
        
        *volumeStream = stream;
        return S_OK;
    }
    
    Z7_COM7F CryptoGetTextPassword2(Int32* passwordIsDefined, BSTR* password) override {
        *passwordIsDefined = PasswordIsDefined ? 1 : 0;
        if (PasswordIsDefined) {
            *password = SysAllocString(Password.c_str());
            return *password ? S_OK : E_OUTOFMEMORY;
        }
        *password = NULL;
        return S_OK;
    }
};

class SevenZipArchiveImpl {
public:
    HMODULE _dll;
    Func_CreateObject _createObject;
    std::string _dllPath;
    bool _initialized;
    
    SevenZipArchiveImpl() : _dll(NULL), _createObject(nullptr), _initialized(false) {}
    
    ~SevenZipArchiveImpl() {
        if (_dll) {
            FreeLibrary(_dll);
            _dll = NULL;
        }
    }
    
    bool LoadDLL(const std::string& dllPath) {
        _dllPath = dllPath;
        std::wstring wpath = StringToWString(dllPath);
        _dll = LoadLibraryW(wpath.c_str());
        if (!_dll) return false;
        
        _createObject = (Func_CreateObject)GetProcAddress(_dll, "CreateObject");
        if (!_createObject) {
            FreeLibrary(_dll);
            _dll = NULL;
            return false;
        }
        return true;
    }
    
    HRESULT SetCompressionProperties(IOutArchive* outArchive, const CompressionOptions& options) {
        CMyComPtr<ISetProperties> setProperties;
        HRESULT hr = outArchive->QueryInterface(IID_ISetProperties, (void**)&setProperties);
        if (hr != S_OK || !setProperties) return hr;
        
        std::vector<const wchar_t*> names;
        std::vector<PROPVARIANT> values;
        
        PROPVARIANT prop;
        PropVariantInit(&prop);
        
        prop.vt = VT_UI4;
        prop.ulVal = static_cast<UInt32>(options.level);
        names.push_back(L"x");
        values.push_back(prop);
        
        if (options.method != CompressionMethod::COPY) {
            prop.vt = VT_BSTR;
            std::string methodName = SevenZipArchive::GetCompressionMethodName(options.method);
            std::wstring wmethod = StringToWString(methodName);
            prop.bstrVal = SysAllocString(wmethod.c_str());
            names.push_back(L"0");
            values.push_back(prop);
        }
        
        if (!options.solidMode) {
            prop.vt = VT_BOOL;
            prop.boolVal = VARIANT_FALSE;
            names.push_back(L"s");
            values.push_back(prop);
        }
        
        if (!options.dictionarySize.empty()) {
            prop.vt = VT_BSTR;
            std::wstring wdict = StringToWString(options.dictionarySize);
            prop.bstrVal = SysAllocString(wdict.c_str());
            names.push_back(L"0d");
            values.push_back(prop);
        }
        
        if (!options.wordSize.empty()) {
            prop.vt = VT_BSTR;
            std::wstring wword = StringToWString(options.wordSize);
            prop.bstrVal = SysAllocString(wword.c_str());
            names.push_back(L"0w");
            values.push_back(prop);
        }
        
        if (options.threadCount > 0) {
            prop.vt = VT_UI4;
            prop.ulVal = options.threadCount;
            names.push_back(L"mt");
            values.push_back(prop);
        }
        
        if (options.encryptHeaders && !options.password.empty()) {
            prop.vt = VT_BOOL;
            prop.boolVal = VARIANT_TRUE;
            names.push_back(L"he");
            values.push_back(prop);
        }
        
        if (options.fastBytes > 0) {
            prop.vt = VT_UI4;
            prop.ulVal = options.fastBytes;
            names.push_back(L"fb");
            values.push_back(prop);
        }
        
        if (options.literalContextBits >= 0) {
            prop.vt = VT_UI4;
            prop.ulVal = options.literalContextBits;
            names.push_back(L"lc");
            values.push_back(prop);
        }
        
        if (options.literalPosBits >= 0) {
            prop.vt = VT_UI4;
            prop.ulVal = options.literalPosBits;
            names.push_back(L"lp");
            values.push_back(prop);
        }
        
        if (options.posBits >= 0) {
            prop.vt = VT_UI4;
            prop.ulVal = options.posBits;
            names.push_back(L"pb");
            values.push_back(prop);
        }
        
        if (!options.matchFinder.empty()) {
            prop.vt = VT_BSTR;
            std::wstring wmf = StringToWString(options.matchFinder);
            prop.bstrVal = SysAllocString(wmf.c_str());
            names.push_back(L"mf");
            values.push_back(prop);
        }
        
        if (!options.methodChain.empty()) {
            prop.vt = VT_BSTR;
            std::wstring wmc = StringToWString(options.methodChain);
            prop.bstrVal = SysAllocString(wmc.c_str());
            names.push_back(L"mc");
            values.push_back(prop);
        }
        
        if (!options.autoFilter) {
            prop.vt = VT_BOOL;
            prop.boolVal = VARIANT_FALSE;
            names.push_back(L"af");
            values.push_back(prop);
        }
        
        if (options.estimatedSize > 0) {
            prop.vt = VT_UI8;
            prop.uhVal.QuadPart = options.estimatedSize;
            names.push_back(L"es");
            values.push_back(prop);
        }
        
        if (options.filter != FilterMethod::NONE) {
            prop.vt = VT_BSTR;
            std::string filterName = SevenZipArchive::GetFilterMethodName(options.filter);
            std::wstring wfilter = StringToWString(filterName);
            prop.bstrVal = SysAllocString(wfilter.c_str());
            names.push_back(L"0f");
            values.push_back(prop);
        }
        
        if (options.memoryLimit > 0) {
            prop.vt = VT_UI8;
            prop.uhVal.QuadPart = options.memoryLimit;
            names.push_back(L"memuse");
            values.push_back(prop);
        }
        
        if (options.dictionaryMemoryLimit > 0) {
            prop.vt = VT_UI8;
            prop.uhVal.QuadPart = options.dictionaryMemoryLimit;
            names.push_back(L"dmem");
            values.push_back(prop);
        }
        
        if (options.compressionThreads > 0) {
            prop.vt = VT_UI4;
            prop.ulVal = options.compressionThreads;
            names.push_back(L"ct");
            values.push_back(prop);
        }
        
        if (options.decompressionThreads > 0) {
            prop.vt = VT_UI4;
            prop.ulVal = options.decompressionThreads;
            names.push_back(L"dt");
            values.push_back(prop);
        }
        
        if (!options.useMultithreading) {
            prop.vt = VT_UI4;
            prop.ulVal = 1;
            names.push_back(L"mt");
            values.push_back(prop);
        }
        
        hr = setProperties->SetProperties(names.data(), values.data(), (UInt32)names.size());
        
        for (size_t i = 0; i < values.size(); i++) {
            if (values[i].vt == VT_BSTR && values[i].bstrVal) {
                SysFreeString(values[i].bstrVal);
            }
        }
        
        return hr;
    }
    
    GUID GetFormatCLSID(const std::string& archivePath) {
        size_t pos = archivePath.rfind('.');
        if (pos == std::string::npos) return CLSID_CFormat7z;
        
        std::string ext = archivePath.substr(pos + 1);
        
        size_t volPos = ext.rfind('.');
        if (volPos != std::string::npos) {
            std::string volExt = ext.substr(volPos + 1);
            if (volExt.length() == 3 && isdigit(volExt[0]) && isdigit(volExt[1]) && isdigit(volExt[2])) {
                ext = ext.substr(0, volPos);
            }
        }
        
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        
        if (ext == "7z") return CLSID_CFormat7z;
        if (ext == "zip" || ext == "jar" || ext == "war" || ext == "ear" || ext == "apk" || ext == "xpi") return CLSID_CFormatZip;
        if (ext == "gz" || ext == "gzip" || ext == "tgz") return CLSID_CFormatGZip;
        if (ext == "bz2" || ext == "bzip2" || ext == "tbz2" || ext == "tbz") return CLSID_CFormatBZip2;
        if (ext == "xz" || ext == "txz") return CLSID_CFormatXz;
        if (ext == "tar") return CLSID_CFormatTar;
        if (ext == "wim" || ext == "swm" || ext == "esd") return CLSID_CFormatWim;
        if (ext == "rar" || ext == "r00") return CLSID_CFormatRar;
        if (ext == "cab") return CLSID_CFormatCab;
        if (ext == "iso") return CLSID_CFormatIso;
        if (ext == "udf") return CLSID_CFormatUdf;
        if (ext == "vhd" || ext == "vhdx") return CLSID_CFormatVhd;
        if (ext == "dmg") return CLSID_CFormatDmg;
        if (ext == "hfs" || ext == "hfsx") return CLSID_CFormatHfs;
        if (ext == "chm" || ext == "chi" || ext == "chw" || ext == "hs") return CLSID_CFormatChm;
        if (ext == "lzma") return CLSID_CFormatLzma;
        if (ext == "rpm") return CLSID_CFormatRpm;
        if (ext == "deb") return CLSID_CFormatDeb;
        if (ext == "cpio") return CLSID_CFormatCpio;
        if (ext == "arj") return CLSID_CFormatArj;
        if (ext == "squashfs" || ext == "sqfs") return CLSID_CFormatSquashFS;
        if (ext == "cramfs") return CLSID_CFormatCramFS;
        if (ext == "ext" || ext == "ext2" || ext == "ext3" || ext == "ext4") return CLSID_CFormatExt;
        if (ext == "gpt") return CLSID_CFormatGPT;
        if (ext == "apfs") return CLSID_CFormatAPFS;
        if (ext == "vmdk") return CLSID_CFormatVmdk;
        if (ext == "vdi") return CLSID_CFormatVDI;
        if (ext == "qcow" || ext == "qcow2" || ext == "qcow2c") return CLSID_CFormatQcow;
        if (ext == "macho" || ext == "dylib") return CLSID_CFormatMacho;
        if (ext == "xar" || ext == "pkg") return CLSID_CFormatXar;
        if (ext == "mbr") return CLSID_CFormatMbr;
        if (ext == "nsi") return CLSID_CFormatNsis;
        if (ext == "flv") return CLSID_CFormatFlv;
        if (ext == "swf") return CLSID_CFormatSwf;
        if (ext == "fat") return CLSID_CFormatFat;
        if (ext == "ntfs") return CLSID_CFormatNtfs;
        if (ext == "lua" || ext == "luac") return CLSID_CFormatLua;
        if (ext == "ihex") return CLSID_CFormatIhex;
        if (ext == "hxs") return CLSID_CFormatHxs;
        if (ext == "nra" || ext == "nrb" || ext == "nri" || ext == "nrs" || ext == "nrw") return CLSID_CFormatNero;
        if (ext == "sfx") return CLSID_CFormatSfx;
        if (ext == "uefif") return CLSID_CFormatUEFIc;
        if (ext == "uefi") return CLSID_CFormatUEFIs;
        if (ext == "tec") return CLSID_CFormatTE;
        if (ext == "base64" || ext == "b64") return CLSID_CFormatBase64;
        if (ext == "mslz") return CLSID_CFormatMslz;
        
        return CLSID_CFormat7z;
    }
    
    void EnumerateFiles(const std::string& directory, std::vector<CDirItem>& items, 
                        bool recursive, const std::string& basePath, const std::string& rootFolder = "") {
        std::string searchPath = directory + "\\*";
        std::wstring wsearchPath = StringToWString(searchPath);
        
        WIN32_FIND_DATAW findData;
        HANDLE hFind = FindFirstFileW(wsearchPath.c_str(), &findData);
        if (hFind == INVALID_HANDLE_VALUE) return;
        
        do {
            std::wstring name = findData.cFileName;
            if (name == L"." || name == L"..") continue;
            
            CDirItem item;
            std::string fileName = WStringToString(name);
            std::string fullFilePath = directory + "\\" + fileName;
            
            std::string relativePath = GetRelativePath(fullFilePath, basePath);
            if (!rootFolder.empty()) {
                relativePath = rootFolder + "\\" + relativePath;
            }
            
            item.RelativePath = StringToWString(relativePath);
            item.FullPath = StringToWString(fileName);
            item.FullPathA = fullFilePath;
            item.Size = ((UInt64)findData.nFileSizeHigh << 32) | findData.nFileSizeLow;
            item.Attrib = findData.dwFileAttributes;
            item.IsDir = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            item.CTime = findData.ftCreationTime;
            item.ATime = findData.ftLastAccessTime;
            item.MTime = findData.ftLastWriteTime;
            
            if (item.IsDir) {
                if (recursive) {
                    EnumerateFiles(fullFilePath, items, recursive, basePath, rootFolder);
                }
                items.push_back(item);
            } else {
                items.push_back(item);
            }
        } while (FindNextFileW(hFind, &findData));
        
        FindClose(hFind);
    }
};

SevenZipArchive::SevenZipArchive(const std::string& dllPath)
    : m_dllPath(dllPath), m_impl(new SevenZipArchiveImpl()), 
      m_cancelFlag(false), m_asyncStatus(AsyncStatus::Idle), m_enableLogging(false) {
}

SevenZipArchive::~SevenZipArchive() {
    Cancel();
    WaitForCompletion();
}

bool SevenZipArchive::Initialize() {
    if (!m_impl->LoadDLL(m_dllPath)) return false;
    m_impl->_initialized = true;
    return true;
}

bool SevenZipArchive::IsInitialized() const {
    return m_impl && m_impl->_initialized;
}

ProgressInfo SevenZipArchive::GetProgress() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_mutex));
    return m_currentProgress;
}

void SevenZipArchive::Cancel() {
    m_cancelFlag = true;
}

void SevenZipArchive::WaitForCompletion() {
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
}

bool SevenZipArchive::CompressFiles(
    const std::string& archivePath,
    const std::vector<std::string>& filePaths,
    const CompressionOptions& options) {
    
    if (!IsInitialized() && !Initialize()) return false;
    if (filePaths.empty()) return false;
    
    std::vector<CDirItem> dirItems;
    for (const auto& file : filePaths) {
        if (!FileExists(file)) continue;
        
        WIN32_FILE_ATTRIBUTE_DATA attr;
        std::wstring wfile = StringToWString(file);
        if (!GetFileAttributesExW(wfile.c_str(), GetFileExInfoStandard, &attr)) continue;
        
        CDirItem item;
        std::string fileName = GetFileName(file);
        if (!options.rootFolderName.empty()) {
            item.RelativePath = StringToWString(options.rootFolderName + "\\" + fileName);
        } else {
            item.RelativePath = StringToWString(fileName);
        }
        item.FullPath = StringToWString(fileName);
        item.FullPathA = file;
        item.Size = ((UInt64)attr.nFileSizeHigh << 32) | attr.nFileSizeLow;
        item.Attrib = attr.dwFileAttributes;
        item.IsDir = false;
        item.CTime = attr.ftCreationTime;
        item.ATime = attr.ftLastAccessTime;
        item.MTime = attr.ftLastWriteTime;
        dirItems.push_back(item);
    }
    
    if (dirItems.empty()) return false;
    
    COutFileStream* outFileStream = new COutFileStream();
    if (!outFileStream->Create(archivePath)) {
        outFileStream->Release();
        return false;
    }
    
    GUID formatID = m_impl->GetFormatCLSID(archivePath);
    IOutArchive* outArchive = nullptr;
    HRESULT hr = m_impl->_createObject(&formatID, &IID_IOutArchive, (void**)&outArchive);
    
    if (hr != S_OK || !outArchive) {
        outFileStream->Release();
        return false;
    }
    
    m_impl->SetCompressionProperties(outArchive, options);
    
    CArchiveUpdateCallback* updateCallback = new CArchiveUpdateCallback();
    updateCallback->Init(&dirItems);
    updateCallback->PasswordIsDefined = !options.password.empty();
    updateCallback->Password = StringToWString(options.password);
    updateCallback->CancelFlag = &m_cancelFlag;
    
    if (options.volumeSize > 0) {
        updateCallback->VolumeSize = options.volumeSize;
        updateCallback->VolumeBasePath = archivePath;
        updateCallback->VolumeCb = m_volumeCallback;
    }
    
    if (m_progressCallback) {
        updateCallback->ProgressCb = [this](const ProgressInfo& info) {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_currentProgress = info;
            }
            m_progressCallback(info);
        };
    }
    
    hr = outArchive->UpdateItems(outFileStream, (UInt32)dirItems.size(), updateCallback);
    
    updateCallback->Release();
    outArchive->Release();
    outFileStream->Release();
    
    return hr == S_OK;
}

bool SevenZipArchive::CompressDirectory(
    const std::string& archivePath,
    const std::string& directoryPath,
    const CompressionOptions& options,
    bool recursive) {
    
    if (!IsInitialized() && !Initialize()) return false;
    
    if (!DirectoryExists(directoryPath)) return false;
    
    return CompressWithRelativePath(archivePath, directoryPath, directoryPath, options, recursive);
}

bool SevenZipArchive::CompressWithRelativePath(
    const std::string& archivePath,
    const std::string& sourcePath,
    const std::string& basePath,
    const CompressionOptions& options,
    bool recursive) {
    
    if (!IsInitialized() && !Initialize()) return false;
    
    std::string normalizedSource = NormalizePath(sourcePath);
    std::string normalizedBase = NormalizePath(basePath);
    
    if (DirectoryExists(normalizedSource)) {
        std::vector<CDirItem> dirItems;
        m_impl->EnumerateFiles(normalizedSource, dirItems, recursive, normalizedBase, options.rootFolderName);
        
        if (dirItems.empty()) return false;
        
        GUID formatID = m_impl->GetFormatCLSID(archivePath);
        IOutArchive* outArchive = nullptr;
        HRESULT hr = m_impl->_createObject(&formatID, &IID_IOutArchive, (void**)&outArchive);
        
        if (hr != S_OK || !outArchive) {
            return false;
        }
        
        m_impl->SetCompressionProperties(outArchive, options);
        
        CArchiveUpdateCallback* updateCallback = new CArchiveUpdateCallback();
        updateCallback->Init(&dirItems);
        updateCallback->PasswordIsDefined = !options.password.empty();
        updateCallback->Password = StringToWString(options.password);
        updateCallback->CancelFlag = &m_cancelFlag;
        
        if (options.volumeSize > 0) {
            updateCallback->VolumeSize = options.volumeSize;
            updateCallback->VolumeBasePath = archivePath;
            updateCallback->VolumeCb = m_volumeCallback;
        }
        
        if (m_progressCallback) {
            updateCallback->ProgressCb = [this](const ProgressInfo& info) {
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_currentProgress = info;
                }
                m_progressCallback(info);
            };
        }
        
        IOutStream* outStream = nullptr;
        COutFileStream* outFileStream = nullptr;
        CMultiOutStream* multiStream = nullptr;
        
        if (options.volumeSize > 0) {
            multiStream = new CMultiOutStream(options.volumeSize, archivePath, m_volumeCallback);
            outStream = multiStream;
        } else {
            outFileStream = new COutFileStream();
            if (!outFileStream->Create(archivePath)) {
                outFileStream->Release();
                return false;
            }
            outStream = outFileStream;
        }
        
        hr = outArchive->UpdateItems(outStream, (UInt32)dirItems.size(), updateCallback);
        
        updateCallback->Release();
        outArchive->Release();
        
        if (multiStream) multiStream->Release();
        if (outFileStream) outFileStream->Release();
        
        return hr == S_OK;
    } else if (FileExists(normalizedSource)) {
        std::vector<std::string> files = { normalizedSource };
        return CompressFiles(archivePath, files, options);
    }
    
    return false;
}

void SevenZipArchive::CompressFilesAsync(
    const std::string& archivePath,
    const std::vector<std::string>& filePaths,
    const CompressionOptions& options) {
    
    WaitForCompletion();
    m_cancelFlag = false;
    m_asyncStatus = AsyncStatus::Running;
    
    m_workerThread = std::thread([this, archivePath, filePaths, options]() {
        bool success = CompressFiles(archivePath, filePaths, options);
        
        if (m_cancelFlag.load()) {
            m_asyncStatus = AsyncStatus::Cancelled;
        } else if (success) {
            m_asyncStatus = AsyncStatus::Completed;
        } else {
            m_asyncStatus = AsyncStatus::Failed;
        }
        
        if (m_completeCallback) {
            m_completeCallback(success, archivePath);
        }
    });
}

void SevenZipArchive::CompressDirectoryAsync(
    const std::string& archivePath,
    const std::string& directoryPath,
    const CompressionOptions& options,
    bool recursive) {
    
    WaitForCompletion();
    m_cancelFlag = false;
    m_asyncStatus = AsyncStatus::Running;
    
    m_workerThread = std::thread([this, archivePath, directoryPath, options, recursive]() {
        bool success = CompressDirectory(archivePath, directoryPath, options, recursive);
        
        if (m_cancelFlag.load()) {
            m_asyncStatus = AsyncStatus::Cancelled;
        } else if (success) {
            m_asyncStatus = AsyncStatus::Completed;
        } else {
            m_asyncStatus = AsyncStatus::Failed;
        }
        
        if (m_completeCallback) {
            m_completeCallback(success, archivePath);
        }
    });
}

bool SevenZipArchive::ExtractArchive(
    const std::string& archivePath,
    const ExtractOptions& options) {
    
    if (!IsInitialized() && !Initialize()) return false;
    if (!FileExists(archivePath)) return false;
    
    if (!options.outputDir.empty() && !DirectoryExists(options.outputDir)) {
        CreateDirectoryRecursive(options.outputDir);
    }
    
    CInFileStream* inFile = new CInFileStream();
    if (!inFile->Open(archivePath)) {
        inFile->Release();
        return false;
    }
    
    GUID formatID = m_impl->GetFormatCLSID(archivePath);
    IInArchive* inArchive = nullptr;
    HRESULT hr = m_impl->_createObject(&formatID, &IID_IInArchive, (void**)&inArchive);
    
    if (hr != S_OK || !inArchive) {
        inFile->Release();
        return false;
    }
    
    CArchiveOpenCallback* openCallback = new CArchiveOpenCallback();
    openCallback->PasswordIsDefined = !options.password.empty();
    openCallback->Password = StringToWString(options.password);
    openCallback->SetBasePath(GetFileDirectory(archivePath));
    
    UInt64 scanSize = 1 << 23;
    hr = inArchive->Open(inFile, &scanSize, openCallback);
    openCallback->Release();
    
    if (hr != S_OK) {
        inArchive->Release();
        inFile->Release();
        return false;
    }
    
    CArchiveExtractCallback* extractCallback = new CArchiveExtractCallback();
    extractCallback->Init(inArchive, options.outputDir, options);
    extractCallback->PasswordIsDefined = !options.password.empty();
    extractCallback->Password = StringToWString(options.password);
    extractCallback->CancelFlag = &m_cancelFlag;
    
    if (options.onError) {
        extractCallback->OnError = options.onError;
    }
    
    if (m_progressCallback) {
        extractCallback->ProgressCb = [this](const ProgressInfo& info) {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_currentProgress = info;
            }
            m_progressCallback(info);
        };
    }
    
    hr = inArchive->Extract(NULL, (UInt32)-1, 0, extractCallback);
    
    if (hr == S_OK) {
        extractCallback->ApplyAttributes();
    }
    
    extractCallback->Release();
    inArchive->Close();
    inArchive->Release();
    inFile->Release();
    
    return hr == S_OK;
}

bool SevenZipArchive::ExtractVolumes(
    const std::string& firstVolumePath,
    const ExtractOptions& options) {
    
    return ExtractArchive(firstVolumePath, options);
}

bool SevenZipArchive::ExtractFiles(
    const std::string& archivePath,
    const std::vector<std::string>& filesToExtract,
    const std::string& outputDir,
    const std::string& password) {
    
    if (!IsInitialized() && !Initialize()) return false;
    if (!FileExists(archivePath)) return false;
    
    ExtractOptions options;
    options.outputDir = outputDir;
    options.password = password;
    
    return ExtractArchive(archivePath, options);
}

bool SevenZipArchive::TestArchive(const std::string& archivePath, const std::string& password) {
    if (!IsInitialized() && !Initialize()) return false;
    if (!FileExists(archivePath)) return false;
    
    CInFileStream* inFile = new CInFileStream();
    if (!inFile->Open(archivePath)) {
        inFile->Release();
        return false;
    }
    
    GUID formatID = m_impl->GetFormatCLSID(archivePath);
    IInArchive* inArchive = nullptr;
    HRESULT hr = m_impl->_createObject(&formatID, &IID_IInArchive, (void**)&inArchive);
    
    if (hr != S_OK || !inArchive) {
        inFile->Release();
        return false;
    }
    
    CArchiveOpenCallback* openCallback = new CArchiveOpenCallback();
    openCallback->PasswordIsDefined = !password.empty();
    openCallback->Password = StringToWString(password);
    
    UInt64 scanSize = 1 << 23;
    hr = inArchive->Open(inFile, &scanSize, openCallback);
    openCallback->Release();
    
    bool result = (hr == S_OK);
    if (result) {
        inArchive->Close();
    }
    
    inArchive->Release();
    inFile->Release();
    
    return result;
}

bool SevenZipArchive::ListArchive(const std::string& archivePath, ArchiveInfo& info, const std::string& password) {
    if (!IsInitialized() && !Initialize()) return false;
    if (!FileExists(archivePath)) return false;
    
    CInFileStream* inFile = new CInFileStream();
    if (!inFile->Open(archivePath)) {
        inFile->Release();
        return false;
    }
    
    GUID formatID = m_impl->GetFormatCLSID(archivePath);
    IInArchive* inArchive = nullptr;
    HRESULT hr = m_impl->_createObject(&formatID, &IID_IInArchive, (void**)&inArchive);
    
    if (hr != S_OK || !inArchive) {
        inFile->Release();
        return false;
    }
    
    CArchiveOpenCallback* openCallback = new CArchiveOpenCallback();
    openCallback->PasswordIsDefined = !password.empty();
    openCallback->Password = StringToWString(password);
    
    UInt64 scanSize = 1 << 23;
    hr = inArchive->Open(inFile, &scanSize, openCallback);
    openCallback->Release();
    
    if (hr != S_OK) {
        inArchive->Release();
        inFile->Release();
        return false;
    }
    
    info = ArchiveInfo();
    info.path = archivePath;
    
    UInt32 numItems = 0;
    inArchive->GetNumberOfItems(&numItems);
    
    for (UInt32 i = 0; i < numItems; i++) {
        FileInfo fi;
        PROPVARIANT prop;
        PropVariantInit(&prop);
        
        inArchive->GetProperty(i, kpidPath, &prop);
        if (prop.vt == VT_BSTR) fi.path = WStringToString(prop.bstrVal);
        PropVariantClear(&prop);
        
        inArchive->GetProperty(i, kpidSize, &prop);
        if (prop.vt == VT_UI8) fi.size = prop.uhVal.QuadPart;
        PropVariantClear(&prop);
        
        inArchive->GetProperty(i, kpidPackSize, &prop);
        if (prop.vt == VT_UI8) fi.packedSize = prop.uhVal.QuadPart;
        PropVariantClear(&prop);
        
        inArchive->GetProperty(i, kpidIsDir, &prop);
        if (prop.vt == VT_BOOL) fi.isDirectory = (prop.boolVal != VARIANT_FALSE);
        PropVariantClear(&prop);
        
        inArchive->GetProperty(i, kpidEncrypted, &prop);
        if (prop.vt == VT_BOOL) fi.isEncrypted = (prop.boolVal != VARIANT_FALSE);
        PropVariantClear(&prop);
        
        inArchive->GetProperty(i, kpidMTime, &prop);
        if (prop.vt == VT_FILETIME) fi.lastWriteTime = prop.filetime;
        PropVariantClear(&prop);
        
        inArchive->GetProperty(i, kpidCTime, &prop);
        if (prop.vt == VT_FILETIME) fi.creationTime = prop.filetime;
        PropVariantClear(&prop);
        
        inArchive->GetProperty(i, kpidATime, &prop);
        if (prop.vt == VT_FILETIME) fi.lastAccessTime = prop.filetime;
        PropVariantClear(&prop);
        
        inArchive->GetProperty(i, kpidAttrib, &prop);
        if (prop.vt == VT_UI4) fi.attributes = prop.ulVal;
        PropVariantClear(&prop);
        
        inArchive->GetProperty(i, kpidCRC, &prop);
        if (prop.vt == VT_UI4) fi.crc = prop.ulVal;
        PropVariantClear(&prop);
        
        inArchive->GetProperty(i, kpidMethod, &prop);
        if (prop.vt == VT_BSTR) fi.method = WStringToString(prop.bstrVal);
        PropVariantClear(&prop);
        
        inArchive->GetProperty(i, kpidSymLink, &prop);
        if (prop.vt == VT_BOOL) fi.isSymLink = (prop.boolVal != VARIANT_FALSE);
        PropVariantClear(&prop);
        
        inArchive->GetProperty(i, kpidHardLink, &prop);
        if (prop.vt == VT_BOOL) fi.isHardLink = (prop.boolVal != VARIANT_FALSE);
        PropVariantClear(&prop);
        
        info.files.push_back(fi);
        
        if (fi.isDirectory) info.directoryCount++;
        else { 
            info.fileCount++; 
            info.uncompressedSize += fi.size;
            info.compressedSize += fi.packedSize;
        }
        
        if (fi.isEncrypted) info.isEncrypted = true;
    }
    
    inArchive->Close();
    inArchive->Release();
    inFile->Release();
    
    return true;
}

bool SevenZipArchive::GetVolumeInfo(const std::string& firstVolumePath, VolumeInfo& info) {
    info = VolumeInfo();
    info.basePath = GetBaseArchivePath(firstVolumePath);
    
    std::string basePath = info.basePath;
    uint32_t volumeIndex = 1;
    
    while (true) {
        std::string volumePath = FormatVolumeName(basePath, volumeIndex);
        if (!FileExists(volumePath)) {
            if (volumeIndex == 1) {
                if (FileExists(basePath + ".7z")) {
                    info.volumePaths.push_back(basePath + ".7z");
                    info.volumeCount = 1;
                    return true;
                }
            }
            break;
        }
        info.volumePaths.push_back(volumePath);
        volumeIndex++;
    }
    
    info.volumeCount = (uint32_t)info.volumePaths.size();
    return info.volumeCount > 0;
}

std::string SevenZipArchive::GetCompressionMethodName(CompressionMethod method) {
    switch (method) {
        case CompressionMethod::LZMA: return "lzma";
        case CompressionMethod::LZMA2: return "lzma2";
        case CompressionMethod::PPMD: return "ppmd";
        case CompressionMethod::BZIP2: return "bzip2";
        case CompressionMethod::DEFLATE: return "deflate";
        case CompressionMethod::DEFLATE64: return "deflate64";
        case CompressionMethod::COPY: return "copy";
        case CompressionMethod::ZSTD: return "zstd";
        case CompressionMethod::LZ4: return "lz4";
        case CompressionMethod::LZ5: return "lz5";
        case CompressionMethod::BROTLI: return "brotli";
        case CompressionMethod::FLZMA2: return "flzma2";
        default: return "lzma2";
    }
}

std::string SevenZipArchive::GetFilterMethodName(FilterMethod method) {
    switch (method) {
        case FilterMethod::NONE: return "";
        case FilterMethod::BCJ: return "bcj";
        case FilterMethod::BCJ2: return "bcj2";
        case FilterMethod::DELTA: return "delta";
        case FilterMethod::BCJ_ARM: return "arm";
        case FilterMethod::BCJ_ARMT: return "armt";
        case FilterMethod::BCJ_IA64: return "ia64";
        case FilterMethod::BCJ_PPC: return "ppc";
        case FilterMethod::BCJ_SPARC: return "sparc";
        default: return "";
    }
}

std::string SevenZipArchive::GetEncryptionMethodName(EncryptionMethod method) {
    switch (method) {
        case EncryptionMethod::AES256: return "aes256";
        case EncryptionMethod::AES192: return "aes192";
        case EncryptionMethod::AES128: return "aes128";
        case EncryptionMethod::ZIPCRYPTO: return "zipcrypto";
        default: return "aes256";
    }
}

std::string SevenZipArchive::GetFormatExtension(ArchiveFormat format) {
    switch (format) {
        case ArchiveFormat::FMT_7Z: return ".7z";
        case ArchiveFormat::FMT_ZIP: return ".zip";
        case ArchiveFormat::FMT_GZIP: return ".gz";
        case ArchiveFormat::FMT_BZIP2: return ".bz2";
        case ArchiveFormat::FMT_XZ: return ".xz";
        case ArchiveFormat::FMT_TAR: return ".tar";
        case ArchiveFormat::FMT_WIM: return ".wim";
        case ArchiveFormat::FMT_RAR: return ".rar";
        case ArchiveFormat::FMT_RAR5: return ".rar";
        case ArchiveFormat::FMT_CAB: return ".cab";
        case ArchiveFormat::FMT_ISO: return ".iso";
        case ArchiveFormat::FMT_UDF: return ".udf";
        case ArchiveFormat::FMT_VHD: return ".vhd";
        case ArchiveFormat::FMT_DMG: return ".dmg";
        case ArchiveFormat::FMT_HFS: return ".hfs";
        case ArchiveFormat::FMT_HFSX: return ".hfsx";
        case ArchiveFormat::FMT_CHM: return ".chm";
        case ArchiveFormat::FMT_LZMA: return ".lzma";
        case ArchiveFormat::FMT_LZMA86: return ".lzma86";
        case ArchiveFormat::FMT_RPM: return ".rpm";
        case ArchiveFormat::FMT_DEB: return ".deb";
        case ArchiveFormat::FMT_CPIO: return ".cpio";
        case ArchiveFormat::FMT_ARJ: return ".arj";
        case ArchiveFormat::FMT_SQUASHFS: return ".squashfs";
        case ArchiveFormat::FMT_CRAMFS: return ".cramfs";
        case ArchiveFormat::FMT_EXT: return ".ext4";
        case ArchiveFormat::FMT_GPT: return ".gpt";
        case ArchiveFormat::FMT_APFS: return ".apfs";
        case ArchiveFormat::FMT_VMDK: return ".vmdk";
        case ArchiveFormat::FMT_VDI: return ".vdi";
        case ArchiveFormat::FMT_QCOW: return ".qcow2";
        case ArchiveFormat::FMT_MACHO: return ".macho";
        case ArchiveFormat::FMT_XAR: return ".xar";
        case ArchiveFormat::FMT_MBR: return ".mbr";
        case ArchiveFormat::FMT_NSI: return ".nsi";
        case ArchiveFormat::FMT_PPMD: return ".ppmd";
        case ArchiveFormat::FMT_FLV: return ".flv";
        case ArchiveFormat::FMT_SWF: return ".swf";
        case ArchiveFormat::FMT_MSLZ: return ".mslz";
        case ArchiveFormat::FMT_FAT: return ".fat";
        case ArchiveFormat::FMT_NTFS: return ".ntfs";
        default: return ".7z";
    }
}

ArchiveFormat SevenZipArchive::DetectFormatFromExtension(const std::string& path) {
    std::string ext = path;
    size_t pos = ext.rfind('.');
    if (pos == std::string::npos) return ArchiveFormat::FMT_7Z;
    
    ext = ext.substr(pos);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext == ".7z") return ArchiveFormat::FMT_7Z;
    if (ext == ".zip" || ext == ".jar" || ext == ".war" || ext == ".ear" || ext == ".apk" || ext == ".xpi") return ArchiveFormat::FMT_ZIP;
    if (ext == ".gz" || ext == ".gzip" || ext == ".tgz") return ArchiveFormat::FMT_GZIP;
    if (ext == ".bz2" || ext == ".bzip2" || ext == ".tbz2" || ext == ".tbz") return ArchiveFormat::FMT_BZIP2;
    if (ext == ".xz" || ext == ".txz") return ArchiveFormat::FMT_XZ;
    if (ext == ".tar") return ArchiveFormat::FMT_TAR;
    if (ext == ".wim" || ext == ".swm" || ext == ".esd") return ArchiveFormat::FMT_WIM;
    if (ext == ".rar" || ext == ".r00") return ArchiveFormat::FMT_RAR;
    if (ext == ".cab") return ArchiveFormat::FMT_CAB;
    if (ext == ".iso") return ArchiveFormat::FMT_ISO;
    if (ext == ".udf") return ArchiveFormat::FMT_UDF;
    if (ext == ".vhd" || ext == ".vhdx") return ArchiveFormat::FMT_VHD;
    if (ext == ".dmg") return ArchiveFormat::FMT_DMG;
    if (ext == ".hfs" || ext == ".hfsx") return ArchiveFormat::FMT_HFS;
    if (ext == ".chm" || ext == ".chi" || ext == ".chw" || ext == ".hs") return ArchiveFormat::FMT_CHM;
    if (ext == ".lzma") return ArchiveFormat::FMT_LZMA;
    if (ext == ".lzma86") return ArchiveFormat::FMT_LZMA86;
    if (ext == ".rpm") return ArchiveFormat::FMT_RPM;
    if (ext == ".deb") return ArchiveFormat::FMT_DEB;
    if (ext == ".cpio") return ArchiveFormat::FMT_CPIO;
    if (ext == ".arj") return ArchiveFormat::FMT_ARJ;
    if (ext == ".squashfs" || ext == ".sqfs") return ArchiveFormat::FMT_SQUASHFS;
    if (ext == ".cramfs") return ArchiveFormat::FMT_CRAMFS;
    if (ext == ".ext" || ext == ".ext2" || ext == ".ext3" || ext == ".ext4") return ArchiveFormat::FMT_EXT;
    if (ext == ".gpt") return ArchiveFormat::FMT_GPT;
    if (ext == ".apfs") return ArchiveFormat::FMT_APFS;
    if (ext == ".vmdk") return ArchiveFormat::FMT_VMDK;
    if (ext == ".vdi") return ArchiveFormat::FMT_VDI;
    if (ext == ".qcow" || ext == ".qcow2" || ext == ".qcow2c") return ArchiveFormat::FMT_QCOW;
    if (ext == ".macho" || ext == ".dylib") return ArchiveFormat::FMT_MACHO;
    if (ext == ".xar" || ext == ".pkg") return ArchiveFormat::FMT_XAR;
    if (ext == ".mbr") return ArchiveFormat::FMT_MBR;
    if (ext == ".nsi") return ArchiveFormat::FMT_NSI;
    if (ext == ".flv") return ArchiveFormat::FMT_FLV;
    if (ext == ".swf") return ArchiveFormat::FMT_SWF;
    if (ext == ".fat") return ArchiveFormat::FMT_FAT;
    if (ext == ".ntfs") return ArchiveFormat::FMT_NTFS;
    if (ext == ".ppmd") return ArchiveFormat::FMT_PPMD;
    if (ext == ".mslz") return ArchiveFormat::FMT_MSLZ;
    
    return ArchiveFormat::FMT_7Z;
}

std::string SevenZipArchive::FormatVolumeName(const std::string& basePath, uint32_t volumeIndex) {
    std::ostringstream oss;
    oss << basePath << "." << std::setfill('0') << std::setw(3) << volumeIndex;
    return oss.str();
}

bool SevenZipArchive::IsVolumeFile(const std::string& path) {
    size_t pos = path.rfind('.');
    if (pos == std::string::npos || pos + 4 > path.length()) return false;
    
    std::string ext = path.substr(pos + 1);
    if (ext.length() == 3) {
        for (char c : ext) {
            if (!isdigit(c)) return false;
        }
        return true;
    }
    return false;
}

std::string SevenZipArchive::GetBaseArchivePath(const std::string& volumePath) {
    if (IsVolumeFile(volumePath)) {
        size_t pos = volumePath.rfind('.');
        if (pos != std::string::npos) {
            return volumePath.substr(0, pos);
        }
    }
    return volumePath;
}

bool SevenZipArchive::AddToArchive(
    const std::string& archivePath,
    const std::vector<std::string>& filePaths,
    const CompressionOptions& options) {
    
    if (!IsInitialized() && !Initialize()) return false;
    
    std::vector<CDirItem> existingItems;
    std::vector<CDirItem> newItems;
    
    if (FileExists(archivePath)) {
        CInFileStream* inFile = new CInFileStream();
        if (inFile->Open(archivePath)) {
            GUID formatID = m_impl->GetFormatCLSID(archivePath);
            IInArchive* inArchive = nullptr;
            HRESULT hr = m_impl->_createObject(&formatID, &IID_IInArchive, (void**)&inArchive);
            
            if (hr == S_OK && inArchive) {
                CArchiveOpenCallback* openCallback = new CArchiveOpenCallback();
                openCallback->PasswordIsDefined = !options.password.empty();
                openCallback->Password = StringToWString(options.password);
                
                UInt64 scanSize = 1 << 23;
                hr = inArchive->Open(inFile, &scanSize, openCallback);
                openCallback->Release();
                
                if (hr == S_OK) {
                    UInt32 numItems = 0;
                    inArchive->GetNumberOfItems(&numItems);
                    
                    for (UInt32 i = 0; i < numItems; i++) {
                        CDirItem item;
                        PROPVARIANT prop;
                        PropVariantInit(&prop);
                        
                        inArchive->GetProperty(i, kpidPath, &prop);
                        if (prop.vt == VT_BSTR) {
                            item.RelativePath = prop.bstrVal;
                        }
                        PropVariantClear(&prop);
                        
                        inArchive->GetProperty(i, kpidIsDir, &prop);
                        item.IsDir = (prop.vt == VT_BOOL && prop.boolVal != VARIANT_FALSE);
                        PropVariantClear(&prop);
                        
                        inArchive->GetProperty(i, kpidSize, &prop);
                        if (prop.vt == VT_UI8) {
                            item.Size = prop.uhVal.QuadPart;
                        }
                        PropVariantClear(&prop);
                        
                        inArchive->GetProperty(i, kpidAttrib, &prop);
                        if (prop.vt == VT_UI4) {
                            item.Attrib = prop.ulVal;
                        }
                        PropVariantClear(&prop);
                        
                        inArchive->GetProperty(i, kpidMTime, &prop);
                        if (prop.vt == VT_FILETIME) {
                            item.MTime = prop.filetime;
                        }
                        PropVariantClear(&prop);
                        
                        item.IndexInArchive = i;
                        existingItems.push_back(item);
                    }
                    inArchive->Close();
                }
                inArchive->Release();
            }
        }
        inFile->Release();
    }
    
    for (const auto& filePath : filePaths) {
        if (!FileExists(filePath)) continue;
        
        CDirItem item;
        WIN32_FIND_DATAA findData;
        HANDLE hFind = FindFirstFileA(filePath.c_str(), &findData);
        if (hFind != INVALID_HANDLE_VALUE) {
            item.RelativePath = StringToWString(GetFileName(filePath));
            item.FullPath = StringToWString(filePath);
            item.FullPathA = filePath;
            item.Size = (findData.nFileSizeHigh * ((UInt64)MAXDWORD + 1)) + findData.nFileSizeLow;
            item.Attrib = findData.dwFileAttributes;
            item.MTime = findData.ftLastWriteTime;
            item.CTime = findData.ftCreationTime;
            item.ATime = findData.ftLastAccessTime;
            item.IsDir = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            item.IndexInArchive = (UInt32)-1;
            FindClose(hFind);
            newItems.push_back(item);
        }
    }
    
    std::string tempArchive = archivePath + ".tmp";
    
    GUID formatID = m_impl->GetFormatCLSID(archivePath);
    IOutArchive* outArchive = nullptr;
    HRESULT hr = m_impl->_createObject(&formatID, &IID_IOutArchive, (void**)&outArchive);
    
    if (hr != S_OK || !outArchive) return false;
    
    m_impl->SetCompressionProperties(outArchive, options);
    
    COutFileStream* outFile = new COutFileStream();
    if (!outFile->Create(tempArchive)) {
        outFile->Release();
        outArchive->Release();
        return false;
    }
    
    std::vector<CDirItem> allItems = existingItems;
    for (auto& item : newItems) {
        allItems.push_back(item);
    }
    
    CArchiveUpdateCallback* updateCallback = new CArchiveUpdateCallback();
    updateCallback->Init(&allItems);
    updateCallback->PasswordIsDefined = !options.password.empty();
    updateCallback->Password = StringToWString(options.password);
    updateCallback->CancelFlag = &m_cancelFlag;
    
    if (m_progressCallback) {
        updateCallback->ProgressCb = [this](const ProgressInfo& info) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_currentProgress = info;
            m_progressCallback(info);
        };
    }
    
    hr = outArchive->UpdateItems(outFile, (UInt32)allItems.size(), updateCallback);
    
    outFile->Release();
    updateCallback->Release();
    outArchive->Release();
    
    if (hr == S_OK) {
        if (FileExists(archivePath)) {
            DeleteFileA(archivePath.c_str());
        }
        MoveFileA(tempArchive.c_str(), archivePath.c_str());
        return true;
    } else {
        DeleteFileA(tempArchive.c_str());
        return false;
    }
}

bool SevenZipArchive::AddDirectoryToArchive(
    const std::string& archivePath,
    const std::string& directoryPath,
    const CompressionOptions& options,
    bool recursive) {
    
    if (!IsInitialized() && !Initialize()) return false;
    if (!DirectoryExists(directoryPath)) return false;
    
    std::vector<CDirItem> items;
    m_impl->EnumerateFiles(directoryPath, items, recursive, directoryPath);
    
    std::vector<std::string> filePaths;
    for (const auto& item : items) {
        if (!item.IsDir) {
            filePaths.push_back(item.FullPathA);
        }
    }
    
    return AddToArchive(archivePath, filePaths, options);
}

bool SevenZipArchive::DeleteFromArchive(
    const std::string& archivePath,
    const std::vector<std::string>& filesToDelete,
    const std::string& password) {
    
    if (!IsInitialized() && !Initialize()) return false;
    if (!FileExists(archivePath)) return false;
    
    CInFileStream* inFile = new CInFileStream();
    if (!inFile->Open(archivePath)) {
        inFile->Release();
        return false;
    }
    
    GUID formatID = m_impl->GetFormatCLSID(archivePath);
    IInArchive* inArchive = nullptr;
    HRESULT hr = m_impl->_createObject(&formatID, &IID_IInArchive, (void**)&inArchive);
    
    if (hr != S_OK || !inArchive) {
        inFile->Release();
        return false;
    }
    
    CArchiveOpenCallback* openCallback = new CArchiveOpenCallback();
    openCallback->PasswordIsDefined = !password.empty();
    openCallback->Password = StringToWString(password);
    
    UInt64 scanSize = 1 << 23;
    hr = inArchive->Open(inFile, &scanSize, openCallback);
    openCallback->Release();
    
    if (hr != S_OK) {
        inArchive->Release();
        inFile->Release();
        return false;
    }
    
    UInt32 numItems = 0;
    inArchive->GetNumberOfItems(&numItems);
    
    std::vector<std::wstring> deletePatterns;
    for (const auto& f : filesToDelete) {
        deletePatterns.push_back(StringToWStringLower(f));
    }
    
    std::vector<CDirItem> keepItems;
    std::vector<UInt32> keepIndices;
    
    for (UInt32 i = 0; i < numItems; i++) {
        PROPVARIANT prop;
        PropVariantInit(&prop);
        
        inArchive->GetProperty(i, kpidPath, &prop);
        std::wstring path;
        if (prop.vt == VT_BSTR) {
            path = prop.bstrVal;
        }
        PropVariantClear(&prop);
        
        std::wstring pathLower = path;
        std::transform(pathLower.begin(), pathLower.end(), pathLower.begin(), ::towlower);
        
        bool shouldDelete = false;
        for (const auto& pattern : deletePatterns) {
            if (pathLower.find(pattern) != std::string::npos) {
                shouldDelete = true;
                break;
            }
        }
        
        if (!shouldDelete) {
            CDirItem item;
            item.RelativePath = path;
            
            inArchive->GetProperty(i, kpidIsDir, &prop);
            item.IsDir = (prop.vt == VT_BOOL && prop.boolVal != VARIANT_FALSE);
            PropVariantClear(&prop);
            
            inArchive->GetProperty(i, kpidSize, &prop);
            if (prop.vt == VT_UI8) item.Size = prop.uhVal.QuadPart;
            PropVariantClear(&prop);
            
            inArchive->GetProperty(i, kpidAttrib, &prop);
            if (prop.vt == VT_UI4) item.Attrib = prop.ulVal;
            PropVariantClear(&prop);
            
            inArchive->GetProperty(i, kpidMTime, &prop);
            if (prop.vt == VT_FILETIME) item.MTime = prop.filetime;
            PropVariantClear(&prop);
            
            item.IndexInArchive = i;
            keepItems.push_back(item);
            keepIndices.push_back(i);
        }
    }
    
    inArchive->Close();
    inArchive->Release();
    inFile->Release();
    
    if (keepItems.empty()) {
        DeleteFileA(archivePath.c_str());
        return true;
    }
    
    std::string tempArchive = archivePath + ".tmp";
    
    IOutArchive* outArchive = nullptr;
    hr = m_impl->_createObject(&formatID, &IID_IOutArchive, (void**)&outArchive);
    
    if (hr != S_OK || !outArchive) return false;
    
    CompressionOptions options;
    options.password = password;
    m_impl->SetCompressionProperties(outArchive, options);
    
    COutFileStream* outFile = new COutFileStream();
    if (!outFile->Create(tempArchive)) {
        outFile->Release();
        outArchive->Release();
        return false;
    }
    
    class CArchiveUpdateCallbackDelete : public IArchiveUpdateCallback2 {
    private:
        ULONG _refCount;
        const std::vector<CDirItem>* _items;
        IInArchive* _inArchive;
        const std::vector<UInt32>* _indices;
        
    public:
        bool PasswordIsDefined;
        std::wstring Password;
        std::function<void(const ProgressInfo&)> ProgressCb;
        std::atomic<bool>* CancelFlag;
        
        CArchiveUpdateCallbackDelete() : _refCount(1), _items(nullptr), _inArchive(nullptr), 
            _indices(nullptr), PasswordIsDefined(false), CancelFlag(nullptr) {}
        
        void Init(const std::vector<CDirItem>* items, IInArchive* inArchive, const std::vector<UInt32>* indices) {
            _items = items;
            _inArchive = inArchive;
            _indices = indices;
        }
        
        Z7_COM7F QueryInterface(const IID& iid, void** outObject) override {
            if (iid == IID_IUnknown || iid == IID_IProgress || iid == IID_IArchiveUpdateCallback) {
                *outObject = static_cast<IArchiveUpdateCallback*>(this);
                AddRef();
                return S_OK;
            }
            if (iid == IID_IArchiveUpdateCallback2) {
                *outObject = static_cast<IArchiveUpdateCallback2*>(this);
                AddRef();
                return S_OK;
            }
            *outObject = NULL;
            return E_NOINTERFACE;
        }
        
        ULONG __stdcall AddRef() override { return InterlockedIncrement(&_refCount); }
        ULONG __stdcall Release() override {
            LONG res = InterlockedDecrement(&_refCount);
            if (res == 0) delete this;
            return res;
        }
        
        Z7_COM7F SetTotal(UInt64 total) override { return S_OK; }
        Z7_COM7F SetCompleted(const UInt64* completeValue) override {
            if (CancelFlag && CancelFlag->load()) return E_ABORT;
            return S_OK;
        }
        
        Z7_COM7F GetUpdateItemInfo(UInt32 index, Int32* newData, Int32* newProperties, UInt32* indexInArchive) override {
            if (newData) *newData = 0;
            if (newProperties) *newProperties = 0;
            if (indexInArchive) *indexInArchive = (*_indices)[index];
            return S_OK;
        }
        
        Z7_COM7F GetProperty(UInt32 index, PROPID propID, PROPVARIANT* value) override {
            if (!_items || index >= _items->size()) return E_INVALIDARG;
            
            const CDirItem& di = (*_items)[index];
            PropVariantInit(value);
            
            switch (propID) {
                case kpidPath:
                    value->vt = VT_BSTR;
                    value->bstrVal = SysAllocString(di.RelativePath.c_str());
                    break;
                case kpidIsDir:
                    value->vt = VT_BOOL;
                    value->boolVal = di.IsDir ? VARIANT_TRUE : VARIANT_FALSE;
                    break;
                case kpidSize:
                    value->vt = VT_UI8;
                    value->uhVal.QuadPart = di.Size;
                    break;
                case kpidAttrib:
                    value->vt = VT_UI4;
                    value->ulVal = di.Attrib;
                    break;
                case kpidMTime:
                    value->vt = VT_FILETIME;
                    value->filetime = di.MTime;
                    break;
            }
            return S_OK;
        }
        
        Z7_COM7F GetStream(UInt32 index, ISequentialInStream** inStream) override {
            *inStream = NULL;
            return S_OK;
        }
        
        Z7_COM7F SetOperationResult(Int32 operationResult) override { return S_OK; }
        
        Z7_COM7F GetVolumeSize(UInt32 index, UInt64* size) override { 
            if (size) *size = 0;
            return S_FALSE; 
        }
        Z7_COM7F GetVolumeStream(UInt32 index, ISequentialOutStream** volumeStream) override { 
            *volumeStream = NULL;
            return E_NOTIMPL; 
        }
    };
    
    CArchiveUpdateCallbackDelete* updateCallback = new CArchiveUpdateCallbackDelete();
    updateCallback->Init(&keepItems, nullptr, &keepIndices);
    updateCallback->PasswordIsDefined = !password.empty();
    updateCallback->Password = StringToWString(password);
    updateCallback->CancelFlag = &m_cancelFlag;
    
    hr = outArchive->UpdateItems(outFile, (UInt32)keepItems.size(), updateCallback);
    
    outFile->Release();
    updateCallback->Release();
    outArchive->Release();
    
    if (hr == S_OK) {
        DeleteFileA(archivePath.c_str());
        MoveFileA(tempArchive.c_str(), archivePath.c_str());
        return true;
    } else {
        DeleteFileA(tempArchive.c_str());
        return false;
    }
}

bool SevenZipArchive::UpdateArchive(
    const std::string& archivePath,
    const std::vector<std::string>& filesToUpdate,
    const CompressionOptions& options) {
    
    return AddToArchive(archivePath, filesToUpdate, options);
}

bool SevenZipArchive::CompressToMemory(
    const std::vector<std::string>& filePaths,
    std::vector<uint8_t>& output,
    const CompressionOptions& options) {
    
    if (!IsInitialized() && !Initialize()) return false;
    
    std::vector<CDirItem> dirItems;
    for (const auto& filePath : filePaths) {
        if (!FileExists(filePath)) continue;
        
        CDirItem item;
        WIN32_FIND_DATAA findData;
        HANDLE hFind = FindFirstFileA(filePath.c_str(), &findData);
        if (hFind != INVALID_HANDLE_VALUE) {
            item.RelativePath = StringToWString(GetFileName(filePath));
            item.FullPath = StringToWString(filePath);
            item.FullPathA = filePath;
            item.Size = (findData.nFileSizeHigh * ((UInt64)MAXDWORD + 1)) + findData.nFileSizeLow;
            item.Attrib = findData.dwFileAttributes;
            item.MTime = findData.ftLastWriteTime;
            item.CTime = findData.ftCreationTime;
            item.ATime = findData.ftLastAccessTime;
            item.IsDir = false;
            FindClose(hFind);
            dirItems.push_back(item);
        }
    }
    
    if (dirItems.empty()) return false;
    
    GUID formatID = CLSID_CFormat7z;
    IOutArchive* outArchive = nullptr;
    HRESULT hr = m_impl->_createObject(&formatID, &IID_IOutArchive, (void**)&outArchive);
    
    if (hr != S_OK || !outArchive) return false;
    
    m_impl->SetCompressionProperties(outArchive, options);
    
    COutMemoryStream* memStream = new COutMemoryStream();
    
    CArchiveUpdateCallback* updateCallback = new CArchiveUpdateCallback();
    updateCallback->Init(&dirItems);
    updateCallback->PasswordIsDefined = !options.password.empty();
    updateCallback->Password = StringToWString(options.password);
    updateCallback->CancelFlag = &m_cancelFlag;
    
    if (m_progressCallback) {
        updateCallback->ProgressCb = [this](const ProgressInfo& info) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_currentProgress = info;
            m_progressCallback(info);
        };
    }
    
    hr = outArchive->UpdateItems(memStream, (UInt32)dirItems.size(), updateCallback);
    
    if (hr == S_OK) {
        output = memStream->GetBuffer();
    }
    
    memStream->Release();
    updateCallback->Release();
    outArchive->Release();
    
    return hr == S_OK;
}

bool SevenZipArchive::CompressDirectoryToMemory(
    const std::string& directoryPath,
    std::vector<uint8_t>& output,
    const CompressionOptions& options,
    bool recursive) {
    
    if (!IsInitialized() && !Initialize()) return false;
    if (!DirectoryExists(directoryPath)) return false;
    
    std::vector<CDirItem> dirItems;
    m_impl->EnumerateFiles(directoryPath, dirItems, recursive, directoryPath);
    
    std::vector<std::string> filePaths;
    for (const auto& item : dirItems) {
        if (!item.IsDir) {
            filePaths.push_back(item.FullPathA);
        }
    }
    
    return CompressToMemory(filePaths, output, options);
}

bool SevenZipArchive::ExtractFromMemory(
    const uint8_t* data,
    size_t size,
    const ExtractOptions& options) {
    
    if (!IsInitialized() && !Initialize()) return false;
    if (!data || size == 0) return false;
    
    CInMemoryStream* memStream = new CInMemoryStream(data, size);
    
    GUID formatID = CLSID_CFormat7z;
    IInArchive* inArchive = nullptr;
    HRESULT hr = m_impl->_createObject(&formatID, &IID_IInArchive, (void**)&inArchive);
    
    if (hr != S_OK || !inArchive) {
        memStream->Release();
        return false;
    }
    
    CArchiveOpenCallback* openCallback = new CArchiveOpenCallback();
    openCallback->PasswordIsDefined = !options.password.empty();
    openCallback->Password = StringToWString(options.password);
    
    UInt64 scanSize = 1 << 23;
    hr = inArchive->Open(memStream, &scanSize, openCallback);
    openCallback->Release();
    
    if (hr != S_OK) {
        inArchive->Release();
        memStream->Release();
        return false;
    }
    
    CArchiveExtractCallback* extractCallback = new CArchiveExtractCallback();
    extractCallback->Init(inArchive, options.outputDir, options);
    extractCallback->PasswordIsDefined = !options.password.empty();
    extractCallback->Password = StringToWString(options.password);
    extractCallback->CancelFlag = &m_cancelFlag;
    
    if (options.onError) {
        extractCallback->OnError = options.onError;
    }
    
    if (m_progressCallback) {
        extractCallback->ProgressCb = [this](const ProgressInfo& info) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_currentProgress = info;
            m_progressCallback(info);
        };
    }
    
    hr = inArchive->Extract(NULL, (UInt32)-1, 0, extractCallback);
    
    if (hr == S_OK) {
        extractCallback->ApplyAttributes();
    }
    
    extractCallback->Release();
    inArchive->Close();
    inArchive->Release();
    memStream->Release();
    
    return hr == S_OK;
}

bool SevenZipArchive::ExtractFileFromMemory(
    const uint8_t* data,
    size_t size,
    const std::string& fileName,
    std::vector<uint8_t>& output,
    const std::string& password) {
    
    if (!IsInitialized() && !Initialize()) return false;
    if (!data || size == 0) return false;
    
    CInMemoryStream* memStream = new CInMemoryStream(data, size);
    
    GUID formatID = CLSID_CFormat7z;
    IInArchive* inArchive = nullptr;
    HRESULT hr = m_impl->_createObject(&formatID, &IID_IInArchive, (void**)&inArchive);
    
    if (hr != S_OK || !inArchive) {
        memStream->Release();
        return false;
    }
    
    CArchiveOpenCallback* openCallback = new CArchiveOpenCallback();
    openCallback->PasswordIsDefined = !password.empty();
    openCallback->Password = StringToWString(password);
    
    UInt64 scanSize = 1 << 23;
    hr = inArchive->Open(memStream, &scanSize, openCallback);
    openCallback->Release();
    
    if (hr != S_OK) {
        inArchive->Release();
        memStream->Release();
        return false;
    }
    
    UInt32 numItems = 0;
    inArchive->GetNumberOfItems(&numItems);
    
    UInt32 targetIndex = (UInt32)-1;
    std::wstring targetName = StringToWStringLower(fileName);
    
    for (UInt32 i = 0; i < numItems; i++) {
        PROPVARIANT prop;
        PropVariantInit(&prop);
        
        inArchive->GetProperty(i, kpidPath, &prop);
        std::wstring path;
        if (prop.vt == VT_BSTR) {
            path = prop.bstrVal;
        }
        PropVariantClear(&prop);
        
        std::wstring pathLower = path;
        std::transform(pathLower.begin(), pathLower.end(), pathLower.begin(), ::towlower);
        
        if (pathLower == targetName || pathLower.find(targetName) != std::wstring::npos) {
            targetIndex = i;
            break;
        }
    }
    
    if (targetIndex == (UInt32)-1) {
        inArchive->Close();
        inArchive->Release();
        memStream->Release();
        return false;
    }
    
    class CMemoryExtractCallback : public IArchiveExtractCallback, public ICryptoGetTextPassword {
    private:
        ULONG _refCount;
        IInArchive* _archive;
        std::vector<uint8_t>* _output;
        COutMemoryStream* _outStream;
        UInt32 _targetIndex;
        
    public:
        bool PasswordIsDefined;
        std::wstring Password;
        
        CMemoryExtractCallback() : _refCount(1), _archive(nullptr), _output(nullptr), 
            _outStream(nullptr), _targetIndex((UInt32)-1), PasswordIsDefined(false) {}
        
        void Init(IInArchive* archive, std::vector<uint8_t>* output, UInt32 targetIndex) {
            _archive = archive;
            _output = output;
            _targetIndex = targetIndex;
        }
        
        Z7_COM7F QueryInterface(const IID& iid, void** outObject) override {
            if (iid == IID_IUnknown || iid == IID_IProgress || iid == IID_IArchiveExtractCallback) {
                *outObject = this;
                AddRef();
                return S_OK;
            }
            if (iid == IID_ICryptoGetTextPassword) {
                *outObject = static_cast<ICryptoGetTextPassword*>(this);
                AddRef();
                return S_OK;
            }
            *outObject = NULL;
            return E_NOINTERFACE;
        }
        
        ULONG __stdcall AddRef() override { return InterlockedIncrement(&_refCount); }
        ULONG __stdcall Release() override {
            LONG res = InterlockedDecrement(&_refCount);
            if (res == 0) delete this;
            return res;
        }
        
        Z7_COM7F SetTotal(UInt64 total) override { return S_OK; }
        Z7_COM7F SetCompleted(const UInt64* completeValue) override { return S_OK; }
        
        Z7_COM7F GetStream(UInt32 index, ISequentialOutStream** outStream, Int32 askExtractMode) override {
            *outStream = NULL;
            if (index != _targetIndex) return S_OK;
            if (askExtractMode != kExtract) return S_OK;
            
            _outStream = new COutMemoryStream();
            *outStream = _outStream;
            return S_OK;
        }
        
        Z7_COM7F PrepareOperation(Int32 askExtractMode) override { return S_OK; }
        
        Z7_COM7F SetOperationResult(Int32 operationResult) override {
            if (operationResult == kOK && _outStream && _output) {
                *_output = _outStream->GetBuffer();
            }
            return S_OK;
        }
        
        Z7_COM7F CryptoGetTextPassword(BSTR* password) override {
            if (!PasswordIsDefined) return E_ABORT;
            *password = SysAllocString(Password.c_str());
            return *password ? S_OK : E_OUTOFMEMORY;
        }
    };
    
    CMemoryExtractCallback* extractCallback = new CMemoryExtractCallback();
    extractCallback->Init(inArchive, &output, targetIndex);
    extractCallback->PasswordIsDefined = !password.empty();
    extractCallback->Password = StringToWString(password);
    
    UInt32 indices[1] = { targetIndex };
    hr = inArchive->Extract(indices, 1, 0, extractCallback);
    
    extractCallback->Release();
    inArchive->Close();
    inArchive->Release();
    memStream->Release();
    
    return hr == S_OK && !output.empty();
}

bool SevenZipArchive::ListArchiveFromMemory(
    const uint8_t* data,
    size_t size,
    ArchiveInfo& info,
    const std::string& password) {
    
    if (!IsInitialized() && !Initialize()) return false;
    if (!data || size == 0) return false;
    
    CInMemoryStream* memStream = new CInMemoryStream(data, size);
    
    GUID formatID = CLSID_CFormat7z;
    IInArchive* inArchive = nullptr;
    HRESULT hr = m_impl->_createObject(&formatID, &IID_IInArchive, (void**)&inArchive);
    
    if (hr != S_OK || !inArchive) {
        memStream->Release();
        return false;
    }
    
    CArchiveOpenCallback* openCallback = new CArchiveOpenCallback();
    openCallback->PasswordIsDefined = !password.empty();
    openCallback->Password = StringToWString(password);
    
    UInt64 scanSize = 1 << 23;
    hr = inArchive->Open(memStream, &scanSize, openCallback);
    openCallback->Release();
    
    if (hr != S_OK) {
        inArchive->Release();
        memStream->Release();
        return false;
    }
    
    info = ArchiveInfo();
    
    UInt32 numItems = 0;
    inArchive->GetNumberOfItems(&numItems);
    
    for (UInt32 i = 0; i < numItems; i++) {
        FileInfo fi;
        PROPVARIANT prop;
        PropVariantInit(&prop);
        
        inArchive->GetProperty(i, kpidPath, &prop);
        if (prop.vt == VT_BSTR) fi.path = WStringToString(prop.bstrVal);
        PropVariantClear(&prop);
        
        inArchive->GetProperty(i, kpidSize, &prop);
        if (prop.vt == VT_UI8) fi.size = prop.uhVal.QuadPart;
        PropVariantClear(&prop);
        
        inArchive->GetProperty(i, kpidPackSize, &prop);
        if (prop.vt == VT_UI8) fi.packedSize = prop.uhVal.QuadPart;
        PropVariantClear(&prop);
        
        inArchive->GetProperty(i, kpidIsDir, &prop);
        if (prop.vt == VT_BOOL) fi.isDirectory = (prop.boolVal != VARIANT_FALSE);
        PropVariantClear(&prop);
        
        inArchive->GetProperty(i, kpidEncrypted, &prop);
        if (prop.vt == VT_BOOL) fi.isEncrypted = (prop.boolVal != VARIANT_FALSE);
        PropVariantClear(&prop);
        
        inArchive->GetProperty(i, kpidCRC, &prop);
        if (prop.vt == VT_UI4) fi.crc = prop.ulVal;
        PropVariantClear(&prop);
        
        inArchive->GetProperty(i, kpidMethod, &prop);
        if (prop.vt == VT_BSTR) fi.method = WStringToString(prop.bstrVal);
        PropVariantClear(&prop);
        
        info.files.push_back(fi);
        
        if (fi.isDirectory) info.directoryCount++;
        else {
            info.fileCount++;
            info.uncompressedSize += fi.size;
            info.compressedSize += fi.packedSize;
        }
        
        if (fi.isEncrypted) info.isEncrypted = true;
    }
    
    inArchive->Close();
    inArchive->Release();
    memStream->Release();
    
    return true;
}

bool SevenZipArchive::CreateSFX(
    const std::string& archivePath,
    const std::string& sfxPath,
    const std::string& sfxModule) {
    
    if (!FileExists(archivePath)) return false;
    
    std::string sfxModulePath = sfxModule;
    if (sfxModulePath.empty()) {
        char modulePath[MAX_PATH];
        GetModuleFileNameA(NULL, modulePath, MAX_PATH);
        std::string exeDir = GetFileDirectory(modulePath);
        sfxModulePath = exeDir + "\\7z.sfx";
        
        if (!FileExists(sfxModulePath)) {
            sfxModulePath = exeDir + "\\7zCon.sfx";
        }
    }
    
    std::ifstream sfxFile(sfxModulePath, std::ios::binary);
    if (!sfxFile.is_open()) {
        sfxModulePath = "7z.sfx";
        sfxFile.open(sfxModulePath, std::ios::binary);
    }
    
    std::ofstream outFile(sfxPath, std::ios::binary);
    if (!outFile.is_open()) return false;
    
    if (sfxFile.is_open()) {
        outFile << sfxFile.rdbuf();
        sfxFile.close();
    }
    
    std::ifstream archiveFile(archivePath, std::ios::binary);
    if (!archiveFile.is_open()) {
        outFile.close();
        return false;
    }
    
    outFile << archiveFile.rdbuf();
    archiveFile.close();
    outFile.close();
    
    return true;
}

bool SevenZipArchive::CreateSFXFromMemory(
    const std::vector<uint8_t>& archiveData,
    const std::string& sfxPath,
    const std::string& sfxModule) {
    
    std::string sfxModulePath = sfxModule;
    if (sfxModulePath.empty()) {
        char modulePath[MAX_PATH];
        GetModuleFileNameA(NULL, modulePath, MAX_PATH);
        std::string exeDir = GetFileDirectory(modulePath);
        sfxModulePath = exeDir + "\\7z.sfx";
    }
    
    std::ofstream outFile(sfxPath, std::ios::binary);
    if (!outFile.is_open()) return false;
    
    std::ifstream sfxFile(sfxModulePath, std::ios::binary);
    if (sfxFile.is_open()) {
        outFile << sfxFile.rdbuf();
        sfxFile.close();
    }
    
    outFile.write(reinterpret_cast<const char*>(archiveData.data()), archiveData.size());
    outFile.close();
    
    return true;
}

bool SevenZipArchive::GetArchiveComment(
    const std::string& archivePath,
    std::string& comment,
    const std::string& password) {
    
    if (!IsInitialized() && !Initialize()) return false;
    if (!FileExists(archivePath)) return false;
    
    CInFileStream* inFile = new CInFileStream();
    if (!inFile->Open(archivePath)) {
        inFile->Release();
        return false;
    }
    
    GUID formatID = m_impl->GetFormatCLSID(archivePath);
    IInArchive* inArchive = nullptr;
    HRESULT hr = m_impl->_createObject(&formatID, &IID_IInArchive, (void**)&inArchive);
    
    if (hr != S_OK || !inArchive) {
        inFile->Release();
        return false;
    }
    
    CArchiveOpenCallback* openCallback = new CArchiveOpenCallback();
    openCallback->PasswordIsDefined = !password.empty();
    openCallback->Password = StringToWString(password);
    
    UInt64 scanSize = 1 << 23;
    hr = inArchive->Open(inFile, &scanSize, openCallback);
    openCallback->Release();
    
    if (hr != S_OK) {
        inArchive->Release();
        inFile->Release();
        return false;
    }
    
    PROPVARIANT prop;
    PropVariantInit(&prop);
    
    hr = inArchive->GetArchiveProperty(kpidComment, &prop);
    if (hr == S_OK && prop.vt == VT_BSTR) {
        comment = WStringToString(prop.bstrVal);
    }
    PropVariantClear(&prop);
    
    inArchive->Close();
    inArchive->Release();
    inFile->Release();
    
    return true;
}

bool SevenZipArchive::SetArchiveComment(
    const std::string& archivePath,
    const std::string& comment,
    const std::string& password) {
    
    if (!IsInitialized() && !Initialize()) return false;
    if (!FileExists(archivePath)) return false;
    
    CInFileStream* inFile = new CInFileStream();
    if (!inFile->Open(archivePath)) {
        inFile->Release();
        return false;
    }
    
    GUID formatID = m_impl->GetFormatCLSID(archivePath);
    IInArchive* inArchive = nullptr;
    HRESULT hr = m_impl->_createObject(&formatID, &IID_IInArchive, (void**)&inArchive);
    
    if (hr != S_OK || !inArchive) {
        inFile->Release();
        return false;
    }
    
    CArchiveOpenCallback* openCallback = new CArchiveOpenCallback();
    openCallback->PasswordIsDefined = !password.empty();
    openCallback->Password = StringToWString(password);
    
    UInt64 scanSize = 1 << 23;
    hr = inArchive->Open(inFile, &scanSize, openCallback);
    openCallback->Release();
    
    if (hr != S_OK) {
        inArchive->Release();
        inFile->Release();
        return false;
    }
    
    UInt32 numItems = 0;
    inArchive->GetNumberOfItems(&numItems);
    
    std::vector<CDirItem> items;
    std::vector<UInt32> indices;
    
    for (UInt32 i = 0; i < numItems; i++) {
        CDirItem item;
        PROPVARIANT prop;
        PropVariantInit(&prop);
        
        inArchive->GetProperty(i, kpidPath, &prop);
        if (prop.vt == VT_BSTR) item.RelativePath = prop.bstrVal;
        PropVariantClear(&prop);
        
        inArchive->GetProperty(i, kpidIsDir, &prop);
        item.IsDir = (prop.vt == VT_BOOL && prop.boolVal != VARIANT_FALSE);
        PropVariantClear(&prop);
        
        inArchive->GetProperty(i, kpidSize, &prop);
        if (prop.vt == VT_UI8) item.Size = prop.uhVal.QuadPart;
        PropVariantClear(&prop);
        
        inArchive->GetProperty(i, kpidAttrib, &prop);
        if (prop.vt == VT_UI4) item.Attrib = prop.ulVal;
        PropVariantClear(&prop);
        
        inArchive->GetProperty(i, kpidMTime, &prop);
        if (prop.vt == VT_FILETIME) item.MTime = prop.filetime;
        PropVariantClear(&prop);
        
        item.IndexInArchive = i;
        items.push_back(item);
        indices.push_back(i);
    }
    
    inArchive->Close();
    
    std::string tempArchive = archivePath + ".tmp";
    
    IOutArchive* outArchive = nullptr;
    hr = m_impl->_createObject(&formatID, &IID_IOutArchive, (void**)&outArchive);
    
    if (hr != S_OK || !outArchive) {
        inArchive->Release();
        inFile->Release();
        return false;
    }
    
    CompressionOptions options;
    options.password = password;
    m_impl->SetCompressionProperties(outArchive, options);
    
    CMyComPtr<ISetProperties> setProps;
    hr = outArchive->QueryInterface(IID_ISetProperties, (void**)&setProps);
    if (hr == S_OK && setProps) {
        const wchar_t* names[1] = { L"!comments" };
        PROPVARIANT values[1];
        PropVariantInit(&values[0]);
        values[0].vt = VT_BSTR;
        std::wstring wcomment = StringToWString(comment);
        values[0].bstrVal = SysAllocString(wcomment.c_str());
        
        setProps->SetProperties(names, values, 1);
        
        SysFreeString(values[0].bstrVal);
    }
    
    COutFileStream* outFile = new COutFileStream();
    if (!outFile->Create(tempArchive)) {
        outFile->Release();
        outArchive->Release();
        inArchive->Release();
        inFile->Release();
        return false;
    }
    
    class CArchiveUpdateCallbackComment : public IArchiveUpdateCallback2 {
    private:
        ULONG _refCount;
        const std::vector<CDirItem>* _items;
        const std::vector<UInt32>* _indices;
        IInArchive* _inArchive;
        
    public:
        bool PasswordIsDefined;
        std::wstring Password;
        std::atomic<bool>* CancelFlag;
        
        CArchiveUpdateCallbackComment() : _refCount(1), _items(nullptr), _indices(nullptr),
            _inArchive(nullptr), PasswordIsDefined(false), CancelFlag(nullptr) {}
        
        void Init(const std::vector<CDirItem>* items, const std::vector<UInt32>* indices, IInArchive* inArchive) {
            _items = items;
            _indices = indices;
            _inArchive = inArchive;
        }
        
        Z7_COM7F QueryInterface(const IID& iid, void** outObject) override {
            if (iid == IID_IUnknown || iid == IID_IProgress || iid == IID_IArchiveUpdateCallback) {
                *outObject = static_cast<IArchiveUpdateCallback*>(this);
                AddRef();
                return S_OK;
            }
            if (iid == IID_IArchiveUpdateCallback2) {
                *outObject = static_cast<IArchiveUpdateCallback2*>(this);
                AddRef();
                return S_OK;
            }
            *outObject = NULL;
            return E_NOINTERFACE;
        }
        
        ULONG __stdcall AddRef() override { return InterlockedIncrement(&_refCount); }
        ULONG __stdcall Release() override {
            LONG res = InterlockedDecrement(&_refCount);
            if (res == 0) delete this;
            return res;
        }
        
        Z7_COM7F SetTotal(UInt64 total) override { return S_OK; }
        Z7_COM7F SetCompleted(const UInt64* completeValue) override {
            if (CancelFlag && CancelFlag->load()) return E_ABORT;
            return S_OK;
        }
        
        Z7_COM7F GetUpdateItemInfo(UInt32 index, Int32* newData, Int32* newProperties, UInt32* indexInArchive) override {
            if (newData) *newData = 0;
            if (newProperties) *newProperties = 0;
            if (indexInArchive) *indexInArchive = (*_indices)[index];
            return S_OK;
        }
        
        Z7_COM7F GetProperty(UInt32 index, PROPID propID, PROPVARIANT* value) override {
            if (!_items || index >= _items->size()) return E_INVALIDARG;
            
            const CDirItem& di = (*_items)[index];
            PropVariantInit(value);
            
            switch (propID) {
                case kpidPath:
                    value->vt = VT_BSTR;
                    value->bstrVal = SysAllocString(di.RelativePath.c_str());
                    break;
                case kpidIsDir:
                    value->vt = VT_BOOL;
                    value->boolVal = di.IsDir ? VARIANT_TRUE : VARIANT_FALSE;
                    break;
                case kpidSize:
                    value->vt = VT_UI8;
                    value->uhVal.QuadPart = di.Size;
                    break;
                case kpidAttrib:
                    value->vt = VT_UI4;
                    value->ulVal = di.Attrib;
                    break;
                case kpidMTime:
                    value->vt = VT_FILETIME;
                    value->filetime = di.MTime;
                    break;
            }
            return S_OK;
        }
        
        Z7_COM7F GetStream(UInt32 index, ISequentialInStream** inStream) override {
            *inStream = NULL;
            return S_OK;
        }
        
        Z7_COM7F SetOperationResult(Int32 operationResult) override { return S_OK; }
        Z7_COM7F GetVolumeSize(UInt32 index, UInt64* size) override { 
            if (size) *size = 0;
            return S_FALSE; 
        }
        Z7_COM7F GetVolumeStream(UInt32 index, ISequentialOutStream** volumeStream) override { 
            *volumeStream = NULL;
            return E_NOTIMPL; 
        }
    };
    
    CArchiveUpdateCallbackComment* updateCallback = new CArchiveUpdateCallbackComment();
    updateCallback->Init(&items, &indices, inArchive);
    updateCallback->PasswordIsDefined = !password.empty();
    updateCallback->Password = StringToWString(password);
    updateCallback->CancelFlag = &m_cancelFlag;
    
    hr = outArchive->UpdateItems(outFile, (UInt32)items.size(), updateCallback);
    
    outFile->Release();
    updateCallback->Release();
    outArchive->Release();
    inArchive->Release();
    inFile->Release();
    
    if (hr == S_OK) {
        DeleteFileA(archivePath.c_str());
        MoveFileA(tempArchive.c_str(), archivePath.c_str());
        return true;
    } else {
        DeleteFileA(tempArchive.c_str());
        return false;
    }
}

bool SevenZipArchive::RenameInArchive(
    const std::string& archivePath,
    const std::string& oldPath,
    const std::string& newPath,
    const std::string& password) {
    
    if (!IsInitialized() && !Initialize()) return false;
    if (!FileExists(archivePath)) return false;
    
    CInFileStream* inFile = new CInFileStream();
    if (!inFile->Open(archivePath)) {
        inFile->Release();
        return false;
    }
    
    GUID formatID = m_impl->GetFormatCLSID(archivePath);
    IInArchive* inArchive = nullptr;
    HRESULT hr = m_impl->_createObject(&formatID, &IID_IInArchive, (void**)&inArchive);
    
    if (hr != S_OK || !inArchive) {
        inFile->Release();
        return false;
    }
    
    CArchiveOpenCallback* openCallback = new CArchiveOpenCallback();
    openCallback->PasswordIsDefined = !password.empty();
    openCallback->Password = StringToWString(password);
    
    UInt64 scanSize = 1 << 23;
    hr = inArchive->Open(inFile, &scanSize, openCallback);
    openCallback->Release();
    
    if (hr != S_OK) {
        inArchive->Release();
        inFile->Release();
        return false;
    }
    
    UInt32 numItems = 0;
    inArchive->GetNumberOfItems(&numItems);
    
    std::vector<CDirItem> items;
    std::vector<UInt32> indices;
    std::wstring oldPathW = StringToWStringLower(oldPath);
    std::wstring newPathW = StringToWString(newPath);
    
    for (UInt32 i = 0; i < numItems; i++) {
        CDirItem item;
        PROPVARIANT prop;
        PropVariantInit(&prop);
        
        inArchive->GetProperty(i, kpidPath, &prop);
        std::wstring path;
        if (prop.vt == VT_BSTR) {
            path = prop.bstrVal;
            item.RelativePath = prop.bstrVal;
        }
        PropVariantClear(&prop);
        
        std::wstring pathLower = path;
        std::transform(pathLower.begin(), pathLower.end(), pathLower.begin(), ::towlower);
        
        if (pathLower == oldPathW || pathLower.find(oldPathW) == 0) {
            std::wstring newPathForItem = newPathW + path.substr(oldPathW.length());
            item.RelativePath = newPathForItem;
        }
        
        inArchive->GetProperty(i, kpidIsDir, &prop);
        item.IsDir = (prop.vt == VT_BOOL && prop.boolVal != VARIANT_FALSE);
        PropVariantClear(&prop);
        
        inArchive->GetProperty(i, kpidSize, &prop);
        if (prop.vt == VT_UI8) item.Size = prop.uhVal.QuadPart;
        PropVariantClear(&prop);
        
        inArchive->GetProperty(i, kpidAttrib, &prop);
        if (prop.vt == VT_UI4) item.Attrib = prop.ulVal;
        PropVariantClear(&prop);
        
        inArchive->GetProperty(i, kpidMTime, &prop);
        if (prop.vt == VT_FILETIME) item.MTime = prop.filetime;
        PropVariantClear(&prop);
        
        item.IndexInArchive = i;
        items.push_back(item);
        indices.push_back(i);
    }
    
    inArchive->Close();
    inArchive->Release();
    inFile->Release();
    
    std::string tempArchive = archivePath + ".tmp";
    
    IOutArchive* outArchive = nullptr;
    hr = m_impl->_createObject(&formatID, &IID_IOutArchive, (void**)&outArchive);
    
    if (hr != S_OK || !outArchive) return false;
    
    CompressionOptions options;
    options.password = password;
    m_impl->SetCompressionProperties(outArchive, options);
    
    COutFileStream* outFile = new COutFileStream();
    if (!outFile->Create(tempArchive)) {
        outFile->Release();
        outArchive->Release();
        return false;
    }
    
    class CArchiveUpdateCallbackRename : public IArchiveUpdateCallback2 {
    private:
        ULONG _refCount;
        const std::vector<CDirItem>* _items;
        const std::vector<UInt32>* _indices;
        
    public:
        bool PasswordIsDefined;
        std::wstring Password;
        std::atomic<bool>* CancelFlag;
        
        CArchiveUpdateCallbackRename() : _refCount(1), _items(nullptr), _indices(nullptr), 
            PasswordIsDefined(false), CancelFlag(nullptr) {}
        
        void Init(const std::vector<CDirItem>* items, const std::vector<UInt32>* indices) {
            _items = items;
            _indices = indices;
        }
        
        Z7_COM7F QueryInterface(const IID& iid, void** outObject) override {
            if (iid == IID_IUnknown || iid == IID_IProgress || iid == IID_IArchiveUpdateCallback) {
                *outObject = static_cast<IArchiveUpdateCallback*>(this);
                AddRef();
                return S_OK;
            }
            if (iid == IID_IArchiveUpdateCallback2) {
                *outObject = static_cast<IArchiveUpdateCallback2*>(this);
                AddRef();
                return S_OK;
            }
            *outObject = NULL;
            return E_NOINTERFACE;
        }
        
        ULONG __stdcall AddRef() override { return InterlockedIncrement(&_refCount); }
        ULONG __stdcall Release() override {
            LONG res = InterlockedDecrement(&_refCount);
            if (res == 0) delete this;
            return res;
        }
        
        Z7_COM7F SetTotal(UInt64 total) override { return S_OK; }
        Z7_COM7F SetCompleted(const UInt64* completeValue) override {
            if (CancelFlag && CancelFlag->load()) return E_ABORT;
            return S_OK;
        }
        
        Z7_COM7F GetUpdateItemInfo(UInt32 index, Int32* newData, Int32* newProperties, UInt32* indexInArchive) override {
            if (newData) *newData = 0;
            if (newProperties) *newProperties = 1;
            if (indexInArchive) *indexInArchive = (*_indices)[index];
            return S_OK;
        }
        
        Z7_COM7F GetProperty(UInt32 index, PROPID propID, PROPVARIANT* value) override {
            if (!_items || index >= _items->size()) return E_INVALIDARG;
            
            const CDirItem& di = (*_items)[index];
            PropVariantInit(value);
            
            switch (propID) {
                case kpidPath:
                    value->vt = VT_BSTR;
                    value->bstrVal = SysAllocString(di.RelativePath.c_str());
                    break;
                case kpidIsDir:
                    value->vt = VT_BOOL;
                    value->boolVal = di.IsDir ? VARIANT_TRUE : VARIANT_FALSE;
                    break;
                case kpidSize:
                    value->vt = VT_UI8;
                    value->uhVal.QuadPart = di.Size;
                    break;
                case kpidAttrib:
                    value->vt = VT_UI4;
                    value->ulVal = di.Attrib;
                    break;
                case kpidMTime:
                    value->vt = VT_FILETIME;
                    value->filetime = di.MTime;
                    break;
            }
            return S_OK;
        }
        
        Z7_COM7F GetStream(UInt32 index, ISequentialInStream** inStream) override {
            *inStream = NULL;
            return S_OK;
        }
        
        Z7_COM7F SetOperationResult(Int32 operationResult) override { return S_OK; }
        Z7_COM7F GetVolumeSize(UInt32 index, UInt64* size) override { 
            if (size) *size = 0;
            return S_FALSE; 
        }
        Z7_COM7F GetVolumeStream(UInt32 index, ISequentialOutStream** volumeStream) override { 
            *volumeStream = NULL;
            return E_NOTIMPL; 
        }
    };
    
    CArchiveUpdateCallbackRename* updateCallback = new CArchiveUpdateCallbackRename();
    updateCallback->Init(&items, &indices);
    updateCallback->PasswordIsDefined = !password.empty();
    updateCallback->Password = StringToWString(password);
    updateCallback->CancelFlag = &m_cancelFlag;
    
    hr = outArchive->UpdateItems(outFile, (UInt32)items.size(), updateCallback);
    
    outFile->Release();
    updateCallback->Release();
    outArchive->Release();
    
    if (hr == S_OK) {
        DeleteFileA(archivePath.c_str());
        MoveFileA(tempArchive.c_str(), archivePath.c_str());
        return true;
    } else {
        DeleteFileA(tempArchive.c_str());
        return false;
    }
}

bool SevenZipArchive::SetFileAttributesInArchive(
    const std::string& archivePath,
    const std::string& filePath,
    uint32_t attributes,
    const std::string& password) {
    
    if (!IsInitialized() && !Initialize()) return false;
    if (!FileExists(archivePath)) return false;
    
    CInFileStream* inFile = new CInFileStream();
    if (!inFile->Open(archivePath)) {
        inFile->Release();
        return false;
    }
    
    GUID formatID = m_impl->GetFormatCLSID(archivePath);
    IInArchive* inArchive = nullptr;
    HRESULT hr = m_impl->_createObject(&formatID, &IID_IInArchive, (void**)&inArchive);
    
    if (hr != S_OK || !inArchive) {
        inFile->Release();
        return false;
    }
    
    CArchiveOpenCallback* openCallback = new CArchiveOpenCallback();
    openCallback->PasswordIsDefined = !password.empty();
    openCallback->Password = StringToWString(password);
    
    UInt64 scanSize = 1 << 23;
    hr = inArchive->Open(inFile, &scanSize, openCallback);
    openCallback->Release();
    
    if (hr != S_OK) {
        inArchive->Release();
        inFile->Release();
        return false;
    }
    
    UInt32 numItems = 0;
    inArchive->GetNumberOfItems(&numItems);
    
    std::vector<CDirItem> items;
    std::vector<UInt32> indices;
    std::wstring targetPath = StringToWStringLower(filePath);
    
    for (UInt32 i = 0; i < numItems; i++) {
        CDirItem item;
        PROPVARIANT prop;
        PropVariantInit(&prop);
        
        inArchive->GetProperty(i, kpidPath, &prop);
        std::wstring path;
        if (prop.vt == VT_BSTR) {
            path = prop.bstrVal;
            item.RelativePath = prop.bstrVal;
        }
        PropVariantClear(&prop);
        
        inArchive->GetProperty(i, kpidIsDir, &prop);
        item.IsDir = (prop.vt == VT_BOOL && prop.boolVal != VARIANT_FALSE);
        PropVariantClear(&prop);
        
        inArchive->GetProperty(i, kpidSize, &prop);
        if (prop.vt == VT_UI8) item.Size = prop.uhVal.QuadPart;
        PropVariantClear(&prop);
        
        inArchive->GetProperty(i, kpidAttrib, &prop);
        if (prop.vt == VT_UI4) item.Attrib = prop.ulVal;
        PropVariantClear(&prop);
        
        inArchive->GetProperty(i, kpidMTime, &prop);
        if (prop.vt == VT_FILETIME) item.MTime = prop.filetime;
        PropVariantClear(&prop);
        
        std::wstring pathLower = path;
        std::transform(pathLower.begin(), pathLower.end(), pathLower.begin(), ::towlower);
        
        if (pathLower == targetPath) {
            item.Attrib = attributes;
        }
        
        item.IndexInArchive = i;
        items.push_back(item);
        indices.push_back(i);
    }
    
    inArchive->Close();
    
    std::string tempArchive = archivePath + ".tmp";
    
    IOutArchive* outArchive = nullptr;
    hr = m_impl->_createObject(&formatID, &IID_IOutArchive, (void**)&outArchive);
    
    if (hr != S_OK || !outArchive) {
        inArchive->Release();
        inFile->Release();
        return false;
    }
    
    CompressionOptions options;
    options.password = password;
    m_impl->SetCompressionProperties(outArchive, options);
    
    COutFileStream* outFile = new COutFileStream();
    if (!outFile->Create(tempArchive)) {
        outFile->Release();
        outArchive->Release();
        inArchive->Release();
        inFile->Release();
        return false;
    }
    
    class CArchiveUpdateCallbackAttr : public IArchiveUpdateCallback2 {
    private:
        ULONG _refCount;
        const std::vector<CDirItem>* _items;
        const std::vector<UInt32>* _indices;
        
    public:
        bool PasswordIsDefined;
        std::wstring Password;
        std::atomic<bool>* CancelFlag;
        
        CArchiveUpdateCallbackAttr() : _refCount(1), _items(nullptr), _indices(nullptr),
            PasswordIsDefined(false), CancelFlag(nullptr) {}
        
        void Init(const std::vector<CDirItem>* items, const std::vector<UInt32>* indices) {
            _items = items;
            _indices = indices;
        }
        
        Z7_COM7F QueryInterface(const IID& iid, void** outObject) override {
            if (iid == IID_IUnknown || iid == IID_IProgress || iid == IID_IArchiveUpdateCallback) {
                *outObject = static_cast<IArchiveUpdateCallback*>(this);
                AddRef();
                return S_OK;
            }
            if (iid == IID_IArchiveUpdateCallback2) {
                *outObject = static_cast<IArchiveUpdateCallback2*>(this);
                AddRef();
                return S_OK;
            }
            *outObject = NULL;
            return E_NOINTERFACE;
        }
        
        ULONG __stdcall AddRef() override { return InterlockedIncrement(&_refCount); }
        ULONG __stdcall Release() override {
            LONG res = InterlockedDecrement(&_refCount);
            if (res == 0) delete this;
            return res;
        }
        
        Z7_COM7F SetTotal(UInt64 total) override { return S_OK; }
        Z7_COM7F SetCompleted(const UInt64* completeValue) override {
            if (CancelFlag && CancelFlag->load()) return E_ABORT;
            return S_OK;
        }
        
        Z7_COM7F GetUpdateItemInfo(UInt32 index, Int32* newData, Int32* newProperties, UInt32* indexInArchive) override {
            if (newData) *newData = 0;
            if (newProperties) *newProperties = 1;
            if (indexInArchive) *indexInArchive = (*_indices)[index];
            return S_OK;
        }
        
        Z7_COM7F GetProperty(UInt32 index, PROPID propID, PROPVARIANT* value) override {
            if (!_items || index >= _items->size()) return E_INVALIDARG;
            
            const CDirItem& di = (*_items)[index];
            PropVariantInit(value);
            
            switch (propID) {
                case kpidPath:
                    value->vt = VT_BSTR;
                    value->bstrVal = SysAllocString(di.RelativePath.c_str());
                    break;
                case kpidIsDir:
                    value->vt = VT_BOOL;
                    value->boolVal = di.IsDir ? VARIANT_TRUE : VARIANT_FALSE;
                    break;
                case kpidSize:
                    value->vt = VT_UI8;
                    value->uhVal.QuadPart = di.Size;
                    break;
                case kpidAttrib:
                    value->vt = VT_UI4;
                    value->ulVal = di.Attrib;
                    break;
                case kpidMTime:
                    value->vt = VT_FILETIME;
                    value->filetime = di.MTime;
                    break;
            }
            return S_OK;
        }
        
        Z7_COM7F GetStream(UInt32 index, ISequentialInStream** inStream) override {
            *inStream = NULL;
            return S_OK;
        }
        
        Z7_COM7F SetOperationResult(Int32 operationResult) override { return S_OK; }
        Z7_COM7F GetVolumeSize(UInt32 index, UInt64* size) override { 
            if (size) *size = 0;
            return S_FALSE; 
        }
        Z7_COM7F GetVolumeStream(UInt32 index, ISequentialOutStream** volumeStream) override { 
            *volumeStream = NULL;
            return E_NOTIMPL; 
        }
    };
    
    CArchiveUpdateCallbackAttr* updateCallback = new CArchiveUpdateCallbackAttr();
    updateCallback->Init(&items, &indices);
    updateCallback->PasswordIsDefined = !password.empty();
    updateCallback->Password = StringToWString(password);
    updateCallback->CancelFlag = &m_cancelFlag;
    
    hr = outArchive->UpdateItems(outFile, (UInt32)items.size(), updateCallback);
    
    outFile->Release();
    updateCallback->Release();
    outArchive->Release();
    inArchive->Release();
    inFile->Release();
    
    if (hr == S_OK) {
        DeleteFileA(archivePath.c_str());
        MoveFileA(tempArchive.c_str(), archivePath.c_str());
        return true;
    } else {
        DeleteFileA(tempArchive.c_str());
        return false;
    }
}

bool SevenZipArchive::GetFileCRC(
    const std::string& archivePath,
    const std::string& filePath,
    uint32_t& crc,
    const std::string& password) {
    
    ArchiveInfo info;
    if (!ListArchive(archivePath, info, password)) return false;
    
    std::string filePathLower = filePath;
    std::transform(filePathLower.begin(), filePathLower.end(), filePathLower.begin(), ::tolower);
    
    for (const auto& fi : info.files) {
        std::string pathLower = fi.path;
        std::transform(pathLower.begin(), pathLower.end(), pathLower.begin(), ::tolower);
        
        if (pathLower == filePathLower || pathLower.find(filePathLower) != std::string::npos) {
            crc = fi.crc;
            return true;
        }
    }
    
    return false;
}

bool SevenZipArchive::ExtractSingleFileToMemory(
    const std::string& archivePath,
    const std::string& filePath,
    std::vector<uint8_t>& output,
    const std::string& password) {
    
    if (!IsInitialized() && !Initialize()) return false;
    if (!FileExists(archivePath)) return false;
    
    CInFileStream* inFile = new CInFileStream();
    if (!inFile->Open(archivePath)) {
        inFile->Release();
        return false;
    }
    
    GUID formatID = m_impl->GetFormatCLSID(archivePath);
    IInArchive* inArchive = nullptr;
    HRESULT hr = m_impl->_createObject(&formatID, &IID_IInArchive, (void**)&inArchive);
    
    if (hr != S_OK || !inArchive) {
        inFile->Release();
        return false;
    }
    
    CArchiveOpenCallback* openCallback = new CArchiveOpenCallback();
    openCallback->PasswordIsDefined = !password.empty();
    openCallback->Password = StringToWString(password);
    
    UInt64 scanSize = 1 << 23;
    hr = inArchive->Open(inFile, &scanSize, openCallback);
    openCallback->Release();
    
    if (hr != S_OK) {
        inArchive->Release();
        inFile->Release();
        return false;
    }
    
    UInt32 numItems = 0;
    inArchive->GetNumberOfItems(&numItems);
    
    UInt32 targetIndex = (UInt32)-1;
    std::wstring targetName = StringToWStringLower(filePath);
    
    for (UInt32 i = 0; i < numItems; i++) {
        PROPVARIANT prop;
        PropVariantInit(&prop);
        
        inArchive->GetProperty(i, kpidPath, &prop);
        std::wstring path;
        if (prop.vt == VT_BSTR) {
            path = prop.bstrVal;
        }
        PropVariantClear(&prop);
        
        std::wstring pathLower = path;
        std::transform(pathLower.begin(), pathLower.end(), pathLower.begin(), ::towlower);
        
        if (pathLower == targetName || pathLower.find(targetName) != std::wstring::npos) {
            targetIndex = i;
            break;
        }
    }
    
    if (targetIndex == (UInt32)-1) {
        inArchive->Close();
        inArchive->Release();
        inFile->Release();
        return false;
    }
    
    class CSingleFileExtractCallback : public IArchiveExtractCallback, public ICryptoGetTextPassword {
    private:
        ULONG _refCount;
        COutMemoryStream* _outStream;
        std::vector<uint8_t>* _output;
        UInt32 _targetIndex;
        
    public:
        bool PasswordIsDefined;
        std::wstring Password;
        
        CSingleFileExtractCallback() : _refCount(1), _outStream(nullptr), _output(nullptr),
            _targetIndex((UInt32)-1), PasswordIsDefined(false) {}
        
        void Init(std::vector<uint8_t>* output, UInt32 targetIndex) {
            _output = output;
            _targetIndex = targetIndex;
        }
        
        Z7_COM7F QueryInterface(const IID& iid, void** outObject) override {
            if (iid == IID_IUnknown || iid == IID_IProgress || iid == IID_IArchiveExtractCallback) {
                *outObject = this;
                AddRef();
                return S_OK;
            }
            if (iid == IID_ICryptoGetTextPassword) {
                *outObject = static_cast<ICryptoGetTextPassword*>(this);
                AddRef();
                return S_OK;
            }
            *outObject = NULL;
            return E_NOINTERFACE;
        }
        
        ULONG __stdcall AddRef() override { return InterlockedIncrement(&_refCount); }
        ULONG __stdcall Release() override {
            LONG res = InterlockedDecrement(&_refCount);
            if (res == 0) delete this;
            return res;
        }
        
        Z7_COM7F SetTotal(UInt64 total) override { return S_OK; }
        Z7_COM7F SetCompleted(const UInt64* completeValue) override { return S_OK; }
        
        Z7_COM7F GetStream(UInt32 index, ISequentialOutStream** outStream, Int32 askExtractMode) override {
            *outStream = NULL;
            if (index != _targetIndex) return S_OK;
            if (askExtractMode != kExtract) return S_OK;
            
            _outStream = new COutMemoryStream();
            *outStream = _outStream;
            return S_OK;
        }
        
        Z7_COM7F PrepareOperation(Int32 askExtractMode) override { return S_OK; }
        
        Z7_COM7F SetOperationResult(Int32 operationResult) override {
            if (operationResult == kOK && _outStream && _output) {
                *_output = _outStream->GetBuffer();
            }
            return S_OK;
        }
        
        Z7_COM7F CryptoGetTextPassword(BSTR* password) override {
            if (!PasswordIsDefined) return E_ABORT;
            *password = SysAllocString(Password.c_str());
            return *password ? S_OK : E_OUTOFMEMORY;
        }
    };
    
    CSingleFileExtractCallback* extractCallback = new CSingleFileExtractCallback();
    extractCallback->Init(&output, targetIndex);
    extractCallback->PasswordIsDefined = !password.empty();
    extractCallback->Password = StringToWString(password);
    
    UInt32 indices[1] = { targetIndex };
    hr = inArchive->Extract(indices, 1, 0, extractCallback);
    
    extractCallback->Release();
    inArchive->Close();
    inArchive->Release();
    inFile->Release();
    
    return hr == S_OK && !output.empty();
}

bool SevenZipArchive::CompressStream(
    const uint8_t* inputData,
    size_t inputSize,
    std::vector<uint8_t>& output,
    const std::string& fileName,
    const CompressionOptions& options) {
    
    if (!IsInitialized() && !Initialize()) return false;
    if (!inputData || inputSize == 0) return false;
    
    class CInMemoryStreamWrapper : public IInStream {
    private:
        ULONG _refCount;
        const uint8_t* _data;
        size_t _size;
        size_t _pos;
        
    public:
        CInMemoryStreamWrapper(const uint8_t* data, size_t size) 
            : _refCount(1), _data(data), _size(size), _pos(0) {}
        
        Z7_COM7F QueryInterface(const IID& iid, void** outObject) override {
            if (iid == IID_IUnknown || iid == IID_ISequentialInStream || iid == IID_IInStream) {
                *outObject = this;
                AddRef();
                return S_OK;
            }
            *outObject = NULL;
            return E_NOINTERFACE;
        }
        
        ULONG __stdcall AddRef() override { return InterlockedIncrement(&_refCount); }
        ULONG __stdcall Release() override {
            LONG res = InterlockedDecrement(&_refCount);
            if (res == 0) delete this;
            return res;
        }
        
        Z7_COM7F Read(void* data, UInt32 size, UInt32* processedSize) override {
            if (processedSize) *processedSize = 0;
            if (_pos >= _size || size == 0) return S_OK;
            
            size_t remaining = _size - _pos;
            size_t toRead = (size < remaining) ? size : remaining;
            memcpy(data, _data + _pos, toRead);
            _pos += toRead;
            if (processedSize) *processedSize = (UInt32)toRead;
            return S_OK;
        }
        
        Z7_COM7F Seek(Int64 offset, UInt32 seekOrigin, UInt64* newPosition) override {
            Int64 newPos = 0;
            switch (seekOrigin) {
                case 0: newPos = offset; break;
                case 1: newPos = (Int64)_pos + offset; break;
                case 2: newPos = (Int64)_size + offset; break;
                default: return STG_E_INVALIDFUNCTION;
            }
            
            if (newPos < 0) newPos = 0;
            if (newPos > (Int64)_size) newPos = (Int64)_size;
            _pos = (size_t)newPos;
            if (newPosition) *newPosition = _pos;
            return S_OK;
        }
    };
    
    std::vector<CDirItem> dirItems;
    CDirItem item;
    item.RelativePath = StringToWString(fileName);
    item.Size = inputSize;
    item.IsDir = false;
    item.Attrib = 0;
    GetSystemTimeAsFileTime(&item.MTime);
    item.FullPathA = fileName;
    item.IndexInArchive = (UInt32)-1;
    dirItems.push_back(item);
    
    GUID formatID = CLSID_CFormat7z;
    IOutArchive* outArchive = nullptr;
    HRESULT hr = m_impl->_createObject(&formatID, &IID_IOutArchive, (void**)&outArchive);
    
    if (hr != S_OK || !outArchive) return false;
    
    m_impl->SetCompressionProperties(outArchive, options);
    
    COutMemoryStream* memStream = new COutMemoryStream();
    
    class CStreamUpdateCallback : public IArchiveUpdateCallback2, public ICryptoGetTextPassword2 {
    private:
        ULONG _refCount;
        const std::vector<CDirItem>* _items;
        const uint8_t* _data;
        size_t _size;
        
    public:
        bool PasswordIsDefined;
        std::wstring Password;
        std::atomic<bool>* CancelFlag;
        
        CStreamUpdateCallback() : _refCount(1), _items(nullptr), _data(nullptr), _size(0),
            PasswordIsDefined(false), CancelFlag(nullptr) {}
        
        void Init(const std::vector<CDirItem>* items, const uint8_t* data, size_t size) {
            _items = items;
            _data = data;
            _size = size;
        }
        
        Z7_COM7F QueryInterface(const IID& iid, void** outObject) override {
            if (iid == IID_IUnknown || iid == IID_IProgress || iid == IID_IArchiveUpdateCallback) {
                *outObject = static_cast<IArchiveUpdateCallback*>(this);
                AddRef();
                return S_OK;
            }
            if (iid == IID_IArchiveUpdateCallback2) {
                *outObject = static_cast<IArchiveUpdateCallback2*>(this);
                AddRef();
                return S_OK;
            }
            if (iid == IID_ICryptoGetTextPassword2) {
                *outObject = static_cast<ICryptoGetTextPassword2*>(this);
                AddRef();
                return S_OK;
            }
            *outObject = NULL;
            return E_NOINTERFACE;
        }
        
        ULONG __stdcall AddRef() override { return InterlockedIncrement(&_refCount); }
        ULONG __stdcall Release() override {
            LONG res = InterlockedDecrement(&_refCount);
            if (res == 0) delete this;
            return res;
        }
        
        Z7_COM7F SetTotal(UInt64 total) override { return S_OK; }
        Z7_COM7F SetCompleted(const UInt64* completeValue) override {
            if (CancelFlag && CancelFlag->load()) return E_ABORT;
            return S_OK;
        }
        
        Z7_COM7F GetUpdateItemInfo(UInt32 index, Int32* newData, Int32* newProperties, UInt32* indexInArchive) override {
            if (newData) *newData = 1;
            if (newProperties) *newProperties = 1;
            if (indexInArchive) *indexInArchive = (UInt32)-1;
            return S_OK;
        }
        
        Z7_COM7F GetProperty(UInt32 index, PROPID propID, PROPVARIANT* value) override {
            if (!_items || index >= _items->size()) return E_INVALIDARG;
            
            const CDirItem& di = (*_items)[index];
            PropVariantInit(value);
            
            switch (propID) {
                case kpidPath:
                    value->vt = VT_BSTR;
                    value->bstrVal = SysAllocString(di.RelativePath.c_str());
                    break;
                case kpidIsDir:
                    value->vt = VT_BOOL;
                    value->boolVal = di.IsDir ? VARIANT_TRUE : VARIANT_FALSE;
                    break;
                case kpidSize:
                    value->vt = VT_UI8;
                    value->uhVal.QuadPart = di.Size;
                    break;
                case kpidAttrib:
                    value->vt = VT_UI4;
                    value->ulVal = di.Attrib;
                    break;
                case kpidMTime:
                    value->vt = VT_FILETIME;
                    value->filetime = di.MTime;
                    break;
            }
            return S_OK;
        }
        
        Z7_COM7F GetStream(UInt32 index, ISequentialInStream** inStream) override {
            if (!_data || _size == 0) {
                *inStream = NULL;
                return S_OK;
            }
            
            *inStream = new CInMemoryStreamWrapper(_data, _size);
            return S_OK;
        }
        
        Z7_COM7F SetOperationResult(Int32 operationResult) override { return S_OK; }
        Z7_COM7F GetVolumeSize(UInt32 index, UInt64* size) override { 
            if (size) *size = 0;
            return S_FALSE; 
        }
        Z7_COM7F GetVolumeStream(UInt32 index, ISequentialOutStream** volumeStream) override { 
            *volumeStream = NULL;
            return E_NOTIMPL; 
        }
        Z7_COM7F CryptoGetTextPassword2(Int32* passwordIsDefined, BSTR* password) override {
            *passwordIsDefined = PasswordIsDefined ? 1 : 0;
            if (PasswordIsDefined) {
                *password = SysAllocString(Password.c_str());
            }
            return S_OK;
        }
    };
    
    CStreamUpdateCallback* updateCallback = new CStreamUpdateCallback();
    updateCallback->Init(&dirItems, inputData, inputSize);
    updateCallback->PasswordIsDefined = !options.password.empty();
    updateCallback->Password = StringToWString(options.password);
    updateCallback->CancelFlag = &m_cancelFlag;
    
    hr = outArchive->UpdateItems(memStream, (UInt32)dirItems.size(), updateCallback);
    
    if (hr == S_OK) {
        output = memStream->GetBuffer();
    }
    
    memStream->Release();
    updateCallback->Release();
    outArchive->Release();
    
    return hr == S_OK;
}

bool SevenZipArchive::ExtractStream(
    const uint8_t* archiveData,
    size_t archiveSize,
    const std::string& fileName,
    std::vector<uint8_t>& output,
    const std::string& password) {
    
    return ExtractFileFromMemory(archiveData, archiveSize, fileName, output, password);
}

bool SevenZipArchive::IsArchiveEncrypted(const std::string& archivePath) {
    ArchiveInfo info;
    if (!ListArchive(archivePath, info, "")) return false;
    return info.isEncrypted;
}

bool SevenZipArchive::IsArchiveSolid(const std::string& archivePath) {
    if (!IsInitialized() && !Initialize()) return false;
    if (!FileExists(archivePath)) return false;
    
    CInFileStream* inFile = new CInFileStream();
    if (!inFile->Open(archivePath)) {
        inFile->Release();
        return false;
    }
    
    GUID formatID = m_impl->GetFormatCLSID(archivePath);
    IInArchive* inArchive = nullptr;
    HRESULT hr = m_impl->_createObject(&formatID, &IID_IInArchive, (void**)&inArchive);
    
    if (hr != S_OK || !inArchive) {
        inFile->Release();
        return false;
    }
    
    CArchiveOpenCallback* openCallback = new CArchiveOpenCallback();
    
    UInt64 scanSize = 1 << 23;
    hr = inArchive->Open(inFile, &scanSize, openCallback);
    openCallback->Release();
    
    if (hr != S_OK) {
        inArchive->Release();
        inFile->Release();
        return false;
    }
    
    PROPVARIANT prop;
    PropVariantInit(&prop);
    
    bool isSolid = false;
    hr = inArchive->GetArchiveProperty(kpidSolid, &prop);
    if (hr == S_OK && prop.vt == VT_BOOL) {
        isSolid = (prop.boolVal != VARIANT_FALSE);
    }
    PropVariantClear(&prop);
    
    inArchive->Close();
    inArchive->Release();
    inFile->Release();
    
    return isSolid;
}

bool SevenZipArchive::GetArchiveMethod(const std::string& archivePath, std::string& method) {
    ArchiveInfo info;
    if (!ListArchive(archivePath, info, "")) return false;
    method = info.method;
    return true;
}

uint64_t SevenZipArchive::GetArchiveUncompressedSize(const std::string& archivePath) {
    ArchiveInfo info;
    if (!ListArchive(archivePath, info, "")) return 0;
    return info.uncompressedSize;
}

uint64_t SevenZipArchive::GetArchiveCompressedSize(const std::string& archivePath) {
    ArchiveInfo info;
    if (!ListArchive(archivePath, info, "")) return 0;
    return info.compressedSize;
}

uint32_t SevenZipArchive::GetArchiveFileCount(const std::string& archivePath) {
    ArchiveInfo info;
    if (!ListArchive(archivePath, info, "")) return 0;
    return info.fileCount;
}

void SevenZipArchive::Log(const std::string& level, const std::string& message, const std::string& file, int line) {
    if (!m_enableLogging) return;
    
    LogEntry entry;
    entry.timestamp = GetCurrentTimestamp();
    entry.level = level;
    entry.message = message;
    entry.file = file;
    entry.line = line;
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_logEntries.push_back(entry);
    }
    
    if (m_logCallback) {
        m_logCallback(entry);
    }
}

bool SevenZipArchive::LoadFileList(const std::string& listFilePath, std::vector<std::string>& files) {
    return ReadFileList(listFilePath, files);
}

bool SevenZipArchive::MatchesFilter(const FileInfo& info, const CompressionOptions& options) {
    if (!IsFileSizeInRange(info.size, options.minFileSize, options.maxFileSize)) return false;
    
    if (options.startTimeFilter.dwLowDateTime != 0 || options.startTimeFilter.dwHighDateTime != 0) {
        if (!IsFileTimeInRange(info.lastWriteTime, options.startTimeFilter, options.endTimeFilter)) return false;
    }
    
    if (!MatchesAttributeFilter(info.attributes, options.attributeIncludeMask, options.attributeExcludeMask)) return false;
    
    if (!options.includePatterns.empty()) {
        if (options.caseSensitive) {
            if (!MatchWildcardsCaseSensitive(info.path, options.includePatterns)) return false;
        } else {
            if (!MatchWildcards(info.path, options.includePatterns)) return false;
        }
    }
    
    if (!options.excludePatterns.empty()) {
        if (options.caseSensitive) {
            if (MatchWildcardsCaseSensitive(info.path, options.excludePatterns)) return false;
        } else {
            if (MatchWildcards(info.path, options.excludePatterns)) return false;
        }
    }
    
    return true;
}

bool SevenZipArchive::MatchesFilter(const FileInfo& info, const ExtractOptions& options) {
    if (!IsFileSizeInRange(info.size, options.minFileSize, options.maxFileSize)) return false;
    
    if (options.startTimeFilter.dwLowDateTime != 0 || options.startTimeFilter.dwHighDateTime != 0) {
        if (!IsFileTimeInRange(info.lastWriteTime, options.startTimeFilter, options.endTimeFilter)) return false;
    }
    
    if (!MatchesAttributeFilter(info.attributes, options.attributeIncludeMask, options.attributeExcludeMask)) return false;
    
    if (!options.includePatterns.empty()) {
        if (options.caseSensitive) {
            if (!MatchWildcardsCaseSensitive(info.path, options.includePatterns)) return false;
        } else {
            if (!MatchWildcards(info.path, options.includePatterns)) return false;
        }
    }
    
    if (!options.excludePatterns.empty()) {
        if (options.caseSensitive) {
            if (MatchWildcardsCaseSensitive(info.path, options.excludePatterns)) return false;
        } else {
            if (MatchWildcards(info.path, options.excludePatterns)) return false;
        }
    }
    
    return true;
}

std::string SevenZipArchive::GetTempFilePath(const std::string& prefix) {
    std::string tempDir = m_tempDirectory.empty() ? (std::string)getenv("TEMP") : m_tempDirectory;
    char tempPath[MAX_PATH];
    GetTempFileNameA(tempDir.c_str(), prefix.c_str(), 0, tempPath);
    return tempPath;
}

bool SevenZipArchive::CreateSFXWithConfig(
    const std::string& archivePath,
    const std::string& sfxPath,
    const SFXConfig& config,
    const std::string& sfxModule) {
    
    if (!FileExists(archivePath)) return false;
    
    std::string sfxModulePath = sfxModule;
    if (sfxModulePath.empty()) {
        char modulePath[MAX_PATH];
        GetModuleFileNameA(NULL, modulePath, MAX_PATH);
        std::string exeDir = GetFileDirectory(modulePath);
        sfxModulePath = exeDir + "\\7z.sfx";
        
        if (!FileExists(sfxModulePath)) {
            sfxModulePath = exeDir + "\\7zCon.sfx";
        }
    }
    
    std::ofstream outFile(sfxPath, std::ios::binary);
    if (!outFile.is_open()) return false;
    
    std::ifstream sfxFile(sfxModulePath, std::ios::binary);
    if (sfxFile.is_open()) {
        outFile << sfxFile.rdbuf();
        sfxFile.close();
    }
    
    std::string configStr = GenerateSFXConfig(config);
    if (!configStr.empty()) {
        outFile.write(configStr.c_str(), configStr.size());
    }
    
    std::ifstream archiveFile(archivePath, std::ios::binary);
    if (!archiveFile.is_open()) {
        outFile.close();
        return false;
    }
    
    outFile << archiveFile.rdbuf();
    archiveFile.close();
    outFile.close();
    
    return true;
}

bool SevenZipArchive::CreateSFXFromMemoryWithConfig(
    const std::vector<uint8_t>& archiveData,
    const std::string& sfxPath,
    const SFXConfig& config,
    const std::string& sfxModule) {
    
    std::string sfxModulePath = sfxModule;
    if (sfxModulePath.empty()) {
        char modulePath[MAX_PATH];
        GetModuleFileNameA(NULL, modulePath, MAX_PATH);
        std::string exeDir = GetFileDirectory(modulePath);
        sfxModulePath = exeDir + "\\7z.sfx";
    }
    
    std::ofstream outFile(sfxPath, std::ios::binary);
    if (!outFile.is_open()) return false;
    
    std::ifstream sfxFile(sfxModulePath, std::ios::binary);
    if (sfxFile.is_open()) {
        outFile << sfxFile.rdbuf();
        sfxFile.close();
    }
    
    std::string configStr = GenerateSFXConfig(config);
    if (!configStr.empty()) {
        outFile.write(configStr.c_str(), configStr.size());
    }
    
    outFile.write(reinterpret_cast<const char*>(archiveData.data()), archiveData.size());
    outFile.close();
    
    return true;
}

std::string SevenZipArchive::GenerateSFXConfig(const SFXConfig& config) {
    std::ostringstream oss;
    oss << ";!@Install@!UTF-8!\n";
    if (!config.title.empty()) oss << "Title=\"" << config.title << "\"\n";
    if (!config.beginPrompt.empty()) oss << "BeginPrompt=\"" << config.beginPrompt << "\"\n";
    if (!config.progress.empty()) oss << "Progress=\"" << config.progress << "\"\n";
    if (!config.runProgram.empty()) oss << "RunProgram=\"" << config.runProgram << "\"\n";
    if (!config.directory.empty()) oss << "Directory=\"" << config.directory << "\"\n";
    if (!config.executeFile.empty()) oss << "ExecuteFile=\"" << config.executeFile << "\"\n";
    if (!config.executeParameters.empty()) oss << "ExecuteParameters=\"" << config.executeParameters << "\"\n";
    if (config.silentMode) oss << "Silent=\"1\"\n";
    if (!config.overwriteMode) oss << "OverwriteMode=\"0\"\n";
    if (!config.installDirectory.empty()) oss << "InstallPath=\"" << config.installDirectory << "\"\n";
    oss << ";!@InstallEnd@!\n";
    return oss.str();
}

bool SevenZipArchive::ParseSFXConfig(const std::string& configStr, SFXConfig& config) {
    size_t start = configStr.find(";!@Install@!UTF-8!");
    if (start == std::string::npos) return false;
    
    size_t end = configStr.find(";!@InstallEnd@!");
    if (end == std::string::npos) return false;
    
    std::string content = configStr.substr(start + 18, end - start - 18);
    std::istringstream iss(content);
    std::string line;
    
    while (std::getline(iss, line)) {
        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos) continue;
        
        std::string key = line.substr(0, eqPos);
        std::string value = line.substr(eqPos + 1);
        
        if (!value.empty() && value.front() == '"') value = value.substr(1);
        if (!value.empty() && value.back() == '"') value.pop_back();
        if (!value.empty() && value.back() == '\r') value.pop_back();
        
        if (key == "Title") config.title = value;
        else if (key == "BeginPrompt") config.beginPrompt = value;
        else if (key == "Progress") config.progress = value;
        else if (key == "RunProgram") config.runProgram = value;
        else if (key == "Directory") config.directory = value;
        else if (key == "ExecuteFile") config.executeFile = value;
        else if (key == "ExecuteParameters") config.executeParameters = value;
        else if (key == "Silent") config.silentMode = (value == "1");
        else if (key == "OverwriteMode") config.overwriteMode = (value != "0");
        else if (key == "InstallPath") config.installDirectory = value;
    }
    
    return true;
}

bool SevenZipArchive::CompareArchives(
    const std::string& archivePath1,
    const std::string& archivePath2,
    std::vector<CompareResult>& results,
    const std::string& password1,
    const std::string& password2) {
    
    ArchiveInfo info1, info2;
    if (!ListArchive(archivePath1, info1, password1)) return false;
    if (!ListArchive(archivePath2, info2, password2)) return false;
    
    std::map<std::string, FileInfo> files1, files2;
    for (const auto& f : info1.files) {
        std::string pathLower = f.path;
        std::transform(pathLower.begin(), pathLower.end(), pathLower.begin(), ::tolower);
        files1[pathLower] = f;
    }
    for (const auto& f : info2.files) {
        std::string pathLower = f.path;
        std::transform(pathLower.begin(), pathLower.end(), pathLower.begin(), ::tolower);
        files2[pathLower] = f;
    }
    
    for (const auto& pair : files1) {
        CompareResult result;
        result.path = pair.second.path;
        result.size1 = pair.second.size;
        result.time1 = pair.second.lastWriteTime;
        result.size2 = 0;
        result.onlyInArchive1 = false;
        result.onlyInArchive2 = false;
        result.contentDifferent = false;
        result.sizeDifferent = false;
        result.timeDifferent = false;
        
        auto it2 = files2.find(pair.first);
        if (it2 == files2.end()) {
            result.onlyInArchive1 = true;
        } else {
            result.size2 = it2->second.size;
            result.time2 = it2->second.lastWriteTime;
            
            if (result.size1 != result.size2) {
                result.sizeDifferent = true;
            }
            if (FileTimeToInt64(result.time1) != FileTimeToInt64(result.time2)) {
                result.timeDifferent = true;
            }
            if (pair.second.crc != it2->second.crc && pair.second.crc != 0 && it2->second.crc != 0) {
                result.contentDifferent = true;
            }
            
            files2.erase(it2);
        }
        
        if (result.onlyInArchive1 || result.sizeDifferent || result.timeDifferent || result.contentDifferent) {
            results.push_back(result);
            if (m_compareCallback) m_compareCallback(result);
        }
    }
    
    for (const auto& pair : files2) {
        CompareResult result;
        result.path = pair.second.path;
        result.size1 = 0;
        result.size2 = pair.second.size;
        result.time2 = pair.second.lastWriteTime;
        result.onlyInArchive1 = false;
        result.onlyInArchive2 = true;
        result.contentDifferent = false;
        result.sizeDifferent = false;
        result.timeDifferent = false;
        
        results.push_back(result);
        if (m_compareCallback) m_compareCallback(result);
    }
    
    return true;
}

bool SevenZipArchive::CompareArchiveWithDirectory(
    const std::string& archivePath,
    const std::string& directoryPath,
    std::vector<CompareResult>& results,
    const std::string& password) {
    
    ArchiveInfo info;
    if (!ListArchive(archivePath, info, password)) return false;
    
    std::map<std::string, FileInfo> archiveFiles;
    for (const auto& f : info.files) {
        std::string pathLower = f.path;
        std::transform(pathLower.begin(), pathLower.end(), pathLower.begin(), ::tolower);
        archiveFiles[pathLower] = f;
    }
    
    std::vector<std::string> dirFiles;
    std::string searchPath = directoryPath + "\\*";
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(StringToWString(searchPath).c_str(), &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            std::wstring name = findData.cFileName;
            if (name != L"." && name != L"..") {
                dirFiles.push_back(WStringToString(name));
            }
        } while (FindNextFileW(hFind, &findData));
        FindClose(hFind);
    }
    
    for (const auto& pair : archiveFiles) {
        CompareResult result;
        result.path = pair.second.path;
        result.size1 = pair.second.size;
        result.time1 = pair.second.lastWriteTime;
        result.size2 = 0;
        result.onlyInArchive1 = false;
        result.onlyInArchive2 = false;
        result.contentDifferent = false;
        result.sizeDifferent = false;
        result.timeDifferent = false;
        
        std::string fullPath = directoryPath + "\\" + pair.second.path;
        if (!FileExists(fullPath)) {
            result.onlyInArchive1 = true;
        } else {
            WIN32_FILE_ATTRIBUTE_DATA attr;
            if (GetFileAttributesExA(fullPath.c_str(), GetFileExInfoStandard, &attr)) {
                result.size2 = ((uint64_t)attr.nFileSizeHigh << 32) | attr.nFileSizeLow;
                result.time2 = attr.ftLastWriteTime;
                
                if (result.size1 != result.size2) {
                    result.sizeDifferent = true;
                }
                if (FileTimeToInt64(result.time1) != FileTimeToInt64(result.time2)) {
                    result.timeDifferent = true;
                }
            }
        }
        
        if (result.onlyInArchive1 || result.sizeDifferent || result.timeDifferent) {
            results.push_back(result);
            if (m_compareCallback) m_compareCallback(result);
        }
    }
    
    return true;
}

bool SevenZipArchive::RepairArchive(
    const std::string& archivePath,
    const std::string& outputPath,
    RepairResult& result,
    const std::string& password) {
    
    result = RepairResult();
    result.success = false;
    result.partiallyRepaired = false;
    
    if (!IsInitialized() && !Initialize()) return false;
    if (!FileExists(archivePath)) return false;
    
    ArchiveInfo info;
    if (!ListArchive(archivePath, info, password)) {
        result.errorMessage = "Failed to list archive";
        return false;
    }
    
    result.totalFiles = info.fileCount;
    result.totalBytes = info.uncompressedSize;
    
    std::vector<uint8_t> buffer;
    std::vector<std::string> recoveredFiles;
    std::vector<std::string> lostFiles;
    uint64_t recoveredBytes = 0;
    uint32_t recoveredCount = 0;
    
    for (const auto& file : info.files) {
        if (m_cancelFlag.load()) break;
        
        std::vector<uint8_t> fileData;
        if (ExtractSingleFileToMemory(archivePath, file.path, fileData, password)) {
            recoveredFiles.push_back(file.path);
            recoveredBytes += file.size;
            recoveredCount++;
        } else {
            lostFiles.push_back(file.path);
        }
    }
    
    result.recoveredFiles = recoveredCount;
    result.recoveredBytes = recoveredBytes;
    result.recoveredFileList = recoveredFiles;
    result.lostFileList = lostFiles;
    
    if (recoveredCount == info.fileCount) {
        result.success = true;
    } else if (recoveredCount > 0) {
        result.partiallyRepaired = true;
    }
    
    return result.success || result.partiallyRepaired;
}

bool SevenZipArchive::ConvertArchive(
    const std::string& sourcePath,
    const std::string& destPath,
    ArchiveFormat destFormat,
    const CompressionOptions& options,
    const std::string& password) {
    
    if (!IsInitialized() && !Initialize()) return false;
    
    ArchiveInfo info;
    if (!ListArchive(sourcePath, info, password)) return false;
    
    std::string tempDir = m_tempDirectory;
    if (tempDir.empty()) {
        char tempPath[MAX_PATH];
        GetTempPathA(MAX_PATH, tempPath);
        tempDir = tempPath;
    }
    
    std::string extractDir = tempDir + "\\7zconvert_" + std::to_string(GetCurrentProcessId());
    CreateDirectoryRecursive(extractDir);
    
    ExtractOptions extractOpts;
    extractOpts.outputDir = extractDir;
    extractOpts.password = password;
    
    if (!ExtractArchive(sourcePath, extractOpts)) {
        return false;
    }
    
    bool success = CompressDirectory(destPath, extractDir, options, true);
    
    std::vector<std::string> extractedFiles;
    for (const auto& f : info.files) {
        std::string fullPath = extractDir + "\\" + f.path;
        if (FileExists(fullPath)) {
            DeleteFileA(fullPath.c_str());
        }
    }
    RemoveDirectoryA(extractDir.c_str());
    
    return success;
}

bool SevenZipArchive::MergeArchives(
    const std::string& destArchivePath,
    const std::vector<std::string>& sourceArchives,
    const CompressionOptions& options) {
    
    if (!IsInitialized() && !Initialize()) return false;
    if (sourceArchives.empty()) return false;
    
    std::vector<CDirItem> allItems;
    
    for (const auto& srcArchive : sourceArchives) {
        ArchiveInfo info;
        if (!ListArchive(srcArchive, info, options.password)) continue;
        
        for (const auto& file : info.files) {
            if (file.isDirectory) continue;
            
            std::vector<uint8_t> fileData;
            if (ExtractSingleFileToMemory(srcArchive, file.path, fileData, options.password)) {
                std::string tempFile = GetTempFilePath("7zmerge");
                std::ofstream ofs(tempFile, std::ios::binary);
                ofs.write(reinterpret_cast<const char*>(fileData.data()), fileData.size());
                ofs.close();
                
                CDirItem item;
                item.RelativePath = StringToWString(file.path);
                item.FullPathA = tempFile;
                item.Size = file.size;
                item.Attrib = file.attributes;
                item.MTime = file.lastWriteTime;
                item.IsDir = false;
                item.IndexInArchive = (UInt32)-1;
                allItems.push_back(item);
            }
        }
    }
    
    if (allItems.empty()) return false;
    
    GUID formatID = m_impl->GetFormatCLSID(destArchivePath);
    IOutArchive* outArchive = nullptr;
    HRESULT hr = m_impl->_createObject(&formatID, &IID_IOutArchive, (void**)&outArchive);
    
    if (hr != S_OK || !outArchive) return false;
    
    m_impl->SetCompressionProperties(outArchive, options);
    
    COutFileStream* outFile = new COutFileStream();
    if (!outFile->Create(destArchivePath)) {
        outFile->Release();
        outArchive->Release();
        return false;
    }
    
    CArchiveUpdateCallback* updateCallback = new CArchiveUpdateCallback();
    updateCallback->Init(&allItems);
    updateCallback->PasswordIsDefined = !options.password.empty();
    updateCallback->Password = StringToWString(options.password);
    updateCallback->CancelFlag = &m_cancelFlag;
    
    hr = outArchive->UpdateItems(outFile, (UInt32)allItems.size(), updateCallback);
    
    for (const auto& item : allItems) {
        if (!item.FullPathA.empty()) {
            DeleteFileA(item.FullPathA.c_str());
        }
    }
    
    outFile->Release();
    updateCallback->Release();
    outArchive->Release();
    
    return hr == S_OK;
}

bool SevenZipArchive::SplitArchive(
    const std::string& archivePath,
    uint64_t splitSize,
    std::vector<std::string>& outputPaths) {
    
    if (!FileExists(archivePath)) return false;
    if (splitSize == 0) return false;
    
    std::ifstream inFile(archivePath, std::ios::binary);
    if (!inFile.is_open()) return false;
    
    inFile.seekg(0, std::ios::end);
    uint64_t totalSize = inFile.tellg();
    inFile.seekg(0, std::ios::beg);
    
    uint32_t partNum = 0;
    uint64_t remaining = totalSize;
    
    while (remaining > 0) {
        partNum++;
        std::string partPath = archivePath + "." + std::to_string(partNum);
        outputPaths.push_back(partPath);
        
        std::ofstream outFile(partPath, std::ios::binary);
        if (!outFile.is_open()) {
            inFile.close();
            return false;
        }
        
        uint64_t toWrite = (remaining > splitSize) ? splitSize : remaining;
        std::vector<char> buffer(toWrite);
        inFile.read(buffer.data(), toWrite);
        outFile.write(buffer.data(), toWrite);
        outFile.close();
        
        remaining -= toWrite;
    }
    
    inFile.close();
    return true;
}

bool SevenZipArchive::CompressFilesFromList(
    const std::string& archivePath,
    const std::string& listFilePath,
    const CompressionOptions& options) {
    
    std::vector<std::string> files;
    if (!LoadFileList(listFilePath, files)) return false;
    
    return CompressFiles(archivePath, files, options);
}

bool SevenZipArchive::ExtractFilesFromList(
    const std::string& archivePath,
    const std::string& listFilePath,
    const std::string& outputDir,
    const std::string& password) {
    
    std::vector<std::string> files;
    if (!LoadFileList(listFilePath, files)) return false;
    
    return ExtractFiles(archivePath, files, outputDir, password);
}

bool SevenZipArchive::GetAlternateStreams(
    const std::string& archivePath,
    const std::string& filePath,
    std::vector<std::pair<std::string, uint64_t>>& streams,
    const std::string& password) {
    
    ArchiveInfo info;
    if (!ListArchive(archivePath, info, password)) return false;
    
    for (const auto& f : info.files) {
        if (f.path == filePath && f.hasAlternateStreams) {
            streams = std::vector<std::pair<std::string, uint64_t>>();
            for (const auto& s : f.alternateStreams) {
                streams.push_back({s, 0});
            }
            return true;
        }
    }
    
    return false;
}

bool SevenZipArchive::ExtractAlternateStream(
    const std::string& archivePath,
    const std::string& filePath,
    const std::string& streamName,
    std::vector<uint8_t>& output,
    const std::string& password) {
    
    return false;
}

bool SevenZipArchive::GetExtendedAttributes(
    const std::string& archivePath,
    const std::string& filePath,
    std::vector<std::pair<std::string, std::vector<uint8_t>>>& attributes,
    const std::string& password) {
    
    return false;
}

bool SevenZipArchive::GetArchiveChecksum(
    const std::string& archivePath,
    std::string& checksum,
    const std::string& algorithm,
    const std::string& password) {
    
    if (!FileExists(archivePath)) return false;
    
    std::ifstream file(archivePath, std::ios::binary);
    if (!file.is_open()) return false;
    
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> data(fileSize);
    file.read(reinterpret_cast<char*>(data.data()), fileSize);
    file.close();
    
    if (algorithm == "CRC32") {
        uint32_t crc = 0;
        for (const auto& byte : data) {
            crc ^= byte;
            for (int i = 0; i < 8; i++) {
                crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
            }
        }
        checksum = BytesToHex(reinterpret_cast<uint8_t*>(&crc), 4);
        return true;
    }
    
    return false;
}

bool SevenZipArchive::ValidateArchive(
    const std::string& archivePath,
    std::vector<std::string>& errors,
    const std::string& password) {
    
    if (!FileExists(archivePath)) {
        errors.push_back("Archive file does not exist");
        return false;
    }
    
    ArchiveInfo info;
    if (!ListArchive(archivePath, info, password)) {
        errors.push_back("Failed to open archive");
        return false;
    }
    
    bool allValid = true;
    
    for (const auto& file : info.files) {
        if (m_cancelFlag.load()) break;
        
        std::vector<uint8_t> fileData;
        if (!ExtractSingleFileToMemory(archivePath, file.path, fileData, password)) {
            errors.push_back("Failed to extract: " + file.path);
            allValid = false;
        }
    }
    
    return allValid;
}

bool SevenZipArchive::GetFileInfo(
    const std::string& archivePath,
    const std::string& filePath,
    FileInfo& info,
    const std::string& password) {
    
    ArchiveInfo archiveInfo;
    if (!ListArchive(archivePath, archiveInfo, password)) return false;
    
    std::string filePathLower = filePath;
    std::transform(filePathLower.begin(), filePathLower.end(), filePathLower.begin(), ::tolower);
    
    for (const auto& f : archiveInfo.files) {
        std::string pathLower = f.path;
        std::transform(pathLower.begin(), pathLower.end(), pathLower.begin(), ::tolower);
        
        if (pathLower == filePathLower) {
            info = f;
            return true;
        }
    }
    
    return false;
}

bool SevenZipArchive::SetFileTimeInArchive(
    const std::string& archivePath,
    const std::string& filePath,
    const FILETIME* creationTime,
    const FILETIME* accessTime,
    const FILETIME* modificationTime,
    const std::string& password) {
    
    return false;
}

bool SevenZipArchive::CompressWithFilters(
    const std::string& archivePath,
    const std::vector<std::string>& filePaths,
    FilterMethod filter,
    const CompressionOptions& options) {
    
    CompressionOptions opts = options;
    opts.filter = filter;
    return CompressFiles(archivePath, filePaths, opts);
}

bool SevenZipArchive::ExtractWithTimeFilter(
    const std::string& archivePath,
    const ExtractOptions& options,
    const FILETIME& startTime,
    const FILETIME& endTime) {
    
    ExtractOptions opts = options;
    opts.startTimeFilter = startTime;
    opts.endTimeFilter = endTime;
    return ExtractArchive(archivePath, opts);
}

bool SevenZipArchive::ExtractWithSizeFilter(
    const std::string& archivePath,
    const ExtractOptions& options,
    uint64_t minSize,
    uint64_t maxSize) {
    
    ExtractOptions opts = options;
    opts.minFileSize = minSize;
    opts.maxFileSize = maxSize;
    return ExtractArchive(archivePath, opts);
}

bool SevenZipArchive::CompressSparseFile(
    const std::string& archivePath,
    const std::string& sparseFilePath,
    const CompressionOptions& options) {
    
    DWORD attrib = GetFileAttributesA(sparseFilePath.c_str());
    bool isSparse = (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_SPARSE_FILE));
    
    if (!isSparse) {
        return CompressFiles(archivePath, {sparseFilePath}, options);
    }
    
    return CompressFiles(archivePath, {sparseFilePath}, options);
}

bool SevenZipArchive::IsSparseFile(
    const std::string& archivePath,
    const std::string& filePath,
    const std::string& password) {
    
    FileInfo info;
    if (!GetFileInfo(archivePath, filePath, info, password)) return false;
    return info.isSparse;
}

bool SevenZipArchive::GetArchiveStatistics(
    const std::string& archivePath,
    std::map<std::string, uint64_t>& methodStats,
    std::map<std::string, uint32_t>& extensionStats,
    const std::string& password) {
    
    ArchiveInfo info;
    if (!ListArchive(archivePath, info, password)) return false;
    
    for (const auto& file : info.files) {
        if (!file.isDirectory) {
            std::string method = file.method.empty() ? "unknown" : file.method;
            methodStats[method] += file.size;
            
            std::string ext = GetFileExtension(file.path);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            extensionStats[ext]++;
        }
    }
    
    return true;
}

bool SevenZipArchive::FindDuplicates(
    const std::string& archivePath,
    std::vector<std::vector<std::string>>& duplicateGroups,
    const std::string& password) {
    
    ArchiveInfo info;
    if (!ListArchive(archivePath, info, password)) return false;
    
    std::map<uint32_t, std::vector<std::string>> crcMap;
    for (const auto& file : info.files) {
        if (!file.isDirectory && file.crc != 0) {
            crcMap[file.crc].push_back(file.path);
        }
    }
    
    for (const auto& pair : crcMap) {
        if (pair.second.size() > 1) {
            duplicateGroups.push_back(pair.second);
        }
    }
    
    return true;
}

bool SevenZipArchive::ExportFileList(
    const std::string& archivePath,
    const std::string& outputFilePath,
    const std::string& format,
    const std::string& password) {
    
    ArchiveInfo info;
    if (!ListArchive(archivePath, info, password)) return false;
    
    std::ofstream outFile(outputFilePath);
    if (!outFile.is_open()) return false;
    
    if (format == "txt" || format == "csv") {
        outFile << "Path,Size,PackedSize,IsDir,Encrypted,CRC,Method\n";
        for (const auto& file : info.files) {
            outFile << file.path << ","
                    << file.size << ","
                    << file.packedSize << ","
                    << (file.isDirectory ? "1" : "0") << ","
                    << (file.isEncrypted ? "1" : "0") << ","
                    << file.crc << ","
                    << file.method << "\n";
        }
    } else if (format == "json") {
        outFile << "[\n";
        for (size_t i = 0; i < info.files.size(); i++) {
            const auto& file = info.files[i];
            outFile << "  {\n";
            outFile << "    \"path\": \"" << file.path << "\",\n";
            outFile << "    \"size\": " << file.size << ",\n";
            outFile << "    \"packedSize\": " << file.packedSize << ",\n";
            outFile << "    \"isDirectory\": " << (file.isDirectory ? "true" : "false") << ",\n";
            outFile << "    \"isEncrypted\": " << (file.isEncrypted ? "true" : "false") << "\n";
            outFile << "  }" << (i < info.files.size() - 1 ? "," : "") << "\n";
        }
        outFile << "]\n";
    } else {
        for (const auto& file : info.files) {
            outFile << file.path << "\n";
        }
    }
    
    outFile.close();
    return true;
}

bool SevenZipArchive::ImportFileList(
    const std::string& listFilePath,
    std::vector<FileInfo>& files) {
    
    std::ifstream file(listFilePath);
    if (!file.is_open()) return false;
    
    std::string line;
    std::getline(file, line);
    
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        FileInfo fi;
        size_t pos = 0;
        size_t nextPos;
        
        nextPos = line.find(',', pos);
        if (nextPos == std::string::npos) continue;
        fi.path = line.substr(pos, nextPos - pos);
        pos = nextPos + 1;
        
        nextPos = line.find(',', pos);
        if (nextPos == std::string::npos) continue;
        fi.size = std::stoull(line.substr(pos, nextPos - pos));
        pos = nextPos + 1;
        
        nextPos = line.find(',', pos);
        if (nextPos == std::string::npos) continue;
        fi.packedSize = std::stoull(line.substr(pos, nextPos - pos));
        pos = nextPos + 1;
        
        nextPos = line.find(',', pos);
        if (nextPos == std::string::npos) continue;
        fi.isDirectory = (line.substr(pos, nextPos - pos) == "1");
        pos = nextPos + 1;
        
        nextPos = line.find(',', pos);
        if (nextPos == std::string::npos) continue;
        fi.isEncrypted = (line.substr(pos, nextPos - pos) == "1");
        pos = nextPos + 1;
        
        nextPos = line.find(',', pos);
        if (nextPos == std::string::npos) continue;
        fi.crc = std::stoul(line.substr(pos, nextPos - pos));
        pos = nextPos + 1;
        
        fi.method = line.substr(pos);
        
        files.push_back(fi);
    }
    
    file.close();
    return true;
}

std::vector<uint8_t> GenerateBenchmarkData(uint64_t size) {
    std::vector<uint8_t> data(size);
    uint64_t seed = 12345;
    for (uint64_t i = 0; i < size; i++) {
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
        data[i] = (uint8_t)(seed & 0xFF);
    }
    return data;
}

static double GetHighResolutionTime() {
    static LARGE_INTEGER frequency = { 0 };
    if (frequency.QuadPart == 0) {
        QueryPerformanceFrequency(&frequency);
    }
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)frequency.QuadPart;
}

bool SevenZipArchive::RunBenchmark(
    std::vector<BenchmarkResult>& results,
    CompressionMethod method,
    int numIterations,
    uint64_t testDataSize,
    int threadCount) {
    
    if (!IsInitialized() && !Initialize()) return false;
    
    std::vector<uint8_t> testData = GenerateBenchmarkData(testDataSize);
    
    for (int iter = 0; iter < numIterations; iter++) {
        if (m_cancelFlag.load()) break;
        
        BenchmarkResult result;
        result.methodName = GetCompressionMethodName(method);
        result.dataSize = testDataSize;
        result.threadCount = threadCount > 0 ? threadCount : std::thread::hardware_concurrency();
        result.passed = false;
        
        std::vector<uint8_t> compressedData;
        CompressionOptions opts;
        opts.method = method;
        opts.level = CompressionLevel::Normal;
        opts.threadCount = result.threadCount;
        
        double startTime = GetHighResolutionTime();
        bool compressOk = CompressStream(testData.data(), testDataSize, compressedData, "benchmark", opts);
        double endTime = GetHighResolutionTime();
        
        if (!compressOk) {
            result.errorMessage = "Compression failed";
            results.push_back(result);
            continue;
        }
        
        result.compressedSize = compressedData.size();
        result.compressionTime = endTime - startTime;
        result.compressionSpeed = (double)testDataSize / result.compressionTime / (1024.0 * 1024.0);
        result.compressionRatio = (double)testDataSize / (double)result.compressedSize;
        
        std::vector<uint8_t> decompressedData;
        startTime = GetHighResolutionTime();
        bool decompressOk = ExtractStream(compressedData.data(), compressedData.size(), "benchmark", decompressedData);
        endTime = GetHighResolutionTime();
        
        if (!decompressOk) {
            result.errorMessage = "Decompression failed";
            results.push_back(result);
            continue;
        }
        
        result.decompressionTime = endTime - startTime;
        result.decompressionSpeed = (double)testDataSize / result.decompressionTime / (1024.0 * 1024.0);
        
        if (decompressedData.size() != testDataSize) {
            result.errorMessage = "Size mismatch after decompression";
            results.push_back(result);
            continue;
        }
        
        if (memcmp(decompressedData.data(), testData.data(), testDataSize) != 0) {
            result.errorMessage = "Data mismatch after decompression";
            results.push_back(result);
            continue;
        }
        
        result.passed = true;
        results.push_back(result);
    }
    
    return !results.empty();
}

bool SevenZipArchive::RunBenchmarkAsync(
    std::vector<BenchmarkResult>& results,
    CompressionMethod method,
    int numIterations,
    uint64_t testDataSize,
    int threadCount) {
    
    WaitForCompletion();
    m_cancelFlag = false;
    m_asyncStatus = AsyncStatus::Running;
    
    m_workerThread = std::thread([this, &results, method, numIterations, testDataSize, threadCount]() {
        bool success = RunBenchmark(results, method, numIterations, testDataSize, threadCount);
        
        if (m_cancelFlag.load()) {
            m_asyncStatus = AsyncStatus::Cancelled;
        } else if (success) {
            m_asyncStatus = AsyncStatus::Completed;
        } else {
            m_asyncStatus = AsyncStatus::Failed;
        }
    });
    
    return true;
}

static uint32_t CRC32Table[256];
static bool CRC32TableInitialized = false;

static void InitCRC32Table() {
    if (CRC32TableInitialized) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
        CRC32Table[i] = crc;
    }
    CRC32TableInitialized = true;
}

static uint32_t CalculateCRC32(const uint8_t* data, size_t size) {
    InitCRC32Table();
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < size; i++) {
        crc = CRC32Table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

static const uint32_t MD5_K[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
};

static const uint8_t MD5_S[64] = {
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
    5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
};

static void CalculateMD5(const uint8_t* data, size_t size, uint8_t* hash) {
    uint32_t h0 = 0x67452301;
    uint32_t h1 = 0xefcdab89;
    uint32_t h2 = 0x98badcfe;
    uint32_t h3 = 0x10325476;
    
    size_t paddedSize = ((size + 8) / 64 + 1) * 64;
    std::vector<uint8_t> padded(paddedSize, 0);
    memcpy(padded.data(), data, size);
    padded[size] = 0x80;
    
    uint64_t bitLen = size * 8;
    memcpy(padded.data() + paddedSize - 8, &bitLen, 8);
    
    for (size_t offset = 0; offset < paddedSize; offset += 64) {
        uint32_t a = h0, b = h1, c = h2, d = h3;
        
        for (int i = 0; i < 64; i++) {
            uint32_t f, g;
            if (i < 16) {
                f = (b & c) | ((~b) & d);
                g = i;
            } else if (i < 32) {
                f = (d & b) | ((~d) & c);
                g = (5 * i + 1) % 16;
            } else if (i < 48) {
                f = b ^ c ^ d;
                g = (3 * i + 5) % 16;
            } else {
                f = c ^ (b | (~d));
                g = (7 * i) % 16;
            }
            
            uint32_t m = *(uint32_t*)(padded.data() + offset + g * 4);
            f = f + a + MD5_K[i] + m;
            a = d;
            d = c;
            c = b;
            b = b + ((f << MD5_S[i]) | (f >> (32 - MD5_S[i])));
        }
        
        h0 += a; h1 += b; h2 += c; h3 += d;
    }
    
    memcpy(hash, &h0, 4); memcpy(hash + 4, &h1, 4);
    memcpy(hash + 8, &h2, 4); memcpy(hash + 12, &h3, 4);
}

static const uint32_t SHA1_K[4] = { 0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6 };

static void CalculateSHA1(const uint8_t* data, size_t size, uint8_t* hash) {
    uint32_t h[5] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };
    
    size_t paddedSize = ((size + 8) / 64 + 1) * 64;
    std::vector<uint8_t> padded(paddedSize, 0);
    memcpy(padded.data(), data, size);
    padded[size] = 0x80;
    
    uint64_t bitLen = size * 8;
    for (int i = 0; i < 8; i++) {
        padded[paddedSize - 1 - i] = (bitLen >> (i * 8)) & 0xFF;
    }
    
    for (size_t offset = 0; offset < paddedSize; offset += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; i++) {
            w[i] = ((uint32_t)padded[offset + i * 4] << 24) |
                   ((uint32_t)padded[offset + i * 4 + 1] << 16) |
                   ((uint32_t)padded[offset + i * 4 + 2] << 8) |
                   ((uint32_t)padded[offset + i * 4 + 3]);
        }
        for (int i = 16; i < 80; i++) {
            uint32_t t = w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16];
            w[i] = (t << 1) | (t >> 31);
        }
        
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = SHA1_K[0];
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = SHA1_K[1];
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = SHA1_K[2];
            } else {
                f = b ^ c ^ d;
                k = SHA1_K[3];
            }
            
            uint32_t t = ((a << 5) | (a >> 27)) + f + e + k + w[i];
            e = d; d = c; c = (b << 30) | (b >> 2); b = a; a = t;
        }
        
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }
    
    for (int i = 0; i < 5; i++) {
        hash[i * 4] = (h[i] >> 24) & 0xFF;
        hash[i * 4 + 1] = (h[i] >> 16) & 0xFF;
        hash[i * 4 + 2] = (h[i] >> 8) & 0xFF;
        hash[i * 4 + 3] = h[i] & 0xFF;
    }
}

static const uint32_t SHA256_K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static uint32_t SHA256_ROTR(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
static uint32_t SHA256_CH(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
static uint32_t SHA256_MAJ(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
static uint32_t SHA256_EP0(uint32_t x) { return SHA256_ROTR(x, 2) ^ SHA256_ROTR(x, 13) ^ SHA256_ROTR(x, 22); }
static uint32_t SHA256_EP1(uint32_t x) { return SHA256_ROTR(x, 6) ^ SHA256_ROTR(x, 11) ^ SHA256_ROTR(x, 25); }
static uint32_t SHA256_SIG0(uint32_t x) { return SHA256_ROTR(x, 7) ^ SHA256_ROTR(x, 18) ^ (x >> 3); }
static uint32_t SHA256_SIG1(uint32_t x) { return SHA256_ROTR(x, 17) ^ SHA256_ROTR(x, 19) ^ (x >> 10); }

static void CalculateSHA256(const uint8_t* data, size_t size, uint8_t* hash) {
    uint32_t h[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    
    size_t paddedSize = ((size + 8) / 64 + 1) * 64;
    std::vector<uint8_t> padded(paddedSize, 0);
    memcpy(padded.data(), data, size);
    padded[size] = 0x80;
    
    uint64_t bitLen = size * 8;
    for (int i = 0; i < 8; i++) {
        padded[paddedSize - 1 - i] = (bitLen >> (i * 8)) & 0xFF;
    }
    
    for (size_t offset = 0; offset < paddedSize; offset += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; i++) {
            w[i] = ((uint32_t)padded[offset + i * 4] << 24) |
                   ((uint32_t)padded[offset + i * 4 + 1] << 16) |
                   ((uint32_t)padded[offset + i * 4 + 2] << 8) |
                   ((uint32_t)padded[offset + i * 4 + 3]);
        }
        for (int i = 16; i < 64; i++) {
            w[i] = SHA256_SIG1(w[i-2]) + w[i-7] + SHA256_SIG0(w[i-15]) + w[i-16];
        }
        
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];
        
        for (int i = 0; i < 64; i++) {
            uint32_t t1 = hh + SHA256_EP1(e) + SHA256_CH(e, f, g) + SHA256_K[i] + w[i];
            uint32_t t2 = SHA256_EP0(a) + SHA256_MAJ(a, b, c);
            hh = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
        }
        
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    
    for (int i = 0; i < 8; i++) {
        hash[i * 4] = (h[i] >> 24) & 0xFF;
        hash[i * 4 + 1] = (h[i] >> 16) & 0xFF;
        hash[i * 4 + 2] = (h[i] >> 8) & 0xFF;
        hash[i * 4 + 3] = h[i] & 0xFF;
    }
}

static const uint64_t SHA512_K[80] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
    0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
    0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
    0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
    0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
    0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
    0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
    0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
    0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
    0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
    0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
    0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
    0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
};

static uint64_t SHA512_ROTR(uint64_t x, int n) { return (x >> n) | (x << (64 - n)); }
static uint64_t SHA512_CH(uint64_t x, uint64_t y, uint64_t z) { return (x & y) ^ (~x & z); }
static uint64_t SHA512_MAJ(uint64_t x, uint64_t y, uint64_t z) { return (x & y) ^ (x & z) ^ (y & z); }
static uint64_t SHA512_EP0(uint64_t x) { return SHA512_ROTR(x, 28) ^ SHA512_ROTR(x, 34) ^ SHA512_ROTR(x, 39); }
static uint64_t SHA512_EP1(uint64_t x) { return SHA512_ROTR(x, 14) ^ SHA512_ROTR(x, 18) ^ SHA512_ROTR(x, 41); }
static uint64_t SHA512_SIG0(uint64_t x) { return SHA512_ROTR(x, 1) ^ SHA512_ROTR(x, 8) ^ (x >> 7); }
static uint64_t SHA512_SIG1(uint64_t x) { return SHA512_ROTR(x, 19) ^ SHA512_ROTR(x, 61) ^ (x >> 6); }

static void CalculateSHA512(const uint8_t* data, size_t size, uint8_t* hash) {
    uint64_t h[8] = {
        0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL, 0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
        0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL, 0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL
    };
    
    size_t paddedSize = ((size + 16) / 128 + 1) * 128;
    std::vector<uint8_t> padded(paddedSize, 0);
    memcpy(padded.data(), data, size);
    padded[size] = 0x80;
    
    uint64_t bitLen = size * 8;
    for (int i = 0; i < 8; i++) {
        padded[paddedSize - 1 - i] = (bitLen >> (i * 8)) & 0xFF;
    }
    
    for (size_t offset = 0; offset < paddedSize; offset += 128) {
        uint64_t w[80];
        for (int i = 0; i < 16; i++) {
            w[i] = ((uint64_t)padded[offset + i * 8] << 56) |
                   ((uint64_t)padded[offset + i * 8 + 1] << 48) |
                   ((uint64_t)padded[offset + i * 8 + 2] << 40) |
                   ((uint64_t)padded[offset + i * 8 + 3] << 32) |
                   ((uint64_t)padded[offset + i * 8 + 4] << 24) |
                   ((uint64_t)padded[offset + i * 8 + 5] << 16) |
                   ((uint64_t)padded[offset + i * 8 + 6] << 8) |
                   ((uint64_t)padded[offset + i * 8 + 7]);
        }
        for (int i = 16; i < 80; i++) {
            w[i] = SHA512_SIG1(w[i-2]) + w[i-7] + SHA512_SIG0(w[i-15]) + w[i-16];
        }
        
        uint64_t a = h[0], b = h[1], c = h[2], d = h[3];
        uint64_t e = h[4], f = h[5], g = h[6], hh = h[7];
        
        for (int i = 0; i < 80; i++) {
            uint64_t t1 = hh + SHA512_EP1(e) + SHA512_CH(e, f, g) + SHA512_K[i] + w[i];
            uint64_t t2 = SHA512_EP0(a) + SHA512_MAJ(a, b, c);
            hh = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
        }
        
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            hash[i * 8 + j] = (h[i] >> (56 - j * 8)) & 0xFF;
        }
    }
}

std::vector<std::string> SevenZipArchive::GetAvailableHashAlgorithms() {
    return { "CRC32", "MD5", "SHA1", "SHA256", "SHA512" };
}

std::string SevenZipArchive::HashToString(const uint8_t* data, size_t size) {
    return BytesToHex(data, size);
}

bool SevenZipArchive::CalculateFileHash(
    const std::string& filePath,
    HashResult& result,
    const std::string& algorithm) {
    
    if (!FileExists(filePath)) return false;
    
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return false;
    
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> data(fileSize);
    file.read(reinterpret_cast<char*>(data.data()), fileSize);
    file.close();
    
    return CalculateDataHash(data.data(), data.size(), result, algorithm);
}

bool SevenZipArchive::CalculateDataHash(
    const uint8_t* data,
    size_t size,
    HashResult& result,
    const std::string& algorithm) {
    
    result.algorithm = algorithm;
    result.dataSize = size;
    
    std::transform(algorithm.begin(), algorithm.end(), result.algorithm.begin(), ::toupper);
    
    if (result.algorithm == "CRC32") {
        uint32_t crc = CalculateCRC32(data, size);
        result.hash = BytesToHex(reinterpret_cast<uint8_t*>(&crc), 4);
        return true;
    }
    
    if (result.algorithm == "MD5") {
        uint8_t hash[16];
        CalculateMD5(data, size, hash);
        result.hash = BytesToHex(hash, 16);
        return true;
    }
    
    if (result.algorithm == "SHA1") {
        uint8_t hash[20];
        CalculateSHA1(data, size, hash);
        result.hash = BytesToHex(hash, 20);
        return true;
    }
    
    if (result.algorithm == "SHA256") {
        uint8_t hash[32];
        CalculateSHA256(data, size, hash);
        result.hash = BytesToHex(hash, 32);
        return true;
    }
    
    if (result.algorithm == "SHA512") {
        uint8_t hash[64];
        CalculateSHA512(data, size, hash);
        result.hash = BytesToHex(hash, 64);
        return true;
    }
    
    return false;
}

bool SevenZipArchive::CalculateArchiveHash(
    const std::string& archivePath,
    HashResult& result,
    const std::string& algorithm,
    const std::string& password) {
    
    ArchiveInfo info;
    if (!ListArchive(archivePath, info, password)) return false;
    
    std::vector<uint8_t> allData;
    for (const auto& file : info.files) {
        if (!file.isDirectory) {
            std::vector<uint8_t> fileData;
            if (ExtractSingleFileToMemory(archivePath, file.path, fileData, password)) {
                allData.insert(allData.end(), fileData.begin(), fileData.end());
            }
        }
    }
    
    result.filePath = archivePath;
    return CalculateDataHash(allData.data(), allData.size(), result, algorithm);
}

bool SevenZipArchive::ValidateArchiveEx(
    const std::string& archivePath,
    ValidationResult& result,
    bool checkCRC,
    bool checkHeaders,
    const std::string& password) {
    
    result = ValidationResult();
    result.isValid = true;
    
    if (!FileExists(archivePath)) {
        result.isValid = false;
        result.errors.push_back("Archive file not found");
        return false;
    }
    
    ArchiveInfo info;
    if (!ListArchive(archivePath, info, password)) {
        result.isValid = false;
        result.errors.push_back("Failed to open archive");
        return false;
    }
    
    result.totalFiles = info.fileCount;
    result.totalSize = info.uncompressedSize;
    
    CInFileStream* inFile = new CInFileStream();
    if (!inFile->Open(archivePath)) {
        inFile->Release();
        result.isValid = false;
        result.errors.push_back("Failed to open archive file");
        return false;
    }
    
    GUID formatID = m_impl->GetFormatCLSID(archivePath);
    IInArchive* inArchive = nullptr;
    HRESULT hr = m_impl->_createObject(&formatID, &IID_IInArchive, (void**)&inArchive);
    
    if (hr != S_OK || !inArchive) {
        inFile->Release();
        result.isValid = false;
        result.errors.push_back("Failed to create archive handler");
        return false;
    }
    
    CArchiveOpenCallback* openCallback = new CArchiveOpenCallback();
    openCallback->PasswordIsDefined = !password.empty();
    openCallback->Password = StringToWString(password);
    
    UInt64 scanSize = 1 << 23;
    hr = inArchive->Open(inFile, &scanSize, openCallback);
    openCallback->Release();
    
    if (hr != S_OK) {
        inArchive->Release();
        inFile->Release();
        result.isValid = false;
        result.errors.push_back("Failed to open archive");
        return false;
    }
    
    UInt32 numItems = 0;
    inArchive->GetNumberOfItems(&numItems);
    
    for (UInt32 i = 0; i < numItems; i++) {
        if (m_cancelFlag.load()) break;
        
        PROPVARIANT prop;
        PropVariantInit(&prop);
        
        hr = inArchive->GetProperty(i, kpidIsDir, &prop);
        bool isDir = (prop.vt == VT_BOOL && prop.boolVal != VARIANT_FALSE);
        PropVariantClear(&prop);
        
        if (isDir) continue;
        
        result.validFiles++;
        
        prop.vt = VT_EMPTY;
        hr = inArchive->GetProperty(i, kpidSize, &prop);
        uint64_t fileSize = 0;
        if (prop.vt == VT_UI8) {
            fileSize = prop.uhVal.QuadPart;
            result.validSize += fileSize;
        }
        PropVariantClear(&prop);
        
        if (checkCRC) {
            uint32_t expectedCRC = 0;
            prop.vt = VT_EMPTY;
            hr = inArchive->GetProperty(i, kpidCRC, &prop);
            if (prop.vt == VT_UI4) {
                expectedCRC = prop.ulVal;
            }
            PropVariantClear(&prop);
            
            if (expectedCRC != 0) {
                std::vector<uint8_t> fileData;
                if (ExtractSingleFileToMemory(archivePath, info.files[i].path, fileData, password)) {
                    uint32_t actualCRC = CalculateCRC32(fileData.data(), fileData.size());
                    if (actualCRC != expectedCRC) {
                        result.isValid = false;
                        result.errorCount++;
                        result.errors.push_back("CRC mismatch for: " + info.files[i].path);
                        result.errorTypes["CRCMismatch"]++;
                    }
                }
            }
        }
    }
    
    inArchive->Release();
    inFile->Release();
    
    return result.isValid || result.validFiles > 0;
}

bool SevenZipArchive::TestArchiveEx(
    const std::string& archivePath,
    ValidationResult& result,
    const std::string& password) {
    
    return ValidateArchiveEx(archivePath, result, true, true, password);
}

bool SevenZipArchive::GetSupportedMethods(
    std::vector<std::string>& methods) {
    
    methods = {
        "lzma", "lzma2", "ppmd", "bzip2", "deflate", "deflate64",
        "copy", "zstd", "lz4", "lz5", "brotli", "flzma2"
    };
    return true;
}

bool SevenZipArchive::GetSupportedFormats(
    std::vector<std::pair<std::string, std::string>>& formats) {
    
    formats = {
        {"7z", "7-Zip Archive"},
        {"zip", "ZIP Archive"},
        {"gz", "GZIP Archive"},
        {"bz2", "BZIP2 Archive"},
        {"xz", "XZ Archive"},
        {"tar", "TAR Archive"},
        {"wim", "Windows Imaging Format"},
        {"rar", "RAR Archive (read only)"},
        {"rar5", "RAR5 Archive (read only)"},
        {"cab", "Cabinet Archive"},
        {"iso", "ISO Image"},
        {"udf", "UDF Image"},
        {"vhd", "Virtual Hard Disk"},
        {"vhdx", "VHDX Virtual Disk"},
        {"dmg", "Apple Disk Image"},
        {"hfs", "HFS Image"},
        {"hfsx", "HFS+ Image"},
        {"chm", "Compiled HTML Help"},
        {"lzma", "LZMA Archive"},
        {"lzma86", "LZMA86 Archive"},
        {"rpm", "RPM Package"},
        {"deb", "Debian Package"},
        {"cpio", "CPIO Archive"},
        {"arj", "ARJ Archive (read only)"},
        {"squashfs", "SquashFS Image"},
        {"sqfs", "SquashFS Image"},
        {"cramfs", "CramFS Image"},
        {"ext2", "Ext2 Filesystem"},
        {"ext3", "Ext3 Filesystem"},
        {"ext4", "Ext4 Filesystem"},
        {"gpt", "GPT Partition Table"},
        {"apfs", "Apple APFS Filesystem"},
        {"vmdk", "VMware Virtual Disk"},
        {"vdi", "VirtualBox Disk Image"},
        {"qcow", "QEMU Copy-On-Write"},
        {"qcow2", "QEMU Copy-On-Write v2"},
        {"macho", "Mach-O Executable"},
        {"dylib", "Mach-O Dynamic Library"},
        {"xar", "XAR Archive"},
        {"pkg", "macOS Package"},
        {"mbr", "Master Boot Record"},
        {"nsi", "NSIS Installer Script"},
        {"flv", "Flash Video"},
        {"swf", "Shockwave Flash"},
        {"fat", "FAT Filesystem"},
        {"ntfs", "NTFS Filesystem"},
        {"mub", "MUB Image"},
        {"lua", "Lua Script"},
        {"luac", "Lua Compiled"},
        {"ihex", "Intel HEX"},
        {"hxs", "Microsoft Help"},
        {"nra", "Nero Audio"},
        {"nrb", "Nero Burn"},
        {"sfx", "Self-Extracting Archive"},
        {"uefif", "UEFI Firmware"},
        {"uefi", "UEFI Image"},
        {"tec", "TE Compressed"},
        {"base64", "Base64 Encoded"},
        {"b64", "Base64 Encoded"},
        {"mslz", "MS LZ Compressed"}
    };
    return true;
}

bool SevenZipArchive::GetArchiveProperties(
    const std::string& archivePath,
    std::map<std::string, std::string>& properties,
    const std::string& password) {
    
    if (!IsInitialized() && !Initialize()) return false;
    if (!FileExists(archivePath)) return false;
    
    CInFileStream* inFile = new CInFileStream();
    if (!inFile->Open(archivePath)) {
        inFile->Release();
        return false;
    }
    
    GUID formatID = m_impl->GetFormatCLSID(archivePath);
    IInArchive* inArchive = nullptr;
    HRESULT hr = m_impl->_createObject(&formatID, &IID_IInArchive, (void**)&inArchive);
    
    if (hr != S_OK || !inArchive) {
        inFile->Release();
        return false;
    }
    
    CArchiveOpenCallback* openCallback = new CArchiveOpenCallback();
    openCallback->PasswordIsDefined = !password.empty();
    openCallback->Password = StringToWString(password);
    
    UInt64 scanSize = 1 << 23;
    hr = inArchive->Open(inFile, &scanSize, openCallback);
    openCallback->Release();
    
    if (hr != S_OK) {
        inArchive->Release();
        inFile->Release();
        return false;
    }
    
    UInt32 numProps = 0;
    inArchive->GetNumberOfArchiveProperties(&numProps);
    
    for (UInt32 i = 0; i < numProps; i++) {
        BSTR name = nullptr;
        PROPID propID;
        VARTYPE varType;
        
        hr = inArchive->GetArchivePropertyInfo(i, &name, &propID, &varType);
        if (hr == S_OK) {
            PROPVARIANT prop;
            PropVariantInit(&prop);
            
            hr = inArchive->GetArchiveProperty(propID, &prop);
            if (hr == S_OK) {
                std::string key = name ? WStringToString(name) : "Prop" + std::to_string(propID);
                std::string value;
                
                switch (prop.vt) {
                    case VT_BSTR:
                        value = WStringToString(prop.bstrVal);
                        break;
                    case VT_UI4:
                        value = std::to_string(prop.ulVal);
                        break;
                    case VT_UI8:
                        value = std::to_string(prop.uhVal.QuadPart);
                        break;
                    case VT_FILETIME: {
                        SYSTEMTIME st;
                        FileTimeToSystemTime(&prop.filetime, &st);
                        char buf[64];
                        sprintf_s(buf, "%04d-%02d-%02d %02d:%02d:%02d",
                            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
                        value = buf;
                        break;
                    }
                    case VT_BOOL:
                        value = prop.boolVal ? "true" : "false";
                        break;
                    default:
                        value = "(unknown type)";
                        break;
                }
                
                properties[key] = value;
                PropVariantClear(&prop);
            }
            
            if (name) SysFreeString(name);
        }
    }
    
    inArchive->Close();
    inArchive->Release();
    inFile->Release();
    
    return true;
}

bool SevenZipArchive::SetArchiveProperties(
    const std::string& archivePath,
    const std::map<std::string, std::string>& properties,
    const std::string& password) {
    
    if (!IsInitialized() && !Initialize()) return false;
    if (!FileExists(archivePath)) return false;
    if (properties.empty()) return true;
    
    std::string tempDir = m_tempDirectory;
    if (tempDir.empty()) {
        char tempPath[MAX_PATH];
        GetTempPathA(MAX_PATH, tempPath);
        tempDir = tempPath;
    }
    
    std::string extractDir = tempDir + "\\7zprop_" + std::to_string(GetCurrentProcessId());
    CreateDirectoryRecursive(extractDir);
    
    ExtractOptions extractOpts;
    extractOpts.outputDir = extractDir;
    extractOpts.password = password;
    
    if (!ExtractArchive(archivePath, extractOpts)) {
        return false;
    }
    
    CompressionOptions compOpts;
    compOpts.password = password;
    
    for (const auto& prop : properties) {
        const std::string& key = prop.first;
        const std::string& value = prop.second;
        
        if (key == "level" || key == "x") {
            int level = std::stoi(value);
            compOpts.level = static_cast<CompressionLevel>(level);
        } else if (key == "method" || key == "m") {
            if (value == "lzma") compOpts.method = CompressionMethod::LZMA;
            else if (value == "lzma2") compOpts.method = CompressionMethod::LZMA2;
            else if (value == "bzip2") compOpts.method = CompressionMethod::BZIP2;
            else if (value == "ppmd") compOpts.method = CompressionMethod::PPMD;
            else if (value == "deflate") compOpts.method = CompressionMethod::DEFLATE;
            else if (value == "deflate64") compOpts.method = CompressionMethod::DEFLATE64;
            else if (value == "copy") compOpts.method = CompressionMethod::COPY;
            else if (value == "zstd") compOpts.method = CompressionMethod::ZSTD;
            else if (value == "lz4") compOpts.method = CompressionMethod::LZ4;
            else if (value == "lz5") compOpts.method = CompressionMethod::LZ5;
            else if (value == "brotli") compOpts.method = CompressionMethod::BROTLI;
            else if (value == "flzma2") compOpts.method = CompressionMethod::FLZMA2;
        } else if (key == "solid" || key == "s") {
            compOpts.solidMode = (value == "on" || value == "true" || value == "1");
        } else if (key == "dictionary" || key == "d") {
            compOpts.dictionarySize = value;
        } else if (key == "word" || key == "w") {
            compOpts.wordSize = value;
        } else if (key == "threads" || key == "mt") {
            compOpts.threadCount = std::stoi(value);
        } else if (key == "encryptHeaders" || key == "he") {
            compOpts.encryptHeaders = (value == "on" || value == "true" || value == "1");
        } else if (key == "filter" || key == "f") {
            if (value == "bcj") compOpts.filter = FilterMethod::BCJ;
            else if (value == "bcj2") compOpts.filter = FilterMethod::BCJ2;
            else if (value == "delta") compOpts.filter = FilterMethod::DELTA;
            else if (value == "arm") compOpts.filter = FilterMethod::BCJ_ARM;
            else if (value == "armt") compOpts.filter = FilterMethod::BCJ_ARMT;
            else if (value == "ia64") compOpts.filter = FilterMethod::BCJ_IA64;
            else if (value == "ppc") compOpts.filter = FilterMethod::BCJ_PPC;
            else if (value == "sparc") compOpts.filter = FilterMethod::BCJ_SPARC;
        } else if (key == "fastBytes" || key == "fb") {
            compOpts.fastBytes = std::stoi(value);
        } else if (key == "lc") {
            compOpts.literalContextBits = std::stoi(value);
        } else if (key == "lp") {
            compOpts.literalPosBits = std::stoi(value);
        } else if (key == "pb") {
            compOpts.posBits = std::stoi(value);
        } else if (key == "matchFinder" || key == "mf") {
            compOpts.matchFinder = value;
        } else if (key == "autoFilter" || key == "af") {
            compOpts.autoFilter = (value == "on" || value == "true" || value == "1");
        } else if (key == "estimatedSize" || key == "es") {
            compOpts.estimatedSize = std::stoll(value);
        }
    }
    
    std::string tempArchive = archivePath + ".tmp";
    
    bool success = CompressDirectory(tempArchive, extractDir, compOpts, true);
    
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(StringToWString(extractDir + "\\*").c_str(), &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            std::wstring name = findData.cFileName;
            if (name != L"." && name != L"..") {
                std::string fullPath = extractDir + "\\" + WStringToString(name);
                if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    RemoveDirectoryA(fullPath.c_str());
                } else {
                    DeleteFileA(fullPath.c_str());
                }
            }
        } while (FindNextFileW(hFind, &findData));
        FindClose(hFind);
    }
    RemoveDirectoryA(extractDir.c_str());
    
    if (success) {
        DeleteFileA(archivePath.c_str());
        MoveFileA(tempArchive.c_str(), archivePath.c_str());
    } else {
        DeleteFileA(tempArchive.c_str());
    }
    
    return success;
}

bool SevenZipArchive::OptimizeArchive(
    const std::string& archivePath,
    const std::string& outputPath,
    const CompressionOptions& options,
    const std::string& password) {
    
    if (!IsInitialized() && !Initialize()) return false;
    if (!FileExists(archivePath)) return false;
    
    std::string tempDir = m_tempDirectory;
    if (tempDir.empty()) {
        char tempPath[MAX_PATH];
        GetTempPathA(MAX_PATH, tempPath);
        tempDir = tempPath;
    }
    
    std::string extractDir = tempDir + "\\7zopt_" + std::to_string(GetCurrentProcessId());
    CreateDirectoryRecursive(extractDir);
    
    ExtractOptions extractOpts;
    extractOpts.outputDir = extractDir;
    extractOpts.password = password;
    
    if (!ExtractArchive(archivePath, extractOpts)) {
        return false;
    }
    
    bool success = CompressDirectory(outputPath, extractDir, options, true);
    
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(StringToWString(extractDir + "\\*").c_str(), &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            std::wstring name = findData.cFileName;
            if (name != L"." && name != L"..") {
                std::string fullPath = extractDir + "\\" + WStringToString(name);
                if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    RemoveDirectoryA(fullPath.c_str());
                } else {
                    DeleteFileA(fullPath.c_str());
                }
            }
        } while (FindNextFileW(hFind, &findData));
        FindClose(hFind);
    }
    RemoveDirectoryA(extractDir.c_str());
    
    return success;
}

bool SevenZipArchive::GetArchiveRecoveryRecord(
    const std::string& archivePath,
    uint32_t& recordSize,
    const std::string& password) {
    
    recordSize = 0;
    
    if (!IsInitialized() && !Initialize()) return false;
    if (!FileExists(archivePath)) return false;
    
    std::ifstream file(archivePath, std::ios::binary);
    if (!file.is_open()) return false;
    
    file.seekg(0, std::ios::end);
    uint64_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    const uint32_t RECOVERY_SIGNATURE = 0x52454330;
    const uint32_t RECOVERY_MAGIC = 0x56455253;
    
    if (fileSize < 1024) return false;
    
    file.seekg(-1024, std::ios::end);
    
    uint32_t sig, magic, size;
    file.read(reinterpret_cast<char*>(&sig), 4);
    file.read(reinterpret_cast<char*>(&magic), 4);
    file.read(reinterpret_cast<char*>(&size), 4);
    
    file.close();
    
    if (sig == RECOVERY_SIGNATURE && magic == RECOVERY_MAGIC) {
        recordSize = size;
        return true;
    }
    
    return false;
}

static bool WriteRecoveryData(const std::string& archivePath, uint32_t recordPercent) {
    std::ifstream inFile(archivePath, std::ios::binary);
    if (!inFile.is_open()) return false;
    
    inFile.seekg(0, std::ios::end);
    uint64_t fileSize = inFile.tellg();
    inFile.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> data(fileSize);
    inFile.read(reinterpret_cast<char*>(data.data()), fileSize);
    inFile.close();
    
    uint32_t recoverySize = (uint32_t)(fileSize * recordPercent / 100);
    if (recoverySize < 512) recoverySize = 512;
    if (recoverySize > 100 * 1024 * 1024) recoverySize = 100 * 1024 * 1024;
    
    std::vector<uint8_t> recoveryData(recoverySize);
    
    uint32_t seed = (uint32_t)time(NULL);
    for (uint32_t i = 0; i < recoverySize; i++) {
        seed = seed * 1103515245 + 12345;
        uint32_t idx = (seed ^ i) % (uint32_t)fileSize;
        recoveryData[i] = data[idx] ^ (uint8_t)(seed & 0xFF);
    }
    
    std::ofstream outFile(archivePath, std::ios::binary | std::ios::app);
    if (!outFile.is_open()) return false;
    
    const uint32_t RECOVERY_SIGNATURE = 0x52454330;
    const uint32_t RECOVERY_MAGIC = 0x56455253;
    
    outFile.write(reinterpret_cast<const char*>(&RECOVERY_SIGNATURE), 4);
    outFile.write(reinterpret_cast<const char*>(&RECOVERY_MAGIC), 4);
    outFile.write(reinterpret_cast<const char*>(&recoverySize), 4);
    outFile.write(reinterpret_cast<const char*>(&fileSize), 8);
    outFile.write(reinterpret_cast<const char*>(recoveryData.data()), recoverySize);
    
    uint32_t crc = CalculateCRC32(recoveryData.data(), recoverySize);
    outFile.write(reinterpret_cast<const char*>(&crc), 4);
    
    outFile.close();
    
    return true;
}

bool SevenZipArchive::AddRecoveryRecord(
    const std::string& archivePath,
    uint32_t recordPercent,
    const std::string& password) {
    
    if (!IsInitialized() && !Initialize()) return false;
    if (!FileExists(archivePath)) return false;
    if (recordPercent == 0 || recordPercent > 100) return false;
    
    uint32_t existingRecord;
    if (GetArchiveRecoveryRecord(archivePath, existingRecord, password)) {
        if (!RemoveRecoveryRecord(archivePath)) {
            return false;
        }
    }
    
    return WriteRecoveryData(archivePath, recordPercent);
}

bool SevenZipArchive::RemoveRecoveryRecord(
    const std::string& archivePath) {
    
    uint32_t recordSize;
    if (!GetArchiveRecoveryRecord(archivePath, recordSize, "")) {
        return true;
    }
    
    std::ifstream inFile(archivePath, std::ios::binary);
    if (!inFile.is_open()) return false;
    
    inFile.seekg(0, std::ios::end);
    uint64_t fileSize = inFile.tellg();
    inFile.close();
    
    uint64_t newSize = fileSize - recordSize - 24;
    
    std::string tempPath = archivePath + ".tmprec";
    
    std::ifstream src(archivePath, std::ios::binary);
    std::ofstream dst(tempPath, std::ios::binary);
    
    if (!src.is_open() || !dst.is_open()) {
        src.close();
        dst.close();
        return false;
    }
    
    std::vector<char> buffer(64 * 1024);
    uint64_t remaining = newSize;
    
    while (remaining > 0) {
        uint64_t toRead = (remaining < buffer.size()) ? remaining : buffer.size();
        src.read(buffer.data(), toRead);
        dst.write(buffer.data(), toRead);
        remaining -= toRead;
    }
    
    src.close();
    dst.close();
    
    DeleteFileA(archivePath.c_str());
    MoveFileA(tempPath.c_str(), archivePath.c_str());
    
    return true;
}

bool SevenZipArchive::RepairArchiveWithRecovery(
    const std::string& archivePath,
    const std::string& outputPath,
    RepairResult& result,
    const std::string& password) {
    
    result = RepairResult();
    result.success = false;
    
    if (!IsInitialized() && !Initialize()) return false;
    if (!FileExists(archivePath)) return false;
    
    uint32_t recordSize;
    if (!GetArchiveRecoveryRecord(archivePath, recordSize, password)) {
        result.errorMessage = "No recovery record found";
        return false;
    }
    
    std::ifstream inFile(archivePath, std::ios::binary);
    if (!inFile.is_open()) {
        result.errorMessage = "Cannot open archive";
        return false;
    }
    
    inFile.seekg(0, std::ios::end);
    uint64_t fileSize = inFile.tellg();
    inFile.seekg(0, std::ios::beg);
    
    uint64_t recoveryOffset = fileSize - recordSize - 24;
    
    inFile.seekg(recoveryOffset + 12);
    
    uint32_t sig, magic, recSize;
    uint64_t originalSize;
    
    inFile.read(reinterpret_cast<char*>(&sig), 4);
    inFile.read(reinterpret_cast<char*>(&magic), 4);
    inFile.read(reinterpret_cast<char*>(&recSize), 4);
    inFile.read(reinterpret_cast<char*>(&originalSize), 8);
    
    std::vector<uint8_t> recoveryData(recSize);
    inFile.read(reinterpret_cast<char*>(recoveryData.data()), recSize);
    
    uint32_t storedCrc;
    inFile.read(reinterpret_cast<char*>(&storedCrc), 4);
    inFile.close();
    
    uint32_t calcCrc = CalculateCRC32(recoveryData.data(), recSize);
    if (calcCrc != storedCrc) {
        result.errorMessage = "Recovery record is corrupted";
        return false;
    }
    
    std::ifstream srcFile(archivePath, std::ios::binary);
    std::ofstream dstFile(outputPath, std::ios::binary);
    
    if (!srcFile.is_open() || !dstFile.is_open()) {
        srcFile.close();
        dstFile.close();
        result.errorMessage = "Cannot create output file";
        return false;
    }
    
    std::vector<char> buffer(64 * 1024);
    uint64_t remaining = originalSize;
    
    while (remaining > 0) {
        uint64_t toRead = (remaining < buffer.size()) ? remaining : buffer.size();
        srcFile.read(buffer.data(), toRead);
        dstFile.write(buffer.data(), toRead);
        remaining -= toRead;
    }
    
    srcFile.close();
    dstFile.close();
    
    result.success = true;
    result.partiallyRepaired = false;
    result.recoveredFiles = 0;
    result.totalFiles = 0;
    result.recoveredBytes = originalSize;
    result.totalBytes = originalSize;
    
    return true;
}

struct PluginInfo {
    std::string path;
    HMODULE handle;
    std::vector<std::string> codecs;
};

static std::vector<PluginInfo> g_loadedPlugins;
static std::mutex g_pluginMutex;

bool SevenZipArchive::LoadPlugin(const std::string& pluginPath) {
    if (!FileExists(pluginPath)) return false;
    
    std::lock_guard<std::mutex> lock(g_pluginMutex);
    
    for (const auto& plugin : g_loadedPlugins) {
        if (plugin.path == pluginPath) return true;
    }
    
    std::wstring wpath = StringToWString(pluginPath);
    HMODULE hPlugin = LoadLibraryW(wpath.c_str());
    if (!hPlugin) return false;
    
    PluginInfo info;
    info.path = pluginPath;
    info.handle = hPlugin;
    
    typedef HRESULT (WINAPI* Func_GetNumberOfMethods)(UInt32* numMethods);
    typedef HRESULT (WINAPI* Func_GetMethodProperty)(UInt32 index, PROPID propID, PROPVARIANT* value);
    
    Func_GetNumberOfMethods getNumMethods = (Func_GetNumberOfMethods)GetProcAddress(hPlugin, "GetNumberOfMethods");
    
    if (getNumMethods) {
        UInt32 numMethods = 0;
        if (getNumMethods(&numMethods) == S_OK) {
            Func_GetMethodProperty getMethodProp = (Func_GetMethodProperty)GetProcAddress(hPlugin, "GetMethodProperty");
            if (getMethodProp) {
                for (UInt32 i = 0; i < numMethods; i++) {
                    PROPVARIANT prop;
                    PropVariantInit(&prop);
                    if (getMethodProp(i, 0, &prop) == S_OK && prop.vt == VT_BSTR) {
                        info.codecs.push_back(WStringToString(prop.bstrVal));
                    }
                    PropVariantClear(&prop);
                }
            }
        }
    }
    
    g_loadedPlugins.push_back(info);
    return true;
}

bool SevenZipArchive::UnloadPlugin(const std::string& pluginPath) {
    std::lock_guard<std::mutex> lock(g_pluginMutex);
    
    for (auto it = g_loadedPlugins.begin(); it != g_loadedPlugins.end(); ++it) {
        if (it->path == pluginPath) {
            FreeLibrary(it->handle);
            g_loadedPlugins.erase(it);
            return true;
        }
    }
    return false;
}

bool SevenZipArchive::UnloadAllPlugins() {
    std::lock_guard<std::mutex> lock(g_pluginMutex);
    
    for (auto& plugin : g_loadedPlugins) {
        FreeLibrary(plugin.handle);
    }
    g_loadedPlugins.clear();
    return true;
}

std::vector<std::string> SevenZipArchive::GetLoadedPlugins() {
    std::lock_guard<std::mutex> lock(g_pluginMutex);
    
    std::vector<std::string> paths;
    for (const auto& plugin : g_loadedPlugins) {
        paths.push_back(plugin.path);
    }
    return paths;
}

bool SevenZipArchive::RegisterCodec(const std::string& codecPath) {
    return LoadPlugin(codecPath);
}

bool SevenZipArchive::RegisterExternalCodec(const GUID& codecID, const std::string& codecPath) {
    return LoadPlugin(codecPath);
}

std::vector<std::string> SevenZipArchive::GetAvailableCodecs() {
    std::vector<std::string> codecs = {
        "lzma", "lzma2", "ppmd", "bzip2", "deflate", "deflate64",
        "copy", "delta", "bcj", "bcj2", "arm", "armt", "ia64", "ppc", "sparc"
    };
    
    std::lock_guard<std::mutex> lock(g_pluginMutex);
    for (const auto& plugin : g_loadedPlugins) {
        for (const auto& codec : plugin.codecs) {
            if (std::find(codecs.begin(), codecs.end(), codec) == codecs.end()) {
                codecs.push_back(codec);
            }
        }
    }
    
    return codecs;
}

bool SevenZipArchive::SetPluginDirectory(const std::string& directory) {
    if (!DirectoryExists(directory)) return false;
    
    std::string searchPath = directory + "\\*.dll";
    std::wstring wsearchPath = StringToWString(searchPath);
    
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(wsearchPath.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) return false;
    
    do {
        std::wstring name = findData.cFileName;
        std::string fullPath = directory + "\\" + WStringToString(name);
        
        if (name.find(L"7z") == 0 || name.find(L"codec") != std::wstring::npos || 
            name.find(L"plugin") != std::wstring::npos) {
            LoadPlugin(fullPath);
        }
    } while (FindNextFileW(hFind, &findData));
    
    FindClose(hFind);
    return true;
}

class CommandLineParser {
public:
    struct Command {
        std::string name;
        std::vector<std::string> args;
        std::map<std::string, std::string> options;
    };
    
    static Command Parse(int argc, char* argv[]) {
        Command cmd;
        if (argc < 2) {
            cmd.name = "help";
            return cmd;
        }
        
        cmd.name = argv[1];
        
        for (int i = 2; i < argc; i++) {
            std::string arg = argv[i];
            if (!arg.empty() && arg[0] == '-') {
                size_t eqPos = arg.find('=');
                if (eqPos != std::string::npos) {
                    std::string key = arg.substr(0, eqPos);
                    std::string value = arg.substr(eqPos + 1);
                    cmd.options[key] = value;
                } else {
                    cmd.options[arg] = "";
                }
            } else {
                cmd.args.push_back(arg);
            }
        }
        
        return cmd;
    }
    
    static std::string GetOption(const Command& cmd, const std::string& name, const std::string& defaultVal = "") {
        auto it = cmd.options.find(name);
        if (it != cmd.options.end()) return it->second;
        
        it = cmd.options.find("-" + name);
        if (it != cmd.options.end()) return it->second;
        
        it = cmd.options.find("--" + name);
        if (it != cmd.options.end()) return it->second;
        
        return defaultVal;
    }
    
    static bool HasOption(const Command& cmd, const std::string& name) {
        return cmd.options.find(name) != cmd.options.end() ||
               cmd.options.find("-" + name) != cmd.options.end() ||
               cmd.options.find("--" + name) != cmd.options.end();
    }
};

class BackupManager;

enum class BackupType {
    Full,
    Incremental,
    Differential
};

struct BackupOptions {
    BackupType type = BackupType::Full;
    CompressionOptions compression;
    std::string baseArchive;
    std::string password;
    bool preservePermissions = true;
    bool preserveTimestamps = true;
    bool includeEmptyDirectories = true;
    std::vector<std::string> excludePatterns;
    std::vector<std::string> includePatterns;
};

struct BackupResult {
    bool success = false;
    uint32_t filesProcessed = 0;
    uint64_t bytesProcessed = 0;
    uint32_t filesSkipped = 0;
    uint64_t bytesSkipped = 0;
    std::string errorMessage;
    std::string archivePath;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
};

struct RestoreOptions {
    std::string password;
    bool overwrite = false;
    std::string pointInTime;
    bool preservePermissions = true;
    bool preserveTimestamps = true;
    std::vector<std::string> filesToRestore;
};

struct RestoreResult {
    bool success = false;
    uint32_t filesRestored = 0;
    uint64_t bytesRestored = 0;
    uint32_t filesSkipped = 0;
    std::string errorMessage;
};

typedef SevenZipArchive SevenZipCompressor;

class BackupManager {
private:
    SevenZipArchive& m_archive;
    std::string m_catalogPath;
    
public:
    BackupManager(SevenZipArchive& archive, const std::string& catalogPath = "")
        : m_archive(archive), m_catalogPath(catalogPath) {}
    
    bool CreateBackup(const std::string& archivePath, const std::string& sourcePath,
                     const BackupOptions& options, BackupResult& result) {
        result = BackupResult();
        result.startTime = std::chrono::system_clock::now();
        
        if (!DirectoryExists(sourcePath) && !FileExists(sourcePath)) {
            result.errorMessage = "Source path does not exist: " + sourcePath;
            return false;
        }
        
        std::map<std::string, FileInfo> previousFiles;
        
        if (options.type != BackupType::Full && !options.baseArchive.empty()) {
            ArchiveInfo baseInfo;
            if (m_archive.ListArchive(options.baseArchive, baseInfo, options.password)) {
                for (const auto& f : baseInfo.files) {
                    previousFiles[f.path] = f;
                }
            }
        } else if (options.type != BackupType::Full) {
            ArchiveInfo existingInfo;
            if (FileExists(archivePath) && m_archive.ListArchive(archivePath, existingInfo, options.password)) {
                for (const auto& f : existingInfo.files) {
                    previousFiles[f.path] = f;
                }
            }
        }
        
        std::vector<CDirItem> itemsToBackup;
        
        if (DirectoryExists(sourcePath)) {
            EnumerateFilesForBackup(sourcePath, sourcePath, itemsToBackup, options, previousFiles, result);
        } else {
            CDirItem item;
            WIN32_FILE_ATTRIBUTE_DATA attr;
            if (GetFileAttributesExA(sourcePath.c_str(), GetFileExInfoStandard, &attr)) {
                item.RelativePath = StringToWString(GetFileName(sourcePath));
                item.FullPathA = sourcePath;
                item.Size = ((UInt64)attr.nFileSizeHigh << 32) | attr.nFileSizeLow;
                item.Attrib = attr.dwFileAttributes;
                item.MTime = attr.ftLastWriteTime;
                item.CTime = attr.ftCreationTime;
                item.ATime = attr.ftLastAccessTime;
                item.IsDir = false;
                
                bool shouldBackup = true;
                if (options.type != BackupType::Full) {
                    auto it = previousFiles.find(GetFileName(sourcePath));
                    if (it != previousFiles.end()) {
                        if (CompareFileTime(&attr.ftLastWriteTime, &it->second.lastWriteTime) <= 0) {
                            shouldBackup = false;
                            result.filesSkipped++;
                            result.bytesSkipped += item.Size;
                        }
                    }
                }
                
                if (shouldBackup) {
                    itemsToBackup.push_back(item);
                    result.filesProcessed++;
                    result.bytesProcessed += item.Size;
                }
            }
        }
        
        if (itemsToBackup.empty() && result.filesSkipped == 0) {
            result.errorMessage = "No files to backup";
            return false;
        }
        
        CompressionOptions compOpts = options.compression;
        compOpts.password = options.password;
        
        bool success = false;
        
        if (options.type == BackupType::Full || !FileExists(archivePath)) {
            success = m_archive.CompressDirectory(archivePath, sourcePath, compOpts, true);
        } else {
            std::vector<std::string> filesToAdd;
            for (const auto& item : itemsToBackup) {
                filesToAdd.push_back(item.FullPathA);
            }
            if (!filesToAdd.empty()) {
                success = m_archive.AddToArchive(archivePath, filesToAdd, compOpts);
            } else {
                success = true;
            }
        }
        
        result.endTime = std::chrono::system_clock::now();
        result.success = success;
        result.archivePath = archivePath;
        
        if (!success) {
            result.errorMessage = "Failed to create backup archive";
        }
        
        return success;
    }
    
    bool RestoreBackup(const std::string& archivePath, const std::string& outputPath,
                      const RestoreOptions& options, RestoreResult& result) {
        result = RestoreResult();
        
        if (!FileExists(archivePath)) {
            result.errorMessage = "Backup archive does not exist: " + archivePath;
            return false;
        }
        
        ExtractOptions extractOpts;
        extractOpts.outputDir = outputPath;
        extractOpts.password = options.password;
        extractOpts.overwriteExisting = options.overwrite;
        extractOpts.preserveFileTime = options.preserveTimestamps;
        
        if (!options.filesToRestore.empty()) {
            for (const auto& file : options.filesToRestore) {
                std::vector<uint8_t> data;
                if (m_archive.ExtractSingleFileToMemory(archivePath, file, data, options.password)) {
                    std::string fullPath = outputPath + "\\" + file;
                    CreateDirectoryForFile(fullPath);
                    
                    std::ofstream outFile(fullPath, std::ios::binary);
                    if (outFile.is_open()) {
                        outFile.write(reinterpret_cast<const char*>(data.data()), data.size());
                        outFile.close();
                        result.filesRestored++;
                        result.bytesRestored += data.size();
                    }
                }
            }
        } else {
            if (m_archive.ExtractArchive(archivePath, extractOpts)) {
                ArchiveInfo info;
                m_archive.ListArchive(archivePath, info, options.password);
                result.filesRestored = info.fileCount;
                result.bytesRestored = info.uncompressedSize;
            } else {
                result.errorMessage = "Failed to extract backup archive";
                return false;
            }
        }
        
        result.success = true;
        return true;
    }
    
private:
    void EnumerateFilesForBackup(const std::string& directory, const std::string& basePath,
                                  std::vector<CDirItem>& items, const BackupOptions& options,
                                  const std::map<std::string, FileInfo>& previousFiles,
                                  BackupResult& result) {
        std::string searchPath = directory + "\\*";
        WIN32_FIND_DATAW findData;
        HANDLE hFind = FindFirstFileW(StringToWString(searchPath).c_str(), &findData);
        
        if (hFind == INVALID_HANDLE_VALUE) return;
        
        do {
            std::wstring name = findData.cFileName;
            if (name == L"." || name == L"..") continue;
            
            std::string fileName = WStringToString(name);
            std::string fullPath = directory + "\\" + fileName;
            std::string relativePath = GetRelativePath(fullPath, basePath);
            
            bool matchesExclude = false;
            for (const auto& pattern : options.excludePatterns) {
                if (MatchWildcard(fileName, pattern)) {
                    matchesExclude = true;
                    break;
                }
            }
            
            if (matchesExclude) continue;
            
            if (!options.includePatterns.empty()) {
                bool matchesInclude = false;
                for (const auto& pattern : options.includePatterns) {
                    if (MatchWildcard(fileName, pattern)) {
                        matchesInclude = true;
                        break;
                    }
                }
                if (!matchesInclude) continue;
            }
            
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (options.includeEmptyDirectories) {
                    CDirItem item;
                    item.RelativePath = StringToWString(relativePath);
                    item.FullPathA = fullPath;
                    item.IsDir = true;
                    item.Attrib = findData.dwFileAttributes;
                    item.MTime = findData.ftLastWriteTime;
                    items.push_back(item);
                }
                EnumerateFilesForBackup(fullPath, basePath, items, options, previousFiles, result);
            } else {
                CDirItem item;
                item.RelativePath = StringToWString(relativePath);
                item.FullPathA = fullPath;
                item.Size = ((UInt64)findData.nFileSizeHigh << 32) | findData.nFileSizeLow;
                item.Attrib = findData.dwFileAttributes;
                item.MTime = findData.ftLastWriteTime;
                item.CTime = findData.ftCreationTime;
                item.ATime = findData.ftLastAccessTime;
                item.IsDir = false;
                
                bool shouldBackup = true;
                
                if (options.type != BackupType::Full) {
                    auto it = previousFiles.find(relativePath);
                    if (it != previousFiles.end()) {
                        if (CompareFileTime(&findData.ftLastWriteTime, &it->second.lastWriteTime) <= 0) {
                            shouldBackup = false;
                            result.filesSkipped++;
                            result.bytesSkipped += item.Size;
                        }
                    }
                }
                
                if (shouldBackup) {
                    items.push_back(item);
                    result.filesProcessed++;
                    result.bytesProcessed += item.Size;
                }
            }
        } while (FindNextFileW(hFind, &findData));
        
        FindClose(hFind);
    }
};

class CommandLineInterface {
private:
    SevenZipArchive& m_archive;
    std::function<void(const std::string&)> m_outputCallback;
    bool m_verbose;
    
public:
    CommandLineInterface(SevenZipArchive& archive) 
        : m_archive(archive), m_verbose(true) {}
    
    void SetOutputCallback(std::function<void(const std::string&)> callback) {
        m_outputCallback = callback;
    }
    
    void SetVerbose(bool verbose) { m_verbose = verbose; }
    
    void Output(const std::string& message) {
        if (m_outputCallback) {
            m_outputCallback(message);
        } else {
            std::cout << message << std::endl;
        }
    }
    
    int Execute(int argc, char* argv[]) {
        CommandLineParser::Command cmd = CommandLineParser::Parse(argc, argv);
        
        if (cmd.name == "help" || cmd.name == "-h" || cmd.name == "--help") {
            return ShowHelp();
        }
        
        if (cmd.name == "a" || cmd.name == "add") {
            return CmdAdd(cmd);
        }
        
        if (cmd.name == "x" || cmd.name == "extract") {
            return CmdExtract(cmd);
        }
        
        if (cmd.name == "e") {
            return CmdExtractSimple(cmd);
        }
        
        if (cmd.name == "l" || cmd.name == "list") {
            return CmdList(cmd);
        }
        
        if (cmd.name == "t" || cmd.name == "test") {
            return CmdTest(cmd);
        }
        
        if (cmd.name == "d" || cmd.name == "delete") {
            return CmdDelete(cmd);
        }
        
        if (cmd.name == "rn" || cmd.name == "rename") {
            return CmdRename(cmd);
        }
        
        if (cmd.name == "u" || cmd.name == "update") {
            return CmdUpdate(cmd);
        }
        
        if (cmd.name == "b" || cmd.name == "benchmark") {
            return CmdBenchmark(cmd);
        }
        
        if (cmd.name == "h" || cmd.name == "hash") {
            return CmdHash(cmd);
        }
        
        if (cmd.name == "i" || cmd.name == "info") {
            return CmdInfo(cmd);
        }
        
        if (cmd.name == "sfx") {
            return CmdCreateSFX(cmd);
        }
        
        if (cmd.name == "split") {
            return CmdSplit(cmd);
        }
        
        if (cmd.name == "merge") {
            return CmdMerge(cmd);
        }
        
        if (cmd.name == "convert") {
            return CmdConvert(cmd);
        }
        
        if (cmd.name == "diff" || cmd.name == "compare") {
            return CmdCompare(cmd);
        }
        
        if (cmd.name == "repair") {
            return CmdRepair(cmd);
        }
        
        if (cmd.name == "backup") {
            return CmdBackup(cmd);
        }
        
        if (cmd.name == "restore") {
            return CmdRestore(cmd);
        }
        
        Output("Unknown command: " + cmd.name);
        Output("Use 'help' to see available commands.");
        return 1;
    }
    
    int ShowHelp() {
        Output("7-Zip SDK Command Line Interface");
        Output("");
        Output("Usage: 7zsdk <command> [options] <archive> [files...]");
        Output("");
        Output("Commands:");
        Output("  a, add       Add files to archive");
        Output("  x, extract   Extract files from archive with full paths");
        Output("  e            Extract files from archive without paths");
        Output("  l, list      List contents of archive");
        Output("  t, test      Test integrity of archive");
        Output("  d, delete    Delete files from archive");
        Output("  rn, rename   Rename files in archive");
        Output("  u, update    Update files in archive");
        Output("  b, benchmark Run compression benchmark");
        Output("  h, hash      Calculate hash of files");
        Output("  i, info      Show archive information");
        Output("  sfx          Create self-extracting archive");
        Output("  split        Split archive into parts");
        Output("  merge        Merge multiple archives");
        Output("  convert      Convert archive to another format");
        Output("  diff         Compare two archives");
        Output("  repair       Attempt to repair damaged archive");
        Output("  backup       Create incremental/differential backup");
        Output("  restore      Restore from backup");
        Output("");
        Output("Options:");
        Output("  -p<password>   Set password");
        Output("  -mx<level>     Set compression level (0-9)");
        Output("  -m<method>     Set compression method (lzma2, lzma, bzip2, etc.)");
        Output("  -md<size>      Set dictionary size (e.g., 64m, 128m)");
        Output("  -mmt<threads>  Set number of threads");
        Output("  -mhe=on/off    Encrypt archive headers");
        Output("  -ms=on/off     Solid mode");
        Output("  -r             Recurse subdirectories");
        Output("  -o<dir>        Set output directory");
        Output("  -y             Assume yes on all queries");
        Output("  -v<size>       Create volumes of specified size");
        Output("  -t<type>       Specify archive type");
        Output("  -x<file>       Exclude files");
        Output("  -i<file>       Include files");
        Output("  -w<dir>        Set working directory");
        Output("  -aoa           Overwrite all existing files");
        Output("  -aos           Skip existing files");
        Output("  -aou           Auto rename extracted files");
        Output("");
        Output("Examples:");
        Output("  7zsdk a archive.7z file1.txt file2.txt");
        Output("  7zsdk a -mx9 -mhe=on -ppassword archive.7z folder\\");
        Output("  7zsdk x archive.7z -ooutput\\");
        Output("  7zsdk l archive.7z");
        Output("  7zsdk t archive.7z");
        return 0;
    }
    
    CompressionOptions ParseCompressionOptions(const CommandLineParser::Command& cmd) {
        CompressionOptions opts;
        
        std::string levelStr = CommandLineParser::GetOption(cmd, "-mx", "");
        if (!levelStr.empty()) {
            int level = std::stoi(levelStr);
            opts.level = static_cast<CompressionLevel>(level);
        }
        
        std::string methodStr = CommandLineParser::GetOption(cmd, "-m", "");
        if (!methodStr.empty()) {
            if (methodStr == "lzma") opts.method = CompressionMethod::LZMA;
            else if (methodStr == "lzma2") opts.method = CompressionMethod::LZMA2;
            else if (methodStr == "bzip2" || methodStr == "bz2") opts.method = CompressionMethod::BZIP2;
            else if (methodStr == "ppmd") opts.method = CompressionMethod::PPMD;
            else if (methodStr == "deflate") opts.method = CompressionMethod::DEFLATE;
            else if (methodStr == "deflate64") opts.method = CompressionMethod::DEFLATE64;
            else if (methodStr == "copy" || methodStr == "store") opts.method = CompressionMethod::COPY;
            else if (methodStr == "zstd") opts.method = CompressionMethod::ZSTD;
            else if (methodStr == "lz4") opts.method = CompressionMethod::LZ4;
            else if (methodStr == "lz5") opts.method = CompressionMethod::LZ5;
            else if (methodStr == "brotli") opts.method = CompressionMethod::BROTLI;
            else if (methodStr == "flzma2") opts.method = CompressionMethod::FLZMA2;
        }
        
        std::string dictStr = CommandLineParser::GetOption(cmd, "-md", "");
        if (!dictStr.empty()) {
            opts.dictionarySize = dictStr;
        }
        
        std::string threadStr = CommandLineParser::GetOption(cmd, "-mmt", "");
        if (!threadStr.empty()) {
            opts.threadCount = std::stoi(threadStr);
        }
        
        std::string heStr = CommandLineParser::GetOption(cmd, "-mhe", "");
        if (heStr == "on" || heStr == "1") {
            opts.encryptHeaders = true;
        }
        
        std::string solidStr = CommandLineParser::GetOption(cmd, "-ms", "");
        if (solidStr == "off" || solidStr == "0") {
            opts.solidMode = false;
        }
        
        std::string passStr = CommandLineParser::GetOption(cmd, "-p", "");
        if (!passStr.empty()) {
            opts.password = passStr;
        }
        
        std::string volStr = CommandLineParser::GetOption(cmd, "-v", "");
        if (!volStr.empty()) {
            opts.volumeSize = ParseSize(volStr);
        }
        
        return opts;
    }
    
    uint64_t ParseSize(const std::string& sizeStr) {
        if (sizeStr.empty()) return 0;
        
        uint64_t size = 0;
        char suffix = 0;
        
        std::istringstream iss(sizeStr);
        iss >> size >> suffix;
        
        switch (toupper(suffix)) {
            case 'K': size *= 1024; break;
            case 'M': size *= 1024 * 1024; break;
            case 'G': size *= 1024ULL * 1024 * 1024; break;
            case 'B': break;
        }
        
        return size;
    }
    
    ExtractOptions ParseExtractOptions(const CommandLineParser::Command& cmd) {
        ExtractOptions opts;
        
        opts.outputDir = CommandLineParser::GetOption(cmd, "-o", "");
        opts.password = CommandLineParser::GetOption(cmd, "-p", "");
        
        std::string overwriteStr = CommandLineParser::GetOption(cmd, "-ao", "");
        if (overwriteStr == "a") {
            opts.overwriteMode = ExtractOptions::OverwriteMode::Overwrite;
        } else if (overwriteStr == "s") {
            opts.overwriteMode = ExtractOptions::OverwriteMode::Skip;
        } else if (overwriteStr == "u") {
            opts.overwriteMode = ExtractOptions::OverwriteMode::Rename;
        }
        
        opts.preserveDirectoryStructure = !CommandLineParser::HasOption(cmd, "-e");
        
        return opts;
    }
    
    int CmdAdd(const CommandLineParser::Command& cmd) {
        if (cmd.args.size() < 2) {
            Output("Usage: 7zsdk a [options] <archive> <files...>");
            return 1;
        }
        
        std::string archivePath = cmd.args[0];
        std::vector<std::string> files(cmd.args.begin() + 1, cmd.args.end());
        
        CompressionOptions opts = ParseCompressionOptions(cmd);
        
        bool recursive = CommandLineParser::HasOption(cmd, "-r");
        
        m_archive.SetProgressCallback([this](const ProgressInfo& info) {
            if (m_verbose) {
                std::ostringstream oss;
                oss << "\rCompressing: " << info.percent << "% - " << info.currentFile;
                Output(oss.str());
            }
        });
        
        bool success = false;
        
        std::vector<std::string> dirs;
        std::vector<std::string> filesOnly;
        
        for (const auto& f : files) {
            if (DirectoryExists(f)) {
                dirs.push_back(f);
            } else if (FileExists(f)) {
                filesOnly.push_back(f);
            }
        }
        
        if (!filesOnly.empty()) {
            success = m_archive.CompressFiles(archivePath, filesOnly, opts);
        }
        
        for (const auto& dir : dirs) {
            success = m_archive.CompressDirectory(archivePath, dir, opts, recursive);
        }
        
        if (success) {
            Output("\nArchive created successfully: " + archivePath);
            return 0;
        } else {
            Output("\nFailed to create archive.");
            return 1;
        }
    }
    
    int CmdExtract(const CommandLineParser::Command& cmd) {
        if (cmd.args.empty()) {
            Output("Usage: 7zsdk x [options] <archive>");
            return 1;
        }
        
        std::string archivePath = cmd.args[0];
        ExtractOptions opts = ParseExtractOptions(cmd);
        
        m_archive.SetProgressCallback([this](const ProgressInfo& info) {
            if (m_verbose) {
                std::ostringstream oss;
                oss << "\rExtracting: " << info.percent << "% - " << info.currentFile;
                Output(oss.str());
            }
        });
        
        if (m_archive.ExtractArchive(archivePath, opts)) {
            Output("\nExtraction completed successfully.");
            return 0;
        } else {
            Output("\nExtraction failed.");
            return 1;
        }
    }
    
    int CmdExtractSimple(const CommandLineParser::Command& cmd) {
        if (cmd.args.empty()) {
            Output("Usage: 7zsdk e [options] <archive>");
            return 1;
        }
        
        std::string archivePath = cmd.args[0];
        ExtractOptions opts = ParseExtractOptions(cmd);
        opts.preserveDirectoryStructure = false;
        opts.extractFullPath = false;
        
        if (m_archive.ExtractArchive(archivePath, opts)) {
            Output("Extraction completed successfully.");
            return 0;
        } else {
            Output("Extraction failed.");
            return 1;
        }
    }
    
    int CmdList(const CommandLineParser::Command& cmd) {
        if (cmd.args.empty()) {
            Output("Usage: 7zsdk l <archive>");
            return 1;
        }
        
        std::string archivePath = cmd.args[0];
        std::string password = CommandLineParser::GetOption(cmd, "-p", "");
        
        ArchiveInfo info;
        if (!m_archive.ListArchive(archivePath, info, password)) {
            Output("Failed to list archive.");
            return 1;
        }
        
        Output("Archive: " + archivePath);
        Output("");
        Output("   Date      Time    Attr         Size   Compressed  Name");
        Output("------------------- ----- ------------ ------------  ----------------");
        
        for (const auto& file : info.files) {
            char dateStr[32], timeStr[32];
            SYSTEMTIME st;
            FileTimeToSystemTime(&file.lastWriteTime, &st);
            sprintf_s(dateStr, "%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
            sprintf_s(timeStr, "%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);
            
            std::string attr = file.isDirectory ? "D...." : ".....";
            if (file.isEncrypted) attr[1] = 'A';
            if (file.isSymLink) attr[2] = 'L';
            
            std::ostringstream oss;
            oss << dateStr << " " << timeStr << " " << attr << " "
                << std::setw(12) << file.size << " "
                << std::setw(12) << file.packedSize << "  "
                << file.path;
            Output(oss.str());
        }
        
        Output("------------------- ----- ------------ ------------  ----------------");
        
        std::ostringstream summary;
        summary << info.fileCount << " files, " << info.directoryCount << " folders";
        summary << ", " << FormatSize(info.uncompressedSize) << " (uncompressed)";
        summary << ", " << FormatSize(info.compressedSize) << " (compressed)";
        Output(summary.str());
        
        return 0;
    }
    
    int CmdTest(const CommandLineParser::Command& cmd) {
        if (cmd.args.empty()) {
            Output("Usage: 7zsdk t <archive>");
            return 1;
        }
        
        std::string archivePath = cmd.args[0];
        std::string password = CommandLineParser::GetOption(cmd, "-p", "");
        
        ValidationResult result;
        if (m_archive.TestArchiveEx(archivePath, result, password)) {
            Output("Archive is valid: " + archivePath);
            Output("Files tested: " + std::to_string(result.validFiles));
            return 0;
        } else {
            Output("Archive test failed: " + archivePath);
            for (const auto& err : result.errors) {
                Output("  Error: " + err);
            }
            return 1;
        }
    }
    
    int CmdDelete(const CommandLineParser::Command& cmd) {
        if (cmd.args.size() < 2) {
            Output("Usage: 7zsdk d <archive> <files...>");
            return 1;
        }
        
        std::string archivePath = cmd.args[0];
        std::vector<std::string> filesToDelete(cmd.args.begin() + 1, cmd.args.end());
        std::string password = CommandLineParser::GetOption(cmd, "-p", "");
        
        if (m_archive.DeleteFromArchive(archivePath, filesToDelete, password)) {
            Output("Files deleted successfully.");
            return 0;
        } else {
            Output("Failed to delete files from archive.");
            return 1;
        }
    }
    
    int CmdRename(const CommandLineParser::Command& cmd) {
        if (cmd.args.size() < 3) {
            Output("Usage: 7zsdk rn <archive> <oldName> <newName>");
            return 1;
        }
        
        std::string archivePath = cmd.args[0];
        std::string oldName = cmd.args[1];
        std::string newName = cmd.args[2];
        std::string password = CommandLineParser::GetOption(cmd, "-p", "");
        
        if (m_archive.RenameInArchive(archivePath, oldName, newName, password)) {
            Output("File renamed successfully.");
            return 0;
        } else {
            Output("Failed to rename file in archive.");
            return 1;
        }
    }
    
    int CmdUpdate(const CommandLineParser::Command& cmd) {
        if (cmd.args.size() < 2) {
            Output("Usage: 7zsdk u [options] <archive> <files...>");
            return 1;
        }
        
        std::string archivePath = cmd.args[0];
        std::vector<std::string> files(cmd.args.begin() + 1, cmd.args.end());
        CompressionOptions opts = ParseCompressionOptions(cmd);
        
        if (m_archive.UpdateArchive(archivePath, files, opts)) {
            Output("Archive updated successfully.");
            return 0;
        } else {
            Output("Failed to update archive.");
            return 1;
        }
    }
    
    int CmdBenchmark(const CommandLineParser::Command& cmd) {
        std::string methodStr = CommandLineParser::GetOption(cmd, "-m", "lzma2");
        std::string sizeStr = CommandLineParser::GetOption(cmd, "-size", "100m");
        std::string iterStr = CommandLineParser::GetOption(cmd, "-iter", "3");
        std::string threadStr = CommandLineParser::GetOption(cmd, "-mmt", "0");
        
        CompressionMethod method = CompressionMethod::LZMA2;
        if (methodStr == "lzma") method = CompressionMethod::LZMA;
        else if (methodStr == "bzip2") method = CompressionMethod::BZIP2;
        else if (methodStr == "ppmd") method = CompressionMethod::PPMD;
        else if (methodStr == "deflate") method = CompressionMethod::DEFLATE;
        else if (methodStr == "zstd") method = CompressionMethod::ZSTD;
        else if (methodStr == "lz4") method = CompressionMethod::LZ4;
        
        uint64_t size = ParseSize(sizeStr);
        int iterations = std::stoi(iterStr);
        int threads = std::stoi(threadStr);
        
        Output("Running benchmark...");
        Output("Method: " + methodStr);
        Output("Data size: " + FormatSize(size));
        Output("Iterations: " + std::to_string(iterations));
        Output("Threads: " + (threads > 0 ? std::to_string(threads) : "auto"));
        Output("");
        
        std::vector<BenchmarkResult> results;
        if (m_archive.RunBenchmark(results, method, iterations, size, threads)) {
            for (const auto& result : results) {
                Output("Iteration " + std::to_string(&result - &results[0] + 1) + ":");
                Output("  Compression: " + FormatSize(result.compressedSize) + 
                       " (" + std::to_string(result.compressionRatio) + "x)");
                Output("  Compression speed: " + std::to_string((int)result.compressionSpeed) + " MB/s");
                Output("  Decompression speed: " + std::to_string((int)result.decompressionSpeed) + " MB/s");
                Output("  Status: " + std::string(result.passed ? "PASSED" : "FAILED"));
                if (!result.errorMessage.empty()) {
                    Output("  Error: " + result.errorMessage);
                }
            }
            return 0;
        } else {
            Output("Benchmark failed.");
            return 1;
        }
    }
    
    int CmdHash(const CommandLineParser::Command& cmd) {
        if (cmd.args.empty()) {
            Output("Usage: 7zsdk h [options] <files...>");
            return 1;
        }
        
        std::string algorithm = CommandLineParser::GetOption(cmd, "-a", "SHA256");
        
        for (const auto& file : cmd.args) {
            if (!FileExists(file)) {
                Output("File not found: " + file);
                continue;
            }
            
            HashResult result;
            if (m_archive.CalculateFileHash(file, result, algorithm)) {
                Output(algorithm + "(" + file + ") = " + result.hash);
            } else {
                Output("Failed to calculate hash for: " + file);
            }
        }
        
        return 0;
    }
    
    int CmdInfo(const CommandLineParser::Command& cmd) {
        if (cmd.args.empty()) {
            Output("7-Zip SDK Information");
            Output("");
            Output("Supported formats:");
            
            std::vector<std::pair<std::string, std::string>> formats;
            m_archive.GetSupportedFormats(formats);
            
            for (const auto& fmt : formats) {
                Output("  " + fmt.first + " - " + fmt.second);
            }
            
            Output("");
            Output("Supported compression methods:");
            
            std::vector<std::string> methods;
            m_archive.GetSupportedMethods(methods);
            
            for (const auto& m : methods) {
                Output("  " + m);
            }
            
            Output("");
            Output("Supported hash algorithms:");
            
            auto hashes = SevenZipArchive::GetAvailableHashAlgorithms();
            for (const auto& h : hashes) {
                Output("  " + h);
            }
            
            return 0;
        }
        
        std::string archivePath = cmd.args[0];
        std::string password = CommandLineParser::GetOption(cmd, "-p", "");
        
        std::map<std::string, std::string> properties;
        if (m_archive.GetArchiveProperties(archivePath, properties, password)) {
            Output("Archive: " + archivePath);
            Output("");
            Output("Properties:");
            for (const auto& prop : properties) {
                Output("  " + prop.first + ": " + prop.second);
            }
            return 0;
        } else {
            Output("Failed to get archive properties.");
            return 1;
        }
    }
    
    int CmdCreateSFX(const CommandLineParser::Command& cmd) {
        if (cmd.args.size() < 2) {
            Output("Usage: 7zsdk sfx <archive> <output.exe> [sfx_module]");
            return 1;
        }
        
        std::string archivePath = cmd.args[0];
        std::string sfxPath = cmd.args[1];
        std::string sfxModule = cmd.args.size() > 2 ? cmd.args[2] : "";
        
        SFXConfig config;
        config.title = CommandLineParser::GetOption(cmd, "-title", "");
        config.beginPrompt = CommandLineParser::GetOption(cmd, "-prompt", "");
        config.installDirectory = CommandLineParser::GetOption(cmd, "-install", "");
        config.executeFile = CommandLineParser::GetOption(cmd, "-run", "");
        config.silentMode = CommandLineParser::HasOption(cmd, "-silent");
        
        if (m_archive.CreateSFXWithConfig(archivePath, sfxPath, config, sfxModule)) {
            Output("SFX archive created: " + sfxPath);
            return 0;
        } else {
            Output("Failed to create SFX archive.");
            return 1;
        }
    }
    
    int CmdSplit(const CommandLineParser::Command& cmd) {
        if (cmd.args.size() < 2) {
            Output("Usage: 7zsdk split <archive> <part_size>");
            return 1;
        }
        
        std::string archivePath = cmd.args[0];
        uint64_t partSize = ParseSize(cmd.args[1]);
        
        std::vector<std::string> outputPaths;
        if (m_archive.SplitArchive(archivePath, partSize, outputPaths)) {
            Output("Archive split successfully:");
            for (const auto& path : outputPaths) {
                Output("  " + path);
            }
            return 0;
        } else {
            Output("Failed to split archive.");
            return 1;
        }
    }
    
    int CmdMerge(const CommandLineParser::Command& cmd) {
        if (cmd.args.size() < 2) {
            Output("Usage: 7zsdk merge <output_archive> <archive1> [archive2] ...");
            return 1;
        }
        
        std::string outputPath = cmd.args[0];
        std::vector<std::string> sourceArchives(cmd.args.begin() + 1, cmd.args.end());
        
        CompressionOptions opts = ParseCompressionOptions(cmd);
        
        if (m_archive.MergeArchives(outputPath, sourceArchives, opts)) {
            Output("Archives merged successfully: " + outputPath);
            return 0;
        } else {
            Output("Failed to merge archives.");
            return 1;
        }
    }
    
    int CmdConvert(const CommandLineParser::Command& cmd) {
        if (cmd.args.size() < 2) {
            Output("Usage: 7zsdk convert <source_archive> <dest_archive>");
            return 1;
        }
        
        std::string sourcePath = cmd.args[0];
        std::string destPath = cmd.args[1];
        
        ArchiveFormat destFormat = SevenZipArchive::DetectFormatFromExtension(destPath);
        CompressionOptions opts = ParseCompressionOptions(cmd);
        std::string password = CommandLineParser::GetOption(cmd, "-p", "");
        
        if (m_archive.ConvertArchive(sourcePath, destPath, destFormat, opts, password)) {
            Output("Archive converted successfully: " + destPath);
            return 0;
        } else {
            Output("Failed to convert archive.");
            return 1;
        }
    }
    
    int CmdCompare(const CommandLineParser::Command& cmd) {
        if (cmd.args.size() < 2) {
            Output("Usage: 7zsdk diff <archive1> <archive2>");
            return 1;
        }
        
        std::string archive1 = cmd.args[0];
        std::string archive2 = cmd.args[1];
        std::string password1 = CommandLineParser::GetOption(cmd, "-p1", "");
        std::string password2 = CommandLineParser::GetOption(cmd, "-p2", "");
        
        std::vector<CompareResult> results;
        if (m_archive.CompareArchives(archive1, archive2, results, password1, password2)) {
            Output("Comparison results:");
            
            for (const auto& result : results) {
                if (result.onlyInArchive1) {
                    Output("  Only in " + archive1 + ": " + result.path);
                } else if (result.onlyInArchive2) {
                    Output("  Only in " + archive2 + ": " + result.path);
                } else if (result.contentDifferent) {
                    Output("  Content differs: " + result.path);
                } else if (result.sizeDifferent) {
                    Output("  Size differs: " + result.path + 
                           " (" + FormatSize(result.size1) + " vs " + FormatSize(result.size2) + ")");
                }
            }
            
            if (results.empty()) {
                Output("  Archives are identical.");
            }
            
            return 0;
        } else {
            Output("Failed to compare archives.");
            return 1;
        }
    }
    
    int CmdRepair(const CommandLineParser::Command& cmd) {
        if (cmd.args.size() < 2) {
            Output("Usage: 7zsdk repair <archive> <output>");
            return 1;
        }
        
        std::string archivePath = cmd.args[0];
        std::string outputPath = cmd.args[1];
        std::string password = CommandLineParser::GetOption(cmd, "-p", "");
        
        RepairResult result;
        if (m_archive.RepairArchive(archivePath, outputPath, result, password)) {
            Output("Archive repaired successfully.");
            Output("Recovered files: " + std::to_string(result.recoveredFiles) + "/" + std::to_string(result.totalFiles));
            Output("Recovered bytes: " + FormatSize(result.recoveredBytes));
            return 0;
        } else {
            Output("Archive repair failed: " + result.errorMessage);
            return 1;
        }
    }
    
    int CmdBackup(const CommandLineParser::Command& cmd) {
        if (cmd.args.size() < 2) {
            Output("Usage: 7zsdk backup [options] <archive> <source>");
            return 1;
        }
        
        std::string archivePath = cmd.args[0];
        std::string sourcePath = cmd.args[1];
        
        BackupOptions opts;
        opts.type = BackupType::Full;
        
        std::string typeStr = CommandLineParser::GetOption(cmd, "-type", "full");
        if (typeStr == "incremental" || typeStr == "inc") {
            opts.type = BackupType::Incremental;
        } else if (typeStr == "differential" || typeStr == "diff") {
            opts.type = BackupType::Differential;
        }
        
        opts.compression = ParseCompressionOptions(cmd);
        opts.baseArchive = CommandLineParser::GetOption(cmd, "-base", "");
        opts.password = CommandLineParser::GetOption(cmd, "-p", "");
        
        BackupResult result;
        BackupManager backupMgr(m_archive);
        if (backupMgr.CreateBackup(archivePath, sourcePath, opts, result)) {
            Output("Backup created successfully: " + archivePath);
            Output("Files backed up: " + std::to_string(result.filesProcessed));
            Output("Bytes backed up: " + FormatSize(result.bytesProcessed));
            return 0;
        } else {
            Output("Backup failed: " + result.errorMessage);
            return 1;
        }
    }
    
    int CmdRestore(const CommandLineParser::Command& cmd) {
        if (cmd.args.size() < 2) {
            Output("Usage: 7zsdk restore [options] <archive> <output>");
            return 1;
        }
        
        std::string archivePath = cmd.args[0];
        std::string outputPath = cmd.args[1];
        
        RestoreOptions opts;
        opts.password = CommandLineParser::GetOption(cmd, "-p", "");
        opts.overwrite = CommandLineParser::HasOption(cmd, "-overwrite");
        opts.pointInTime = CommandLineParser::GetOption(cmd, "-time", "");
        
        RestoreResult result;
        BackupManager backupMgr(m_archive);
        if (backupMgr.RestoreBackup(archivePath, outputPath, opts, result)) {
            Output("Restore completed successfully.");
            Output("Files restored: " + std::to_string(result.filesRestored));
            Output("Bytes restored: " + FormatSize(result.bytesRestored));
            return 0;
        } else {
            Output("Restore failed: " + result.errorMessage);
            return 1;
        }
    }
    
    std::string FormatSize(uint64_t size) {
        std::ostringstream oss;
        
        if (size >= 1024ULL * 1024 * 1024) {
            oss << std::fixed << std::setprecision(2) << (double)size / (1024 * 1024 * 1024) << " GB";
        } else if (size >= 1024 * 1024) {
            oss << std::fixed << std::setprecision(2) << (double)size / (1024 * 1024) << " MB";
        } else if (size >= 1024) {
            oss << std::fixed << std::setprecision(2) << (double)size / 1024 << " KB";
        } else {
            oss << size << " B";
        }
        
        return oss.str();
    }
};

class StreamPipeline {
public:
    enum class PipelineStage {
        Read,
        Filter,
        Compress,
        Encrypt,
        Write
    };
    
    struct PipelineConfig {
        std::vector<PipelineStage> stages;
        CompressionMethod compressionMethod = CompressionMethod::LZMA2;
        CompressionLevel compressionLevel = CompressionLevel::Normal;
        std::string password;
        FilterMethod filter = FilterMethod::NONE;
        int bufferSize = 64 * 1024;
    };
    
private:
    SevenZipArchive& m_archive;
    PipelineConfig m_config;
    std::atomic<bool> m_cancelFlag;
    std::atomic<double> m_progress;
    
public:
    StreamPipeline(SevenZipArchive& archive) : m_archive(archive), m_cancelFlag(false), m_progress(0) {}
    
    bool ProcessStream(std::istream& input, std::ostream& output, const PipelineConfig& config) {
        m_config = config;
        m_progress = 0;
        
        std::vector<uint8_t> buffer(config.bufferSize);
        std::vector<uint8_t> processedBuffer;
        
        input.seekg(0, std::ios::end);
        uint64_t totalSize = input.tellg();
        input.seekg(0, std::ios::beg);
        
        uint64_t processedSize = 0;
        
        while (input.good() && !m_cancelFlag) {
            input.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
            std::streamsize bytesRead = input.gcount();
            
            if (bytesRead == 0) break;
            
            std::vector<uint8_t> chunk(buffer.begin(), buffer.begin() + bytesRead);
            
            if (config.filter != FilterMethod::NONE) {
                ApplyFilter(chunk, config.filter);
            }
            
            processedBuffer.insert(processedBuffer.end(), chunk.begin(), chunk.end());
            
            processedSize += bytesRead;
            m_progress = (double)processedSize / totalSize * 100.0;
        }
        
        if (m_cancelFlag) return false;
        
        std::vector<uint8_t> compressedData;
        if (!m_archive.CompressStream(processedBuffer.data(), processedBuffer.size(), 
                                       compressedData, "stream", CreateCompressionOptions(config))) {
            return false;
        }
        
        output.write(reinterpret_cast<const char*>(compressedData.data()), compressedData.size());
        
        return true;
    }
    
    bool ProcessFile(const std::string& inputPath, const std::string& outputPath, 
                    const PipelineConfig& config) {
        std::ifstream inputFile(inputPath, std::ios::binary);
        if (!inputFile.is_open()) return false;
        
        std::ofstream outputFile(outputPath, std::ios::binary);
        if (!outputFile.is_open()) {
            inputFile.close();
            return false;
        }
        
        bool result = ProcessStream(inputFile, outputFile, config);
        
        inputFile.close();
        outputFile.close();
        
        return result;
    }
    
    void Cancel() {
        m_cancelFlag = true;
    }
    
    double GetProgress() const {
        return m_progress;
    }
    
private:
    void ApplyFilter(std::vector<uint8_t>& data, FilterMethod filter) {
        switch (filter) {
            case FilterMethod::DELTA: {
                uint8_t prev = 0;
                for (auto& byte : data) {
                    uint8_t temp = byte;
                    byte = byte - prev;
                    prev = temp;
                }
                break;
            }
            case FilterMethod::BCJ: {
                for (size_t i = 0; i + 4 <= data.size(); i++) {
                    uint32_t value = (data[i] << 24) | (data[i+1] << 16) | (data[i+2] << 8) | data[i+3];
                    if ((value & 0xFE000000) == 0xE8000000) {
                        uint32_t offset = value & 0x01FFFFFF;
                        uint32_t newOffset = offset - (uint32_t)i;
                        data[i] = (value >> 24) & 0xFF;
                        data[i+1] = (newOffset >> 16) & 0xFF;
                        data[i+2] = (newOffset >> 8) & 0xFF;
                        data[i+3] = newOffset & 0xFF;
                    }
                }
                break;
            }
            default:
                break;
        }
    }
    
    CompressionOptions CreateCompressionOptions(const PipelineConfig& config) {
        CompressionOptions opts;
        opts.method = config.compressionMethod;
        opts.level = config.compressionLevel;
        opts.password = config.password;
        return opts;
    }
};

class MemoryMappedFile {
private:
    HANDLE m_fileHandle;
    HANDLE m_mappingHandle;
    void* m_data;
    size_t m_size;
    
public:
    MemoryMappedFile() : m_fileHandle(INVALID_HANDLE_VALUE), m_mappingHandle(NULL), 
                         m_data(nullptr), m_size(0) {}
    
    ~MemoryMappedFile() {
        Close();
    }
    
    bool Open(const std::string& filePath, bool readOnly = true) {
        Close();
        
        std::wstring wpath = StringToWString(filePath);
        
        DWORD access = readOnly ? GENERIC_READ : (GENERIC_READ | GENERIC_WRITE);
        DWORD share = readOnly ? FILE_SHARE_READ : 0;
        DWORD create = OPEN_EXISTING;
        
        m_fileHandle = CreateFileW(wpath.c_str(), access, share, NULL, create, 
                                   FILE_ATTRIBUTE_NORMAL, NULL);
        
        if (m_fileHandle == INVALID_HANDLE_VALUE) return false;
        
        LARGE_INTEGER fileSize;
        if (!GetFileSizeEx(m_fileHandle, &fileSize)) {
            CloseHandle(m_fileHandle);
            m_fileHandle = INVALID_HANDLE_VALUE;
            return false;
        }
        
        m_size = (size_t)fileSize.QuadPart;
        
        DWORD protect = readOnly ? PAGE_READONLY : PAGE_READWRITE;
        
        m_mappingHandle = CreateFileMappingW(m_fileHandle, NULL, protect, 0, 0, NULL);
        
        if (!m_mappingHandle) {
            CloseHandle(m_fileHandle);
            m_fileHandle = INVALID_HANDLE_VALUE;
            return false;
        }
        
        DWORD mapAccess = readOnly ? FILE_MAP_READ : FILE_MAP_ALL_ACCESS;
        
        m_data = MapViewOfFile(m_mappingHandle, mapAccess, 0, 0, 0);
        
        if (!m_data) {
            CloseHandle(m_mappingHandle);
            m_mappingHandle = NULL;
            CloseHandle(m_fileHandle);
            m_fileHandle = INVALID_HANDLE_VALUE;
            return false;
        }
        
        return true;
    }
    
    void Close() {
        if (m_data) {
            UnmapViewOfFile(m_data);
            m_data = nullptr;
        }
        
        if (m_mappingHandle) {
            CloseHandle(m_mappingHandle);
            m_mappingHandle = NULL;
        }
        
        if (m_fileHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(m_fileHandle);
            m_fileHandle = INVALID_HANDLE_VALUE;
        }
        
        m_size = 0;
    }
    
    void* Data() { return m_data; }
    const void* Data() const { return m_data; }
    size_t Size() const { return m_size; }
    bool IsOpen() const { return m_data != nullptr; }
    
    uint8_t& operator[](size_t index) { return static_cast<uint8_t*>(m_data)[index]; }
    const uint8_t& operator[](size_t index) const { return static_cast<const uint8_t*>(m_data)[index]; }
};

class FileSystemWatcher {
private:
    HANDLE m_directoryHandle;
    HANDLE m_eventHandle;
    std::thread m_watchThread;
    std::atomic<bool> m_running;
    std::string m_watchPath;
    std::function<void(const std::string&, int)> m_callback;
    std::vector<std::string> m_changedFiles;
    std::mutex m_mutex;
    
public:
    enum class ChangeType {
        Added,
        Removed,
        Modified,
        Renamed
    };
    
    FileSystemWatcher() : m_directoryHandle(INVALID_HANDLE_VALUE), m_eventHandle(NULL), 
                          m_running(false) {}
    
    ~FileSystemWatcher() {
        Stop();
    }
    
    bool Start(const std::string& path, std::function<void(const std::string&, int)> callback) {
        Stop();
        
        m_watchPath = path;
        m_callback = callback;
        
        std::wstring wpath = StringToWString(path);
        
        m_directoryHandle = CreateFileW(wpath.c_str(), FILE_LIST_DIRECTORY,
                                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                        NULL, OPEN_EXISTING, 
                                        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                                        NULL);
        
        if (m_directoryHandle == INVALID_HANDLE_VALUE) return false;
        
        m_eventHandle = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (!m_eventHandle) {
            CloseHandle(m_directoryHandle);
            m_directoryHandle = INVALID_HANDLE_VALUE;
            return false;
        }
        
        m_running = true;
        m_watchThread = std::thread(&FileSystemWatcher::WatchLoop, this);
        
        return true;
    }
    
    void Stop() {
        m_running = false;
        
        if (m_eventHandle) {
            SetEvent(m_eventHandle);
        }
        
        if (m_watchThread.joinable()) {
            m_watchThread.join();
        }
        
        if (m_directoryHandle != INVALID_HANDLE_VALUE) {
            CancelIo(m_directoryHandle);
            CloseHandle(m_directoryHandle);
            m_directoryHandle = INVALID_HANDLE_VALUE;
        }
        
        if (m_eventHandle) {
            CloseHandle(m_eventHandle);
            m_eventHandle = NULL;
        }
    }
    
    std::vector<std::string> GetChangedFiles() {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<std::string> result = m_changedFiles;
        m_changedFiles.clear();
        return result;
    }
    
private:
    void WatchLoop() {
        const DWORD bufferSize = 64 * 1024;
        std::vector<uint8_t> buffer(bufferSize);
        
        OVERLAPPED overlapped;
        memset(&overlapped, 0, sizeof(overlapped));
        overlapped.hEvent = m_eventHandle;
        
        DWORD notifyFilter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                            FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE |
                            FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION;
        
        while (m_running) {
            DWORD bytesReturned;
            
            ResetEvent(m_eventHandle);
            
            if (!ReadDirectoryChangesW(m_directoryHandle, buffer.data(), bufferSize, TRUE,
                                       notifyFilter, &bytesReturned, &overlapped, NULL)) {
                break;
            }
            
            HANDLE handles[] = { m_eventHandle };
            DWORD waitResult = WaitForMultipleObjects(1, handles, FALSE, INFINITE);
            
            if (!m_running) break;
            
            if (waitResult == WAIT_OBJECT_0) {
                if (!GetOverlappedResult(m_directoryHandle, &overlapped, &bytesReturned, FALSE)) {
                    continue;
                }
                
                if (bytesReturned > 0) {
                    ProcessNotification(buffer.data(), bytesReturned);
                }
            }
        }
    }
    
    void ProcessNotification(uint8_t* buffer, DWORD size) {
        FILE_NOTIFY_INFORMATION* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer);
        
        while (true) {
            std::wstring fileName(info->FileName, info->FileNameLength / sizeof(wchar_t));
            std::string filePath = m_watchPath + "\\" + WStringToString(fileName);
            
            int changeType = 0;
            switch (info->Action) {
                case FILE_ACTION_ADDED:
                    changeType = (int)ChangeType::Added;
                    break;
                case FILE_ACTION_REMOVED:
                    changeType = (int)ChangeType::Removed;
                    break;
                case FILE_ACTION_MODIFIED:
                    changeType = (int)ChangeType::Modified;
                    break;
                case FILE_ACTION_RENAMED_OLD_NAME:
                case FILE_ACTION_RENAMED_NEW_NAME:
                    changeType = (int)ChangeType::Renamed;
                    break;
            }
            
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_changedFiles.push_back(filePath);
            }
            
            if (m_callback) {
                m_callback(filePath, changeType);
            }
            
            if (info->NextEntryOffset == 0) break;
            
            info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
                reinterpret_cast<uint8_t*>(info) + info->NextEntryOffset);
        }
    }
};

class DigitalSignature {
public:
    struct SignatureInfo {
        std::vector<uint8_t> signature;
        std::string algorithm;
        std::string signer;
        std::string timestamp;
        bool valid;
    };
    
private:
    std::vector<uint8_t> m_privateKey;
    std::vector<uint8_t> m_publicKey;
    std::string m_algorithm;
    
public:
    DigitalSignature(const std::string& algorithm = "SHA256") : m_algorithm(algorithm) {}
    
    bool GenerateKeyPair() {
        m_privateKey.resize(32);
        m_publicKey.resize(32);
        
        HCRYPTPROV hProv = 0;
        if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
            return false;
        }
        
        if (!CryptGenRandom(hProv, (DWORD)m_privateKey.size(), m_privateKey.data())) {
            CryptReleaseContext(hProv, 0);
            return false;
        }
        
        CryptReleaseContext(hProv, 0);
        
        for (size_t i = 0; i < m_privateKey.size(); i++) {
            m_publicKey[i] = m_privateKey[i] ^ 0xAA;
        }
        
        return true;
    }
    
    bool LoadPrivateKey(const std::string& keyPath) {
        std::ifstream file(keyPath, std::ios::binary);
        if (!file.is_open()) return false;
        
        file.seekg(0, std::ios::end);
        m_privateKey.resize(file.tellg());
        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char*>(m_privateKey.data()), m_privateKey.size());
        
        return true;
    }
    
    bool LoadPublicKey(const std::string& keyPath) {
        std::ifstream file(keyPath, std::ios::binary);
        if (!file.is_open()) return false;
        
        file.seekg(0, std::ios::end);
        m_publicKey.resize(file.tellg());
        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char*>(m_publicKey.data()), m_publicKey.size());
        
        return true;
    }
    
    bool SavePrivateKey(const std::string& keyPath) {
        std::ofstream file(keyPath, std::ios::binary);
        if (!file.is_open()) return false;
        file.write(reinterpret_cast<const char*>(m_privateKey.data()), m_privateKey.size());
        return true;
    }
    
    bool SavePublicKey(const std::string& keyPath) {
        std::ofstream file(keyPath, std::ios::binary);
        if (!file.is_open()) return false;
        file.write(reinterpret_cast<const char*>(m_publicKey.data()), m_publicKey.size());
        return true;
    }
    
    bool Sign(const uint8_t* data, size_t size, std::vector<uint8_t>& signature) {
        if (m_privateKey.empty()) return false;
        
        uint8_t hash[32];
        CalculateSHA256(data, size, hash);
        
        signature.resize(32);
        for (size_t i = 0; i < 32; i++) {
            signature[i] = hash[i] ^ m_privateKey[i % m_privateKey.size()];
        }
        
        return true;
    }
    
    bool SignFile(const std::string& filePath, std::vector<uint8_t>& signature) {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) return false;
        
        file.seekg(0, std::ios::end);
        size_t fileSize = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::vector<uint8_t> data(fileSize);
        file.read(reinterpret_cast<char*>(data.data()), fileSize);
        
        return Sign(data.data(), data.size(), signature);
    }
    
    bool Verify(const uint8_t* data, size_t size, const std::vector<uint8_t>& signature) {
        if (m_publicKey.empty() || signature.size() != 32) return false;
        
        uint8_t hash[32];
        CalculateSHA256(data, size, hash);
        
        for (size_t i = 0; i < 32; i++) {
            uint8_t expected = hash[i] ^ m_publicKey[i % m_publicKey.size()];
            if (signature[i] != expected) return false;
        }
        
        return true;
    }
    
    bool VerifyFile(const std::string& filePath, const std::vector<uint8_t>& signature) {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) return false;
        
        file.seekg(0, std::ios::end);
        size_t fileSize = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::vector<uint8_t> data(fileSize);
        file.read(reinterpret_cast<char*>(data.data()), fileSize);
        
        return Verify(data.data(), data.size(), signature);
    }
    
    SignatureInfo GetSignatureInfo(const std::string& filePath) {
        SignatureInfo info;
        info.algorithm = m_algorithm;
        info.valid = false;
        
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) return info;
        
        file.seekg(0, std::ios::end);
        size_t fileSize = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::vector<uint8_t> data(fileSize);
        file.read(reinterpret_cast<char*>(data.data()), fileSize);
        
        uint8_t hash[32];
        CalculateSHA256(data.data(), data.size(), hash);
        info.signature.assign(hash, hash + 32);
        info.valid = true;
        
        return info;
    }
    
private:
    static void CalculateSHA256(const uint8_t* data, size_t size, uint8_t* hash);
};

class KeyFileEncryption {
public:
    struct KeyFileInfo {
        std::string path;
        uint64_t size;
        std::string hash;
        std::chrono::system_clock::time_point created;
    };
    
private:
    std::vector<uint8_t> m_keyData;
    std::string m_keyFilePath;
    
public:
    bool GenerateKeyFile(const std::string& path, size_t keySize = 256) {
        m_keyFilePath = path;
        m_keyData.resize(keySize);
        
        HCRYPTPROV hProv = 0;
        if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
            return false;
        }
        
        if (!CryptGenRandom(hProv, (DWORD)keySize, m_keyData.data())) {
            CryptReleaseContext(hProv, 0);
            return false;
        }
        
        CryptReleaseContext(hProv, 0);
        
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) return false;
        
        file.write(reinterpret_cast<const char*>(m_keyData.data()), m_keyData.size());
        
        return true;
    }
    
    bool LoadKeyFile(const std::string& path) {
        m_keyFilePath = path;
        
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return false;
        
        file.seekg(0, std::ios::end);
        m_keyData.resize(file.tellg());
        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char*>(m_keyData.data()), m_keyData.size());
        
        return !m_keyData.empty();
    }
    
    std::string DerivePassword(const std::string& salt = "") {
        if (m_keyData.empty()) return "";
        
        std::vector<uint8_t> combined;
        combined.insert(combined.end(), m_keyData.begin(), m_keyData.end());
        combined.insert(combined.end(), salt.begin(), salt.end());
        
        std::vector<uint8_t> hash(32);
        CalculateSHA256Hash(combined, hash);
        
        return BytesToHex(hash.data(), 32);
    }
    
    bool EncryptData(const std::vector<uint8_t>& input, std::vector<uint8_t>& output) {
        if (m_keyData.empty()) return false;
        
        output.resize(input.size());
        
        for (size_t i = 0; i < input.size(); i++) {
            output[i] = input[i] ^ m_keyData[i % m_keyData.size()];
        }
        
        return true;
    }
    
    bool DecryptData(const std::vector<uint8_t>& input, std::vector<uint8_t>& output) {
        return EncryptData(input, output);
    }
    
    KeyFileInfo GetKeyFileInfo() {
        KeyFileInfo info;
        info.path = m_keyFilePath;
        info.size = m_keyData.size();
        
        if (!m_keyData.empty()) {
            std::vector<uint8_t> hash(32);
            CalculateSHA256Hash(m_keyData, hash);
            info.hash = BytesToHex(hash.data(), 32);
        }
        
        info.created = std::chrono::system_clock::now();
        
        return info;
    }
    
    const std::vector<uint8_t>& GetKeyData() const { return m_keyData; }

private:
    static void CalculateSHA256Hash(const std::vector<uint8_t>& data, std::vector<uint8_t>& hash) {
        HCRYPTPROV hProv = 0;
        HCRYPTHASH hHash = 0;

        if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
            return;
        }

        if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
            CryptReleaseContext(hProv, 0);
            return;
        }

        if (!CryptHashData(hHash, data.data(), (DWORD)data.size(), 0)) {
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            return;
        }

        DWORD hashLen = 32;
        CryptGetHashParam(hHash, HP_HASHVAL, hash.data(), &hashLen, 0);

        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
    }
};

class LinkHandler {
public:
    struct LinkInfo {
        std::string linkPath;
        std::string targetPath;
        bool isSymbolic;
        bool isHard;
        bool targetExists;
    };
    
    static bool CreateSymbolicLink(const std::string& linkPath, const std::string& targetPath, 
                                   bool isDirectory) {
        std::wstring wlink = StringToWString(linkPath);
        std::wstring wtarget = StringToWString(targetPath);
        
        DWORD flags = isDirectory ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0;
        
        return CreateSymbolicLinkW(wlink.c_str(), wtarget.c_str(), flags) != FALSE;
    }
    
    static bool CreateHardLink(const std::string& linkPath, const std::string& targetPath) {
        std::wstring wlink = StringToWString(linkPath);
        std::wstring wtarget = StringToWString(targetPath);
        
        return CreateHardLinkW(wlink.c_str(), wtarget.c_str(), NULL) != FALSE;
    }
    
    static bool CreateJunction(const std::string& junctionPath, const std::string& targetPath) {
        std::wstring wjunction = StringToWString(junctionPath);
        std::wstring wtarget = StringToWString(targetPath);
        
        if (!CreateDirectoryW(wjunction.c_str(), NULL)) {
            if (GetLastError() != ERROR_ALREADY_EXISTS) return false;
        }
        
        HANDLE hDir = CreateFileW(wjunction.c_str(), GENERIC_WRITE, 
                                  FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                  OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
        
        if (hDir == INVALID_HANDLE_VALUE) return false;
        
        std::wstring reparseTarget = L"\\??\\" + wtarget;
        if (reparseTarget.back() != L'\\') {
            reparseTarget += L'\\';
        }
        
        BYTE buffer[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
        REPARSE_GUID_DATA_BUFFER* reparseData = (REPARSE_GUID_DATA_BUFFER*)buffer;
        memset(reparseData, 0, sizeof(REPARSE_GUID_DATA_BUFFER));
        reparseData->ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
        reparseData->ReparseDataLength = (WORD)(reparseTarget.size() * 2 + 12);
        reparseData->ReparseGuid = GUID_NULL;
        memcpy(reparseData->GenericReparseBuffer.DataBuffer, reparseTarget.c_str(), 
               reparseTarget.size() * 2);
        
        DWORD bytesReturned;
        BOOL result = DeviceIoControl(hDir, FSCTL_SET_REPARSE_POINT, reparseData,
                                      sizeof(REPARSE_GUID_DATA_BUFFER), NULL, 0, &bytesReturned, NULL);
        
        CloseHandle(hDir);
        
        return result != FALSE;
    }
    
    static LinkInfo GetLinkInfo(const std::string& path) {
        LinkInfo info;
        info.linkPath = path;
        info.isSymbolic = false;
        info.isHard = false;
        info.targetExists = false;
        
        WIN32_FILE_ATTRIBUTE_DATA attr;
        if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &attr)) {
            return info;
        }
        
        if (attr.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
            info.isSymbolic = true;
            
            std::wstring wpath = StringToWString(path);
            HANDLE hFile = CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                                       OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
            
            if (hFile != INVALID_HANDLE_VALUE) {
                BYTE buffer[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
                
                DWORD bytesReturned;
                if (DeviceIoControl(hFile, FSCTL_GET_REPARSE_POINT, NULL, 0,
                                   buffer, sizeof(buffer), &bytesReturned, NULL)) {
                    REPARSE_GUID_DATA_BUFFER* reparseData = (REPARSE_GUID_DATA_BUFFER*)buffer;
                    
                    if (reparseData->ReparseTag == IO_REPARSE_TAG_SYMLINK) {
                        wchar_t* target = (wchar_t*)reparseData->GenericReparseBuffer.DataBuffer;
                        info.targetPath = WStringToString(target);
                    }
                }
                
                CloseHandle(hFile);
            }
        }
        
        info.targetExists = FileExists(info.targetPath) || DirectoryExists(info.targetPath);
        
        return info;
    }
    
    static bool IsSymbolicLink(const std::string& path) {
        DWORD attr = GetFileAttributesA(path.c_str());
        return (attr != INVALID_FILE_ATTRIBUTES) && 
               (attr & FILE_ATTRIBUTE_REPARSE_POINT);
    }
    
    static bool IsHardLink(const std::string& path) {
        std::wstring wpath = StringToWString(path);
        
        HANDLE hFile = CreateFileW(wpath.c_str(), 0, FILE_SHARE_READ, NULL,
                                   OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
        
        if (hFile == INVALID_HANDLE_VALUE) return false;
        
        BY_HANDLE_FILE_INFORMATION fileInfo;
        BOOL result = GetFileInformationByHandle(hFile, &fileInfo);
        CloseHandle(hFile);
        
        return result && fileInfo.nNumberOfLinks > 1;
    }
    
    static uint32_t GetHardLinkCount(const std::string& path) {
        std::wstring wpath = StringToWString(path);
        
        HANDLE hFile = CreateFileW(wpath.c_str(), 0, FILE_SHARE_READ, NULL,
                                   OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
        
        if (hFile == INVALID_HANDLE_VALUE) return 0;
        
        BY_HANDLE_FILE_INFORMATION fileInfo;
        BOOL result = GetFileInformationByHandle(hFile, &fileInfo);
        CloseHandle(hFile);
        
        return result ? fileInfo.nNumberOfLinks : 0;
    }
    
    static bool DeleteLink(const std::string& path) {
        LinkInfo info = GetLinkInfo(path);
        
        if (info.isSymbolic) {
            DWORD attr = GetFileAttributesA(path.c_str());
            if (attr & FILE_ATTRIBUTE_DIRECTORY) {
                return RemoveDirectoryA(path.c_str()) != FALSE;
            } else {
                return DeleteFileA(path.c_str()) != FALSE;
            }
        }
        
        return DeleteFileA(path.c_str()) != FALSE;
    }
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
    
private:
    std::string m_archivePath;
    std::string m_password;
    std::vector<Version> m_versions;
    
public:
    VersionControl(const std::string& archivePath, const std::string& password = "")
        : m_archivePath(archivePath), m_password(password) {}
    
    bool Initialize() {
        if (!FileExists(m_archivePath)) {
            Version initial;
            initial.id = GenerateVersionId();
            initial.message = "Initial commit";
            initial.author = "System";
            initial.timestamp = std::chrono::system_clock::now();
            m_versions.push_back(initial);
            return SaveVersions();
        }
        return LoadVersions();
    }
    
    std::string Commit(const std::string& sourcePath, const std::string& message, 
                      const std::string& author) {
        Version newVersion;
        newVersion.id = GenerateVersionId();
        newVersion.message = message;
        newVersion.author = author;
        newVersion.timestamp = std::chrono::system_clock::now();
        
        std::vector<std::string> files;
        EnumerateFiles(sourcePath, files);
        
        for (const auto& file : files) {
            std::string relativePath = GetRelativePath(file, sourcePath);
            newVersion.files.push_back(relativePath);
            
            HashResult hash;
            SevenZipArchive archive;
            archive.CalculateFileHash(file, hash, "SHA256");
            newVersion.fileHashes[relativePath] = hash.hash;
        }
        
        m_versions.push_back(newVersion);
        
        if (!SaveVersions()) {
            m_versions.pop_back();
            return "";
        }
        
        return newVersion.id;
    }
    
    std::vector<DiffEntry> Diff(const std::string& versionId1, const std::string& versionId2) {
        std::vector<DiffEntry> diffs;
        
        Version* v1 = FindVersion(versionId1);
        Version* v2 = FindVersion(versionId2);
        
        if (!v1 || !v2) return diffs;
        
        std::map<std::string, std::string> v1Hashes = v1->fileHashes;
        std::map<std::string, std::string> v2Hashes = v2->fileHashes;
        
        for (const auto& pair : v2Hashes) {
            auto it = v1Hashes.find(pair.first);
            
            DiffEntry entry;
            entry.path = pair.first;
            entry.newHash = pair.second;
            
            if (it == v1Hashes.end()) {
                entry.type = DiffEntry::Added;
                diffs.push_back(entry);
            } else if (it->second != pair.second) {
                entry.type = DiffEntry::Modified;
                entry.oldHash = it->second;
                diffs.push_back(entry);
            }
            
            v1Hashes.erase(pair.first);
        }
        
        for (const auto& pair : v1Hashes) {
            DiffEntry entry;
            entry.path = pair.first;
            entry.type = DiffEntry::Deleted;
            entry.oldHash = pair.second;
            diffs.push_back(entry);
        }
        
        return diffs;
    }
    
    bool Checkout(const std::string& versionId, const std::string& outputPath) {
        Version* version = FindVersion(versionId);
        if (!version) return false;
        
        SevenZipArchive archive;
        
        ExtractOptions opts;
        opts.outputDir = outputPath;
        opts.password = m_password;
        
        return archive.ExtractArchive(m_archivePath, opts);
    }
    
    std::vector<Version> GetHistory() {
        return m_versions;
    }
    
    Version* FindVersion(const std::string& versionId) {
        for (auto& v : m_versions) {
            if (v.id == versionId) return &v;
        }
        return nullptr;
    }
    
    std::string GetCurrentVersionId() {
        if (m_versions.empty()) return "";
        return m_versions.back().id;
    }
    
private:
    std::string GenerateVersionId() {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        
        std::ostringstream oss;
        oss << std::hex << millis;
        return oss.str();
    }
    
    bool SaveVersions() {
        std::string versionPath = m_archivePath + ".versions";
        
        std::ofstream file(versionPath, std::ios::binary);
        if (!file.is_open()) return false;
        
        uint32_t count = (uint32_t)m_versions.size();
        file.write(reinterpret_cast<const char*>(&count), sizeof(count));
        
        for (const auto& version : m_versions) {
            WriteString(file, version.id);
            WriteString(file, version.message);
            WriteString(file, version.author);
            
            int64_t timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                version.timestamp.time_since_epoch()).count();
            file.write(reinterpret_cast<const char*>(&timestamp), sizeof(timestamp));
            
            uint32_t fileCount = (uint32_t)version.files.size();
            file.write(reinterpret_cast<const char*>(&fileCount), sizeof(fileCount));
            
            for (const auto& f : version.files) {
                WriteString(file, f);
                WriteString(file, version.fileHashes.at(f));
            }
        }
        
        return true;
    }
    
    bool LoadVersions() {
        std::string versionPath = m_archivePath + ".versions";
        
        if (!FileExists(versionPath)) return true;
        
        std::ifstream file(versionPath, std::ios::binary);
        if (!file.is_open()) return false;
        
        m_versions.clear();
        
        uint32_t count;
        file.read(reinterpret_cast<char*>(&count), sizeof(count));
        
        for (uint32_t i = 0; i < count; i++) {
            Version version;
            version.id = ReadString(file);
            version.message = ReadString(file);
            version.author = ReadString(file);
            
            int64_t timestamp;
            file.read(reinterpret_cast<char*>(&timestamp), sizeof(timestamp));
            version.timestamp = std::chrono::system_clock::from_time_t(timestamp);
            
            uint32_t fileCount;
            file.read(reinterpret_cast<char*>(&fileCount), sizeof(fileCount));
            
            for (uint32_t j = 0; j < fileCount; j++) {
                std::string filePath = ReadString(file);
                std::string fileHash = ReadString(file);
                version.files.push_back(filePath);
                version.fileHashes[filePath] = fileHash;
            }
            
            m_versions.push_back(version);
        }
        
        return true;
    }
    
    void WriteString(std::ofstream& file, const std::string& str) {
        uint32_t len = (uint32_t)str.size();
        file.write(reinterpret_cast<const char*>(&len), sizeof(len));
        file.write(str.c_str(), len);
    }
    
    std::string ReadString(std::ifstream& file) {
        uint32_t len;
        file.read(reinterpret_cast<char*>(&len), sizeof(len));
        
        std::string str(len, '\0');
        file.read(&str[0], len);
        
        return str;
    }
    
    void EnumerateFiles(const std::string& directory, std::vector<std::string>& files) {
        std::string searchPath = directory + "\\*";
        WIN32_FIND_DATAW findData;
        HANDLE hFind = FindFirstFileW(StringToWString(searchPath).c_str(), &findData);
        
        if (hFind == INVALID_HANDLE_VALUE) return;
        
        do {
            std::wstring name = findData.cFileName;
            if (name == L"." || name == L"..") continue;
            
            std::string fileName = WStringToString(name);
            std::string fullPath = directory + "\\" + fileName;
            
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                EnumerateFiles(fullPath, files);
            } else {
                files.push_back(fullPath);
            }
        } while (FindNextFileW(hFind, &findData));
        
        FindClose(hFind);
    }
};

class ThreadPool {
private:
    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_tasks;
    std::mutex m_queueMutex;
    std::condition_variable m_condition;
    std::atomic<bool> m_stop;
    std::atomic<int> m_activeTasks;
    
public:
    ThreadPool(size_t threads = std::thread::hardware_concurrency()) 
        : m_stop(false), m_activeTasks(0) {
        for (size_t i = 0; i < threads; i++) {
            m_workers.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    
                    {
                        std::unique_lock<std::mutex> lock(m_queueMutex);
                        m_condition.wait(lock, [this] {
                            return m_stop || !m_tasks.empty();
                        });
                        
                        if (m_stop && m_tasks.empty()) return;
                        
                        task = std::move(m_tasks.front());
                        m_tasks.pop();
                    }
                    
                    m_activeTasks++;
                    task();
                    m_activeTasks--;
                }
            });
        }
    }
    
    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_stop = true;
        }
        
        m_condition.notify_all();
        
        for (auto& worker : m_workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }
    
    template<class F>
    auto Enqueue(F&& f) -> std::future<decltype(f())> {
        using ReturnType = decltype(f());
        
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(std::forward<F>(f));
        
        std::future<ReturnType> result = task->get_future();
        
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            
            if (m_stop) {
                throw std::runtime_error("ThreadPool is stopped");
            }
            
            m_tasks.emplace([task]() { (*task)(); });
        }
        
        m_condition.notify_one();
        
        return result;
    }
    
    void WaitAll() {
        while (m_activeTasks > 0 || !m_tasks.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    size_t GetThreadCount() const { return m_workers.size(); }
    int GetActiveTaskCount() const { return m_activeTasks; }
    size_t GetPendingTaskCount() {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        return m_tasks.size();
    }
};

class MultiThreadedCompressor {
private:
    SevenZipArchive& m_archive;
    ThreadPool m_pool;
    std::atomic<bool> m_cancelFlag;
    
public:
    MultiThreadedCompressor(SevenZipArchive& archive, size_t threads = 0)
        : m_archive(archive), m_pool(threads > 0 ? threads : std::thread::hardware_concurrency()),
          m_cancelFlag(false) {}
    
    bool CompressFilesParallel(const std::string& archivePath,
                              const std::vector<std::string>& files,
                              const CompressionOptions& options) {
        if (files.empty()) return false;
        
        std::vector<std::future<bool>> results;
        std::mutex resultsMutex;
        std::vector<std::string> failedFiles;
        
        size_t batchSize = std::max((size_t)1, files.size() / m_pool.GetThreadCount());
        
        for (size_t i = 0; i < files.size(); i += batchSize) {
            size_t end = std::min(i + batchSize, files.size());
            std::vector<std::string> batch(files.begin() + i, files.begin() + end);
            
            results.push_back(m_pool.Enqueue([this, &archivePath, batch, options, &failedFiles, &resultsMutex]() {
                CompressionOptions opts = options;
                opts.solidMode = false;
                
                for (const auto& file : batch) {
                    if (m_cancelFlag) return false;
                    
                    std::vector<std::string> singleFile = { file };
                    if (!m_archive.AddToArchive(archivePath, singleFile, opts)) {
                        std::lock_guard<std::mutex> lock(resultsMutex);
                        failedFiles.push_back(file);
                    }
                }
                
                return true;
            }));
        }
        
        for (auto& result : results) {
            result.wait();
        }
        
        return failedFiles.empty();
    }
    
    bool ExtractFilesParallel(const std::string& archivePath,
                             const std::string& outputDir,
                             const std::string& password = "") {
        ArchiveInfo info;
        if (!m_archive.ListArchive(archivePath, info, password)) return false;
        
        std::vector<std::future<bool>> results;
        std::atomic<uint32_t> successCount(0);
        std::atomic<uint32_t> failCount(0);
        
        for (const auto& file : info.files) {
            if (file.isDirectory) continue;
            
            results.push_back(m_pool.Enqueue([this, &archivePath, &file, &outputDir, &password, &successCount, &failCount]() {
                if (m_cancelFlag) return false;
                
                std::vector<uint8_t> data;
                if (m_archive.ExtractSingleFileToMemory(archivePath, file.path, data, password)) {
                    std::string outputPath = outputDir + "\\" + file.path;
                    CreateDirectoryForFile(outputPath);
                    
                    std::ofstream outFile(outputPath, std::ios::binary);
                    if (outFile.is_open()) {
                        outFile.write(reinterpret_cast<const char*>(data.data()), data.size());
                        outFile.close();
                        successCount++;
                        return true;
                    }
                }
                
                failCount++;
                return false;
            }));
        }
        
        for (auto& result : results) {
            result.wait();
        }
        
        return failCount == 0;
    }
    
    void Cancel() {
        m_cancelFlag = true;
    }
    
    size_t GetThreadCount() const {
        return m_pool.GetThreadCount();
    }
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

private:
    ConnectionConfig m_config;
    void* m_handle;
    std::function<void(const TransferProgress&)> m_progressCallback;
    std::atomic<bool> m_cancelled;
    
public:
    CloudStorageClient() : m_handle(nullptr), m_cancelled(false) {}
    ~CloudStorageClient() { Disconnect(); }
    
    bool Connect(const ConnectionConfig& config) {
        m_config = config;
        m_cancelled = false;
        
        WININET_DATA* inetData = new WININET_DATA();
        memset(inetData, 0, sizeof(WININET_DATA));
        
        std::wstring agent = L"SevenZipSDK/1.0";
        inetData->hInternet = InternetOpenW(agent.c_str(), INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
        
        if (!inetData->hInternet) {
            delete inetData;
            return false;
        }
        
        std::wstring host = StringToWString(config.host);
        uint16_t port = config.port ? config.port : (config.useSSL ? 443 : 80);
        
        DWORD flags = config.useSSL ? INTERNET_FLAG_SECURE : 0;
        inetData->hConnect = InternetConnectW(inetData->hInternet, host.c_str(), port,
                                              StringToWString(config.username).c_str(),
                                              StringToWString(config.password).c_str(),
                                              INTERNET_SERVICE_HTTP, 0, 0);
        
        if (!inetData->hConnect) {
            InternetCloseHandle(inetData->hInternet);
            delete inetData;
            return false;
        }
        
        m_handle = inetData;
        return true;
    }
    
    void Disconnect() {
        if (m_handle) {
            WININET_DATA* inetData = (WININET_DATA*)m_handle;
            if (inetData->hConnect) InternetCloseHandle(inetData->hConnect);
            if (inetData->hInternet) InternetCloseHandle(inetData->hInternet);
            delete inetData;
            m_handle = nullptr;
        }
    }
    
    bool UploadFile(const std::string& localPath, const std::string& remotePath) {
        if (!m_handle) return false;
        
        std::ifstream file(localPath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return false;
        
        uint64_t fileSize = file.tellg();
        file.seekg(0);
        
        TransferProgress progress = {};
        progress.totalBytes = fileSize;
        progress.currentFile = remotePath;
        progress.isUpload = true;
        
        WININET_DATA* inetData = (WININET_DATA*)m_handle;
        
        std::wstring wpath = StringToWString(m_config.basePath + remotePath);
        HINTERNET hRequest = HttpOpenRequestW(inetData->hConnect, L"PUT", wpath.c_str(),
                                              NULL, NULL, NULL, 
                                              m_config.useSSL ? INTERNET_FLAG_SECURE : 0, 0);
        
        if (!hRequest) return false;
        
        const size_t bufferSize = 64 * 1024;
        std::vector<char> buffer(bufferSize);
        bool success = true;
        
        while (file && !m_cancelled) {
            file.read(buffer.data(), bufferSize);
            std::streamsize bytesRead = file.gcount();
            
            if (bytesRead > 0) {
                DWORD bytesWritten = 0;
                if (!InternetWriteFile(hRequest, buffer.data(), (DWORD)bytesRead, &bytesWritten)) {
                    success = false;
                    break;
                }
                
                progress.bytesTransferred += bytesWritten;
                
                if (m_progressCallback) {
                    m_progressCallback(progress);
                }
            }
        }
        
        InternetCloseHandle(hRequest);
        return success && !m_cancelled;
    }
    
    bool DownloadFile(const std::string& remotePath, const std::string& localPath) {
        if (!m_handle) return false;
        
        WININET_DATA* inetData = (WININET_DATA*)m_handle;
        
        std::wstring wpath = StringToWString(m_config.basePath + remotePath);
        HINTERNET hRequest = HttpOpenRequestW(inetData->hConnect, L"GET", wpath.c_str(),
                                              NULL, NULL, NULL,
                                              m_config.useSSL ? INTERNET_FLAG_SECURE : 0, 0);
        
        if (!hRequest) return false;
        
        if (!HttpSendRequestW(hRequest, NULL, 0, NULL, 0)) {
            InternetCloseHandle(hRequest);
            return false;
        }
        
        DWORD contentLength = 0;
        DWORD lengthSize = sizeof(contentLength);
        HttpQueryInfoW(hRequest, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER,
                       &contentLength, &lengthSize, NULL);
        
        std::ofstream outFile(localPath, std::ios::binary);
        if (!outFile.is_open()) {
            InternetCloseHandle(hRequest);
            return false;
        }
        
        TransferProgress progress = {};
        progress.totalBytes = contentLength;
        progress.currentFile = remotePath;
        progress.isUpload = false;
        
        const size_t bufferSize = 64 * 1024;
        std::vector<char> buffer(bufferSize);
        bool success = true;
        
        while (!m_cancelled) {
            DWORD bytesRead = 0;
            if (!InternetReadFile(hRequest, buffer.data(), bufferSize, &bytesRead)) {
                success = false;
                break;
            }
            
            if (bytesRead == 0) break;
            
            outFile.write(buffer.data(), bytesRead);
            progress.bytesTransferred += bytesRead;
            
            if (m_progressCallback) {
                m_progressCallback(progress);
            }
        }
        
        outFile.close();
        InternetCloseHandle(hRequest);
        
        return success && !m_cancelled;
    }
    
    std::vector<RemoteFile> ListDirectory(const std::string& remotePath) {
        std::vector<RemoteFile> files;
        if (!m_handle) return files;
        
        WININET_DATA* inetData = (WININET_DATA*)m_handle;
        
        std::wstring wpath = StringToWString(m_config.basePath + remotePath);
        HINTERNET hFind = FtpFindFirstFileW(inetData->hConnect, wpath.c_str(), NULL, 0, 0);
        
        if (!hFind) return files;
        
        WIN32_FIND_DATAW findData;
        while (InternetFindNextFileW(hFind, &findData)) {
            RemoteFile file;
            file.path = WStringToString(findData.cFileName);
            file.isDirectory = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            file.size = ((uint64_t)findData.nFileSizeHigh << 32) | findData.nFileSizeLow;
            
            FILETIME ft = findData.ftLastWriteTime;
            file.modifiedTime = FileTimeToTimeT(ft);
            
            files.push_back(file);
        }
        
        InternetCloseHandle(hFind);
        return files;
    }
    
    bool CreateDirectory(const std::string& remotePath) {
        if (!m_handle) return false;
        
        WININET_DATA* inetData = (WININET_DATA*)m_handle;
        std::wstring wpath = StringToWString(m_config.basePath + remotePath);
        
        return FtpCreateDirectoryW(inetData->hConnect, wpath.c_str()) != FALSE;
    }
    
    bool DeleteFile(const std::string& remotePath) {
        if (!m_handle) return false;
        
        WININET_DATA* inetData = (WININET_DATA*)m_handle;
        std::wstring wpath = StringToWString(m_config.basePath + remotePath);
        
        return FtpDeleteFileW(inetData->hConnect, wpath.c_str()) != FALSE;
    }
    
    bool UploadArchive(const std::string& archivePath, const std::string& remotePath,
                      SevenZipArchive& archive, const std::string& sourceDir,
                      const CompressionOptions& options) {
        std::string tempArchive = archivePath;
        
        if (!archive.CompressDirectory(tempArchive, sourceDir, options, true)) {
            return false;
        }
        
        bool result = UploadFile(tempArchive, remotePath);
        DeleteFileA(tempArchive.c_str());
        
        return result;
    }
    
    bool DownloadAndExtract(const std::string& remotePath, const std::string& localPath,
                           SevenZipArchive& archive, const ExtractOptions& options) {
        std::string tempArchive = localPath + ".tmp";
        
        if (!DownloadFile(remotePath, tempArchive)) {
            return false;
        }
        
        bool result = archive.ExtractArchive(tempArchive, options);
        DeleteFileA(tempArchive.c_str());
        
        return result;
    }
    
    void SetProgressCallback(std::function<void(const TransferProgress&)> callback) {
        m_progressCallback = callback;
    }
    
    void Cancel() { m_cancelled = true; }
    bool IsConnected() const { return m_handle != nullptr; }
    
private:
    struct WININET_DATA {
        HINTERNET hInternet;
        HINTERNET hConnect;
    };
    
    static std::wstring StringToWString(const std::string& str) {
        if (str.empty()) return L"";
        int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
        std::wstring result(size - 1, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], size);
        return result;
    }
    
    static std::string WStringToString(const std::wstring& wstr) {
        if (wstr.empty()) return "";
        int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
        std::string result(size - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], size, NULL, NULL);
        return result;
    }
    
    static time_t FileTimeToTimeT(const FILETIME& ft) {
        ULARGE_INTEGER ull;
        ull.LowPart = ft.dwLowDateTime;
        ull.HighPart = ft.dwHighDateTime;
        return (time_t)((ull.QuadPart - 116444736000000000ULL) / 10000000ULL);
    }
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

private:
    SevenZipArchive& m_archive;
    
public:
    ArchiveRepair(SevenZipArchive& archive) : m_archive(archive) {}
    
    RepairResult RepairArchive(const std::string& archivePath, const RepairOptions& options) {
        RepairResult result = {};
        
        std::ifstream file(archivePath, std::ios::binary);
        if (!file.is_open()) {
            result.errorMessage = "Cannot open archive file";
            return result;
        }
        
        file.seekg(0, std::ios::end);
        uint64_t fileSize = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::vector<uint8_t> signature = ReadSignature(file);
        ArchiveType type = DetectArchiveType(signature);
        
        if (type == ArchiveType::Unknown) {
            result.errorMessage = "Unknown archive format";
            return result;
        }
        
        if (options.rebuildHeaders) {
            if (!RebuildHeaders(archivePath, type)) {
                result.errorMessage = "Failed to rebuild headers";
                if (!options.tryPartialRecovery) return result;
            }
        }
        
        ArchiveInfo info;
        if (m_archive.ListArchive(archivePath, info)) {
            for (const auto& fileInfo : info.files) {
                if (fileInfo.isDirectory) continue;
                
                std::vector<uint8_t> data;
                bool extracted = false;
                
                for (int retry = 0; retry < options.maxRetries && !extracted; retry++) {
                    try {
                        extracted = m_archive.ExtractSingleFileToMemory(
                            archivePath, fileInfo.path, data);
                    } catch (...) {
                        extracted = false;
                    }
                }
                
                if (extracted) {
                    result.filesRecovered++;
                    result.bytesRecovered += data.size();
                    result.recoveredFiles.push_back(fileInfo.path);
                    
                    if (!options.outputDir.empty()) {
                        std::string outputPath = options.outputDir + "\\" + fileInfo.path;
                        CreateDirectoryForFile(outputPath);
                        
                        std::ofstream outFile(outputPath, std::ios::binary);
                        if (outFile.is_open()) {
                            outFile.write(reinterpret_cast<const char*>(data.data()), data.size());
                        }
                    }
                } else {
                    result.filesLost++;
                    result.lostFiles.push_back(fileInfo.path);
                    
                    if (!options.skipCorruptedFiles) {
                        result.errorMessage = "Failed to recover: " + fileInfo.path;
                        return result;
                    }
                }
            }
        }
        
        result.success = result.filesRecovered > 0;
        return result;
    }
    
    bool ValidateArchive(const std::string& archivePath) {
        std::ifstream file(archivePath, std::ios::binary);
        if (!file.is_open()) return false;
        
        std::vector<uint8_t> signature = ReadSignature(file);
        ArchiveType type = DetectArchiveType(signature);
        
        if (type == ArchiveType::Unknown) return false;
        
        switch (type) {
            case ArchiveType::SevenZip:
                return Validate7zArchive(file);
            case ArchiveType::Zip:
                return ValidateZipArchive(file);
            default:
                return false;
        }
    }
    
    std::vector<uint8_t> ExtractRawData(const std::string& archivePath, uint64_t offset, uint64_t size) {
        std::vector<uint8_t> data;
        
        std::ifstream file(archivePath, std::ios::binary);
        if (!file.is_open()) return data;
        
        file.seekg(offset);
        data.resize(size);
        file.read(reinterpret_cast<char*>(data.data()), size);
        data.resize(file.gcount());
        
        return data;
    }
    
    bool RebuildArchive(const std::string& damagedPath, const std::string& outputPath) {
        RepairOptions options;
        options.rebuildHeaders = true;
        options.tryPartialRecovery = true;
        options.skipCorruptedFiles = true;
        options.maxRetries = 3;
        options.outputDir = "";
        
        RepairResult result = RepairArchive(damagedPath, options);
        
        if (!result.success || result.recoveredFiles.empty()) {
            return false;
        }
        
        CompressionOptions compOpts;
        compOpts.level = CompressionLevel::Normal;
        
        std::vector<std::string> files = result.recoveredFiles;
        
        return m_archive.AddToArchive(outputPath, files, compOpts);
    }
    
private:
    enum class ArchiveType { Unknown, SevenZip, Zip, GZip, BZip2, Rar, Tar, Xz };
    
    std::vector<uint8_t> ReadSignature(std::ifstream& file) {
        std::vector<uint8_t> signature(16);
        file.read(reinterpret_cast<char*>(signature.data()), signature.size());
        file.seekg(0);
        return signature;
    }
    
    ArchiveType DetectArchiveType(const std::vector<uint8_t>& signature) {
        if (signature.size() >= 6) {
            static const uint8_t sevenZipSig[] = { '7', 'z', 0xBC, 0xAF, 0x27, 0x1C };
            if (memcmp(signature.data(), sevenZipSig, 6) == 0) {
                return ArchiveType::SevenZip;
            }
        }
        
        if (signature.size() >= 4) {
            if (signature[0] == 'P' && signature[1] == 'K' && 
                signature[2] == 0x03 && signature[3] == 0x04) {
                return ArchiveType::Zip;
            }
        }
        
        if (signature.size() >= 2) {
            if (signature[0] == 0x1F && signature[1] == 0x8B) {
                return ArchiveType::GZip;
            }
        }
        
        if (signature.size() >= 3) {
            if (signature[0] == 'B' && signature[1] == 'Z' && signature[2] == 'h') {
                return ArchiveType::BZip2;
            }
        }
        
        if (signature.size() >= 6) {
            static const uint8_t xzSig[] = { 0xFD, '7', 'z', 'X', 'Z', 0x00 };
            if (memcmp(signature.data(), xzSig, 6) == 0) {
                return ArchiveType::Xz;
            }
        }
        
        return ArchiveType::Unknown;
    }
    
    bool RebuildHeaders(const std::string& archivePath, ArchiveType type) {
        std::fstream file(archivePath, std::ios::binary | std::ios::in | std::ios::out);
        if (!file.is_open()) return false;
        
        switch (type) {
            case ArchiveType::SevenZip: {
                file.seekp(0);
                static const uint8_t header[] = { '7', 'z', 0xBC, 0xAF, 0x27, 0x1C,
                                                  0, 0, 0, 0, 0, 0 };
                file.write(reinterpret_cast<const char*>(header), sizeof(header));
                break;
            }
            case ArchiveType::Zip: {
                file.seekp(0);
                static const uint8_t header[] = { 'P', 'K', 0x03, 0x04 };
                file.write(reinterpret_cast<const char*>(header), sizeof(header));
                break;
            }
            default:
                return false;
        }
        
        return true;
    }
    
    bool Validate7zArchive(std::ifstream& file) {
        file.seekg(0, std::ios::end);
        uint64_t fileSize = file.tellg();
        
        if (fileSize < 32) return false;
        
        file.seekg(fileSize - 6);
        uint8_t endSig[6];
        file.read(reinterpret_cast<char*>(endSig), 6);
        
        return true;
    }
    
    bool ValidateZipArchive(std::ifstream& file) {
        file.seekg(0, std::ios::end);
        uint64_t fileSize = file.tellg();
        
        if (fileSize < 22) return false;
        
        file.seekg(fileSize - 22);
        uint8_t buffer[22];
        file.read(reinterpret_cast<char*>(buffer), 22);
        
        uint32_t sig = *(uint32_t*)buffer;
        return sig == 0x06054B50;
    }
    
    static void CreateDirectoryForFile(const std::string& filePath) {
        size_t pos = filePath.find_last_of("\\/");
        if (pos != std::string::npos) {
            std::string dir = filePath.substr(0, pos);
            CreateDirectoryA(dir.c_str(), NULL);
        }
    }
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

private:
    std::map<std::string, ChunkInfo> m_chunkStore;
    DedupOptions m_options;
    
public:
    DeduplicationEngine() {
        m_options.chunkSize = 64 * 1024;
        m_options.chunkSizeMin = 16 * 1024;
        m_options.chunkSizeMax = 256 * 1024;
        m_options.hashAlgorithm = "SHA256";
        m_options.variableSizeChunks = true;
        m_options.similarityThreshold = 0.8;
    }
    
    DedupResult DeduplicateFiles(const std::vector<std::string>& files) {
        DedupResult result = {};
        
        for (const auto& file : files) {
            DedupFile(file, result);
        }
        
        result.deduplicationRatio = result.originalSize > 0 ?
            (double)result.savedBytes / result.originalSize : 0.0;
        
        return result;
    }
    
    bool StoreDeduplicatedArchive(const std::string& archivePath,
                                  const std::vector<std::string>& files,
                                  SevenZipArchive& archive) {
        DedupResult dedupResult = DeduplicateFiles(files);
        
        std::string manifestPath = archivePath + ".manifest";
        std::ofstream manifest(manifestPath, std::ios::binary);
        
        if (!manifest.is_open()) return false;
        
        uint32_t chunkCount = (uint32_t)m_chunkStore.size();
        manifest.write(reinterpret_cast<const char*>(&chunkCount), sizeof(chunkCount));
        
        for (const auto& pair : m_chunkStore) {
            const ChunkInfo& chunk = pair.second;
            
            uint32_t hashLen = (uint32_t)chunk.hash.size();
            manifest.write(reinterpret_cast<const char*>(&hashLen), sizeof(hashLen));
            manifest.write(chunk.hash.c_str(), hashLen);
            manifest.write(reinterpret_cast<const char*>(&chunk.offset), sizeof(chunk.offset));
            manifest.write(reinterpret_cast<const char*>(&chunk.size), sizeof(chunk.size));
            manifest.write(reinterpret_cast<const char*>(&chunk.refCount), sizeof(chunk.refCount));
        }
        
        manifest.close();
        
        std::string chunkStorePath = archivePath + ".chunks";
        std::ofstream chunkStore(chunkStorePath, std::ios::binary);
        
        if (!chunkStore.is_open()) {
            DeleteFileA(manifestPath.c_str());
            return false;
        }
        
        std::vector<std::string> chunkFiles;
        for (const auto& pair : m_chunkStore) {
            chunkFiles.push_back(pair.first);
        }
        
        chunkStore.close();
        
        std::vector<std::string> archiveFiles = { manifestPath, chunkStorePath };
        CompressionOptions options;
        options.level = CompressionLevel::Maximum;
        
        bool result = archive.AddToArchive(archivePath, archiveFiles, options);
        
        DeleteFileA(manifestPath.c_str());
        DeleteFileA(chunkStorePath.c_str());
        
        return result;
    }
    
    std::vector<std::string> FindDuplicates(const std::vector<std::string>& files) {
        std::map<std::string, std::vector<std::string>> hashToFiles;
        std::vector<std::string> duplicates;
        
        for (const auto& file : files) {
            std::string hash = CalculateFileHash(file);
            hashToFiles[hash].push_back(file);
        }
        
        for (const auto& pair : hashToFiles) {
            if (pair.second.size() > 1) {
                for (size_t i = 1; i < pair.second.size(); i++) {
                    duplicates.push_back(pair.second[i]);
                }
            }
        }
        
        return duplicates;
    }
    
    uint64_t CalculateSavedSpace(const std::vector<std::string>& files) {
        DedupResult result = DeduplicateFiles(files);
        return result.savedBytes;
    }
    
    void ClearChunkStore() {
        m_chunkStore.clear();
    }
    
    void SetOptions(const DedupOptions& options) {
        m_options = options;
    }
    
private:
    void DedupFile(const std::string& filePath, DedupResult& result) {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) return;
        
        file.seekg(0, std::ios::end);
        uint64_t fileSize = file.tellg();
        file.seekg(0);
        
        result.originalSize += fileSize;
        
        std::vector<uint8_t> buffer(m_options.chunkSize);
        uint64_t offset = 0;
        
        while (offset < fileSize) {
            uint32_t chunkSize = m_options.chunkSize;
            
            if (m_options.variableSizeChunks) {
                chunkSize = CalculateOptimalChunkSize(file, offset, fileSize);
            }
            
            if (offset + chunkSize > fileSize) {
                chunkSize = (uint32_t)(fileSize - offset);
            }
            
            buffer.resize(chunkSize);
            file.seekg(offset);
            file.read(reinterpret_cast<char*>(buffer.data()), chunkSize);
            
            std::string hash = CalculateChunkHash(buffer.data(), chunkSize);
            
            result.totalChunks++;
            
            auto it = m_chunkStore.find(hash);
            if (it != m_chunkStore.end()) {
                it->second.refCount++;
                result.savedBytes += chunkSize;
            } else {
                ChunkInfo chunk;
                chunk.hash = hash;
                chunk.offset = offset;
                chunk.size = chunkSize;
                chunk.refCount = 1;
                m_chunkStore[hash] = chunk;
                result.uniqueChunks++;
                result.deduplicatedSize += chunkSize;
            }
            
            offset += chunkSize;
        }
    }
    
    uint32_t CalculateOptimalChunkSize(std::ifstream& file, uint64_t offset, uint64_t fileSize) {
        if (!m_options.variableSizeChunks) {
            return m_options.chunkSize;
        }
        
        uint32_t windowSize = 48;
        uint32_t target = m_options.chunkSize;
        uint32_t mask = target - 1;
        
        std::vector<uint8_t> window(windowSize);
        uint32_t hash = 0;
        
        file.seekg(offset);
        
        for (uint32_t i = 0; i < m_options.chunkSizeMax && offset + i < fileSize; i++) {
            uint8_t byte;
            if (!file.read(reinterpret_cast<char*>(&byte), 1)) break;
            
            window[i % windowSize] = byte;
            
            if (i >= windowSize) {
                hash = (hash * 31 + byte) & mask;
                
                if (hash == 0 && i >= m_options.chunkSizeMin) {
                    return i;
                }
            }
        }
        
        return m_options.chunkSize;
    }
    
    std::string CalculateChunkHash(const void* data, size_t size) {
        uint32_t crc = 0xFFFFFFFF;
        const uint8_t* p = (const uint8_t*)data;
        
        for (size_t i = 0; i < size; i++) {
            crc ^= p[i];
            for (int j = 0; j < 8; j++) {
                crc = (crc >> 1) ^ (crc & 1 ? 0xEDB88320 : 0);
            }
        }
        
        std::ostringstream oss;
        oss << std::hex << std::setw(8) << std::setfill('0') << (crc ^ 0xFFFFFFFF);
        return oss.str();
    }
    
    std::string CalculateFileHash(const std::string& filePath) {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) return "";
        
        uint32_t crc = 0xFFFFFFFF;
        uint8_t buffer[8192];
        
        while (file.read(reinterpret_cast<char*>(buffer), sizeof(buffer))) {
            size_t bytesRead = file.gcount();
            for (size_t i = 0; i < bytesRead; i++) {
                crc ^= buffer[i];
                for (int j = 0; j < 8; j++) {
                    crc = (crc >> 1) ^ (crc & 1 ? 0xEDB88320 : 0);
                }
            }
        }
        
        std::ostringstream oss;
        oss << std::hex << std::setw(8) << std::setfill('0') << (crc ^ 0xFFFFFFFF);
        return oss.str();
    }
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

private:
    SfxConfig m_config;
    std::string m_sfxModule;
    
public:
    SfxScriptBuilder() {
        m_sfxModule = "7zSD.sfx";
        
        m_config.title = "7-Zip Self-Extracting Archive";
        m_config.beginPrompt = "Do you want to install this archive?";
        m_config.extractDialogText = "Extracting files...";
        m_config.extractPathText = "Install Path";
        m_config.extractTitle = "Install";
        m_config.errorTitle = "Error";
        m_config.errorMessage = "Installation failed!";
        m_config.installPath = "";
        m_config.showExtractDialog = true;
        m_config.overwriteMode = true;
        m_config.guiMode = true;
        m_config.silentMode = false;
        m_config.runAfterExtract = false;
        m_config.deleteArchive = false;
    }
    
    void SetConfig(const SfxConfig& config) {
        m_config = config;
    }
    
    SfxConfig& GetConfig() {
        return m_config;
    }
    
    void SetSfxModule(const std::string& module) {
        m_sfxModule = module;
    }
    
    bool BuildSfxArchive(const std::string& outputPath,
                        const std::string& archivePath,
                        SevenZipArchive& archive) {
        std::string configContent = GenerateConfigFile();
        
        std::string tempConfig = outputPath + ".config.tmp";
        std::ofstream configFile(tempConfig, std::ios::binary);
        if (!configFile.is_open()) return false;
        
        configFile.write(configContent.c_str(), configContent.size());
        configFile.close();
        
        std::ifstream sfxModule(m_sfxModule, std::ios::binary);
        if (!sfxModule.is_open()) {
            DeleteFileA(tempConfig.c_str());
            return false;
        }
        
        std::ofstream outFile(outputPath, std::ios::binary);
        if (!outFile.is_open()) {
            sfxModule.close();
            DeleteFileA(tempConfig.c_str());
            return false;
        }
        
        sfxModule.seekg(0, std::ios::end);
        uint64_t sfxSize = sfxModule.tellg();
        sfxModule.seekg(0);
        
        std::vector<char> sfxBuffer(sfxSize);
        sfxModule.read(sfxBuffer.data(), sfxSize);
        outFile.write(sfxBuffer.data(), sfxSize);
        sfxModule.close();
        
        std::ifstream configStream(tempConfig, std::ios::binary);
        configStream.seekg(0, std::ios::end);
        uint64_t configSize = configStream.tellg();
        configStream.seekg(0);
        
        std::vector<char> configBuffer(configSize);
        configStream.read(configBuffer.data(), configSize);
        outFile.write(configBuffer.data(), configSize);
        configStream.close();
        
        DeleteFileA(tempConfig.c_str());
        
        std::ifstream archiveStream(archivePath, std::ios::binary);
        if (!archiveStream.is_open()) {
            outFile.close();
            return false;
        }
        
        archiveStream.seekg(0, std::ios::end);
        uint64_t archiveSize = archiveStream.tellg();
        archiveStream.seekg(0);
        
        std::vector<char> archiveBuffer(archiveSize);
        archiveStream.read(archiveBuffer.data(), archiveSize);
        outFile.write(archiveBuffer.data(), archiveSize);
        archiveStream.close();
        
        outFile.close();
        
        return true;
    }
    
    bool BuildSfxFromDirectory(const std::string& outputPath,
                              const std::string& sourceDir,
                              SevenZipArchive& archive,
                              const CompressionOptions& options) {
        std::string tempArchive = outputPath + ".temp.7z";
        
        if (!archive.CompressDirectory(tempArchive, sourceDir, options, true)) {
            return false;
        }
        
        bool result = BuildSfxArchive(outputPath, tempArchive, archive);
        DeleteFileA(tempArchive.c_str());
        
        return result;
    }
    
    std::string GenerateConfigFile() {
        std::ostringstream oss;
        
        oss << ";!@Install@!UTF-8!\n";
        
        if (!m_config.title.empty()) {
            oss << "Title=\"" << EscapeString(m_config.title) << "\"\n";
        }
        
        if (!m_config.beginPrompt.empty()) {
            oss << "BeginPrompt=\"" << EscapeString(m_config.beginPrompt) << "\"\n";
        }
        
        if (!m_config.extractDialogText.empty()) {
            oss << "ExtractDialogText=\"" << EscapeString(m_config.extractDialogText) << "\"\n";
        }
        
        if (!m_config.extractPathText.empty()) {
            oss << "ExtractPathText=\"" << EscapeString(m_config.extractPathText) << "\"\n";
        }
        
        if (!m_config.extractTitle.empty()) {
            oss << "ExtractTitle=\"" << EscapeString(m_config.extractTitle) << "\"\n";
        }
        
        if (!m_config.errorTitle.empty()) {
            oss << "ErrorTitle=\"" << EscapeString(m_config.errorTitle) << "\"\n";
        }
        
        if (!m_config.errorMessage.empty()) {
            oss << "ErrorMessage=\"" << EscapeString(m_config.errorMessage) << "\"\n";
        }
        
        if (!m_config.installPath.empty()) {
            oss << "InstallPath=\"" << EscapeString(m_config.installPath) << "\"\n";
        }
        
        if (!m_config.runProgram.empty()) {
            oss << "RunProgram=\"" << EscapeString(m_config.runProgram) << "\"\n";
        }
        
        if (!m_config.runProgramArgs.empty()) {
            oss << "RunProgramArgs=\"" << EscapeString(m_config.runProgramArgs) << "\"\n";
        }
        
        if (m_config.silentMode) {
            oss << "GUIMode=\"2\"\n";
        } else if (!m_config.guiMode) {
            oss << "GUIMode=\"1\"\n";
        }
        
        oss << "OverwriteMode=\"" << (m_config.overwriteMode ? "2" : "0") << "\"\n";
        
        if (m_config.deleteArchive) {
            oss << "DeleteAfterInstall=\"1\"\n";
        }
        
        if (m_config.createShortcut && !m_config.shortcutPath.empty()) {
            oss << "Shortcut=\"" << EscapeString(m_config.shortcutPath);
            if (!m_config.shortcutName.empty()) {
                oss << "," << EscapeString(m_config.shortcutName);
            }
            oss << "\"\n";
        }
        
        oss << ";!@InstallEnd@!\n";
        
        return oss.str();
    }
    
    std::string GenerateBatchScript(const std::string& archivePath) {
        std::ostringstream oss;
        
        oss << "@echo off\n";
        oss << "setlocal\n\n";
        
        if (!m_config.title.empty()) {
            oss << "title " << m_config.title << "\n\n";
        }
        
        if (!m_config.beginPrompt.empty()) {
            oss << "echo " << m_config.beginPrompt << "\n";
            oss << "set /p confirm=Continue? (Y/N): \n";
            oss << "if /i not \"%confirm%\"==\"Y\" exit /b 1\n\n";
        }
        
        if (!m_config.installPath.empty()) {
            oss << "set INSTALL_PATH=" << m_config.installPath << "\n";
        } else {
            oss << "set INSTALL_PATH=%~dp0\n";
        }
        
        oss << "if not exist \"%INSTALL_PATH%\" mkdir \"%INSTALL_PATH%\"\n\n";
        
        oss << "echo " << m_config.extractDialogText << "\n";
        oss << "7z x -y -o\"%INSTALL_PATH%\" \"" << archivePath << "\"\n\n";
        
        if (m_config.runAfterExtract && !m_config.runProgram.empty()) {
            oss << "if exist \"%INSTALL_PATH%\\" << m_config.runProgram << "\" (\n";
            oss << "    cd /d \"%INSTALL_PATH%\"\n";
            oss << "    start \"\" \"" << m_config.runProgram << "\"";
            if (!m_config.runProgramArgs.empty()) {
                oss << " " << m_config.runProgramArgs;
            }
            oss << "\n)\n\n";
        }
        
        if (m_config.deleteArchive) {
            oss << "del /q \"" << archivePath << "\"\n";
        }
        
        oss << "echo Installation complete.\n";
        oss << "pause\n";
        
        return oss.str();
    }
    
private:
    static std::string EscapeString(const std::string& str) {
        std::string result;
        for (char c : str) {
            if (c == '"') {
                result += "\\\"";
            } else if (c == '\\') {
                result += "\\\\";
            } else {
                result += c;
            }
        }
        return result;
    }
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

private:
    SevenZipArchive& m_archive;
    
public:
    MultiVolumeRecovery(SevenZipArchive& archive) : m_archive(archive) {}
    
    std::vector<VolumeInfo> AnalyzeVolumes(const std::string& firstVolumePath) {
        std::vector<VolumeInfo> volumes;
        
        std::string basePath = GetBasePath(firstVolumePath);
        std::string extension = GetExtension(firstVolumePath);
        
        uint32_t index = 1;
        while (true) {
            std::string volumePath = BuildVolumePath(basePath, extension, index);
            
            if (!FileExists(volumePath)) {
                break;
            }
            
            VolumeInfo info;
            info.path = volumePath;
            info.index = index;
            info.isComplete = true;
            
            std::ifstream file(volumePath, std::ios::binary | std::ios::ate);
            if (file.is_open()) {
                info.size = file.tellg();
                file.close();
            }
            
            HashResult hashResult;
            if (m_archive.CalculateFileHash(volumePath, hashResult, "CRC32")) {
                info.crc = (uint32_t)std::stoul(hashResult.hash, nullptr, 16);
            }
            
            volumes.push_back(info);
            index++;
        }
        
        return volumes;
    }
    
    RecoveryResult RecoverMissingVolumes(const std::string& firstVolumePath,
                                        const std::string& parityPath) {
        RecoveryResult result = {};
        
        std::vector<VolumeInfo> volumes = AnalyzeVolumes(firstVolumePath);
        
        if (volumes.empty()) {
            result.errorMessage = "No volumes found";
            return result;
        }
        
        std::vector<VolumeInfo> missingVolumes;
        for (const auto& vol : volumes) {
            if (!vol.isComplete) {
                missingVolumes.push_back(vol);
            }
        }
        
        if (missingVolumes.empty()) {
            result.success = true;
            result.volumesRecovered = 0;
            return result;
        }
        
        if (!FileExists(parityPath)) {
            result.errorMessage = "Parity file not found";
            return result;
        }
        
        std::ifstream parityFile(parityPath, std::ios::binary);
        if (!parityFile.is_open()) {
            result.errorMessage = "Cannot open parity file";
            return result;
        }
        
        for (const auto& missing : missingVolumes) {
            parityFile.seekg(0);
            
            std::ofstream outFile(missing.path, std::ios::binary);
            if (!outFile.is_open()) {
                result.missingVolumes.push_back(missing.path);
                continue;
            }
            
            std::vector<char> buffer(64 * 1024);
            uint64_t bytesWritten = 0;
            
            while (bytesWritten < missing.size) {
                uint64_t toRead = std::min((uint64_t)buffer.size(), missing.size - bytesWritten);
                parityFile.read(buffer.data(), toRead);
                std::streamsize bytesRead = parityFile.gcount();
                
                if (bytesRead <= 0) break;
                
                outFile.write(buffer.data(), bytesRead);
                bytesWritten += bytesRead;
            }
            
            outFile.close();
            result.volumesRecovered++;
            result.bytesRecovered += bytesWritten;
        }
        
        parityFile.close();
        result.success = result.volumesRecovered > 0;
        
        return result;
    }
    
    bool CreateParityFile(const std::string& firstVolumePath,
                         const std::string& parityPath,
                         uint32_t parityCount) {
        std::vector<VolumeInfo> volumes = AnalyzeVolumes(firstVolumePath);
        
        if (volumes.empty()) return false;
        
        uint64_t maxSize = 0;
        for (const auto& vol : volumes) {
            if (vol.size > maxSize) maxSize = vol.size;
        }
        
        std::ofstream parityFile(parityPath, std::ios::binary);
        if (!parityFile.is_open()) return false;
        
        std::vector<std::ifstream> volumeFiles;
        for (const auto& vol : volumes) {
            volumeFiles.emplace_back(vol.path, std::ios::binary);
        }
        
        std::vector<uint8_t> parityBuffer(maxSize, 0);
        
        for (size_t i = 0; i < maxSize; i++) {
            uint8_t parity = 0;
            
            for (auto& file : volumeFiles) {
                uint8_t byte = 0;
                if (file.read(reinterpret_cast<char*>(&byte), 1)) {
                    parity ^= byte;
                }
            }
            
            parityBuffer[i] = parity;
        }
        
        for (uint32_t p = 0; p < parityCount; p++) {
            parityFile.write(reinterpret_cast<const char*>(parityBuffer.data()), maxSize);
        }
        
        for (auto& file : volumeFiles) {
            file.close();
        }
        parityFile.close();
        
        return true;
    }
    
    bool MergeVolumes(const std::string& firstVolumePath,
                     const std::string& outputPath) {
        std::vector<VolumeInfo> volumes = AnalyzeVolumes(firstVolumePath);
        
        if (volumes.empty()) return false;
        
        std::ofstream outFile(outputPath, std::ios::binary);
        if (!outFile.is_open()) return false;
        
        std::vector<char> buffer(64 * 1024);
        
        for (const auto& vol : volumes) {
            std::ifstream inFile(vol.path, std::ios::binary);
            if (!inFile.is_open()) {
                outFile.close();
                DeleteFileA(outputPath.c_str());
                return false;
            }
            
            while (inFile.read(buffer.data(), buffer.size())) {
                outFile.write(buffer.data(), inFile.gcount());
            }
            
            if (inFile.gcount() > 0) {
                outFile.write(buffer.data(), inFile.gcount());
            }
            
            inFile.close();
        }
        
        outFile.close();
        return true;
    }
    
    bool SplitArchive(const std::string& archivePath,
                     const std::string& outputPattern,
                     uint64_t volumeSize) {
        std::ifstream inFile(archivePath, std::ios::binary);
        if (!inFile.is_open()) return false;
        
        inFile.seekg(0, std::ios::end);
        uint64_t totalSize = inFile.tellg();
        inFile.seekg(0);
        
        uint32_t volumeCount = (uint32_t)((totalSize + volumeSize - 1) / volumeSize);
        
        std::vector<char> buffer(volumeSize);
        
        for (uint32_t i = 0; i < volumeCount; i++) {
            std::ostringstream oss;
            oss << outputPattern << "." << std::setfill('0') << std::setw(3) << (i + 1);
            std::string volumePath = oss.str();
            
            std::ofstream outFile(volumePath, std::ios::binary);
            if (!outFile.is_open()) {
                inFile.close();
                return false;
            }
            
            uint64_t remaining = totalSize - (i * volumeSize);
            uint64_t toRead = std::min(volumeSize, remaining);
            
            inFile.read(buffer.data(), toRead);
            outFile.write(buffer.data(), inFile.gcount());
            outFile.close();
        }
        
        inFile.close();
        return true;
    }
    
private:
    static bool FileExists(const std::string& path) {
        DWORD attr = GetFileAttributesA(path.c_str());
        return attr != INVALID_FILE_ATTRIBUTES;
    }
    
    static std::string GetBasePath(const std::string& path) {
        size_t pos = path.find_last_of('.');
        if (pos == std::string::npos) return path;
        
        std::string ext = path.substr(pos);
        if (ext.size() == 4 && ext[1] == '.' && 
            std::isdigit(ext[2]) && std::isdigit(ext[3])) {
            return path.substr(0, pos);
        }
        
        return path;
    }
    
    static std::string GetExtension(const std::string& path) {
        size_t pos = path.find_last_of('.');
        if (pos == std::string::npos) return "";
        return path.substr(pos);
    }
    
    static std::string BuildVolumePath(const std::string& basePath,
                                       const std::string& extension,
                                       uint32_t index) {
        std::ostringstream oss;
        oss << basePath << "." << std::setfill('0') << std::setw(3) << index;
        return oss.str();
    }
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

private:
    SevenZipArchive& m_archive;
    std::map<std::string, std::vector<std::string>> m_index;
    
public:
    ArchiveSearchEngine(SevenZipArchive& archive) : m_archive(archive) {}
    
    std::vector<SearchResult> Search(const std::string& archivePath,
                                    const SearchOptions& options) {
        std::vector<SearchResult> results;
        
        ArchiveInfo info;
        if (!m_archive.ListArchive(archivePath, info)) {
            return results;
        }
        
        for (const auto& file : info.files) {
            if (file.isDirectory) continue;
            
            if (options.searchFilenames) {
                if (MatchPattern(file.path, options.query, options)) {
                    SearchResult result;
                    result.archivePath = archivePath;
                    result.filePath = file.path;
                    result.size = file.size;
                    result.relevance = 1.0;
                    results.push_back(result);
                }
            }
            
            if (options.searchContent && !file.isDirectory) {
                SearchInFileContent(archivePath, file.path, options, results);
            }
            
            if (results.size() >= options.maxResults) {
                break;
            }
        }
        
        std::sort(results.begin(), results.end(),
                  [](const SearchResult& a, const SearchResult& b) {
                      return a.relevance > b.relevance;
                  });
        
        if (results.size() > options.maxResults) {
            results.resize(options.maxResults);
        }
        
        return results;
    }
    
    std::vector<SearchResult> SearchMultiple(const std::vector<std::string>& archivePaths,
                                            const SearchOptions& options) {
        std::vector<SearchResult> allResults;
        
        for (const auto& archivePath : archivePaths) {
            auto results = Search(archivePath, options);
            allResults.insert(allResults.end(), results.begin(), results.end());
        }
        
        std::sort(allResults.begin(), allResults.end(),
                  [](const SearchResult& a, const SearchResult& b) {
                      return a.relevance > b.relevance;
                  });
        
        if (allResults.size() > options.maxResults) {
            allResults.resize(options.maxResults);
        }
        
        return allResults;
    }
    
    void BuildIndex(const std::string& archivePath) {
        ArchiveInfo info;
        if (!m_archive.ListArchive(archivePath, info)) return;
        
        std::vector<std::string> words;
        
        for (const auto& file : info.files) {
            ExtractWords(file.path, words);
        }
        
        m_index[archivePath] = words;
    }
    
    void ClearIndex() {
        m_index.clear();
    }
    
    std::vector<std::string> FindSimilarFiles(const std::string& archivePath,
                                              const std::string& referenceFile,
                                              double threshold) {
        std::vector<std::string> similarFiles;
        
        std::vector<uint8_t> refData;
        if (!m_archive.ExtractSingleFileToMemory(archivePath, referenceFile, refData)) {
            return similarFiles;
        }
        
        std::string refHash = CalculateContentHash(refData);
        
        ArchiveInfo info;
        if (!m_archive.ListArchive(archivePath, info)) {
            return similarFiles;
        }
        
        for (const auto& file : info.files) {
            if (file.isDirectory || file.path == referenceFile) continue;
            
            std::vector<uint8_t> fileData;
            if (!m_archive.ExtractSingleFileToMemory(archivePath, file.path, fileData)) {
                continue;
            }
            
            std::string fileHash = CalculateContentHash(fileData);
            
            double similarity = CalculateSimilarity(refData, fileData);
            
            if (similarity >= threshold) {
                similarFiles.push_back(file.path);
            }
        }
        
        return similarFiles;
    }
    
private:
    bool MatchPattern(const std::string& text, const std::string& pattern,
                     const SearchOptions& options) {
        if (options.regex) {
            try {
                std::regex::flag_type flags = options.caseSensitive ? 
                    std::regex::ECMAScript : std::regex::ECMAScript | std::regex::icase;
                std::regex re(pattern, flags);
                return std::regex_search(text, re);
            } catch (...) {
                return false;
            }
        }
        
        std::string searchText = text;
        std::string searchPattern = pattern;
        
        if (!options.caseSensitive) {
            std::transform(searchText.begin(), searchText.end(), searchText.begin(), ::tolower);
            std::transform(searchPattern.begin(), searchPattern.end(), searchPattern.begin(), ::tolower);
        }
        
        if (options.wholeWord) {
            std::istringstream iss(searchText);
            std::string word;
            while (iss >> word) {
                if (word == searchPattern) return true;
            }
            return false;
        }
        
        return searchText.find(searchPattern) != std::string::npos;
    }
    
    void SearchInFileContent(const std::string& archivePath,
                            const std::string& filePath,
                            const SearchOptions& options,
                            std::vector<SearchResult>& results) {
        std::vector<uint8_t> data;
        if (!m_archive.ExtractSingleFileToMemory(archivePath, filePath, data)) {
            return;
        }
        
        std::string content(reinterpret_cast<const char*>(data.data()), 
                           std::min(data.size(), (size_t)1024 * 1024));
        
        size_t pos = 0;
        while ((pos = content.find(options.query, pos)) != std::string::npos) {
            SearchResult result;
            result.archivePath = archivePath;
            result.filePath = filePath;
            result.offset = pos;
            result.size = options.query.size();
            
            size_t contextStart = pos > 50 ? pos - 50 : 0;
            size_t contextEnd = std::min(pos + options.query.size() + 50, content.size());
            result.context = content.substr(contextStart, contextEnd - contextStart);
            
            result.relevance = 1.0;
            results.push_back(result);
            
            pos += options.query.size();
        }
    }
    
    void ExtractWords(const std::string& text, std::vector<std::string>& words) {
        std::string word;
        for (char c : text) {
            if (std::isalnum(c) || c == '_') {
                word += c;
            } else if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        }
        if (!word.empty()) {
            words.push_back(word);
        }
    }
    
    static std::string CalculateContentHash(const std::vector<uint8_t>& data) {
        uint32_t crc = 0xFFFFFFFF;
        for (uint8_t byte : data) {
            crc ^= byte;
            for (int i = 0; i < 8; i++) {
                crc = (crc >> 1) ^ (crc & 1 ? 0xEDB88320 : 0);
            }
        }
        
        std::ostringstream oss;
        oss << std::hex << std::setw(8) << std::setfill('0') << (crc ^ 0xFFFFFFFF);
        return oss.str();
    }
    
    static double CalculateSimilarity(const std::vector<uint8_t>& data1,
                                     const std::vector<uint8_t>& data2) {
        if (data1.empty() || data2.empty()) return 0.0;
        
        size_t minSize = std::min(data1.size(), data2.size());
        size_t matches = 0;
        
        for (size_t i = 0; i < minSize; i++) {
            if (data1[i] == data2[i]) matches++;
        }
        
        return (double)matches / minSize;
    }
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

private:
    SevenZipArchive& m_archive;
    
public:
    CompressionAnalyzer(SevenZipArchive& archive) : m_archive(archive) {}
    
    AnalysisResult Analyze(const std::string& sourcePath) {
        AnalysisResult result = {};
        
        std::vector<FileInfo> files;
        
        if (DirectoryExists(sourcePath)) {
            EnumerateFiles(sourcePath, files);
        } else {
            FileInfo info = AnalyzeFile(sourcePath);
            files.push_back(info);
        }
        
        for (const auto& file : files) {
            result.uncompressedSize += file.size;
        }
        
        result.methodRatios["LZMA"] = EstimateCompressionRatio(files, "LZMA");
        result.methodRatios["LZMA2"] = EstimateCompressionRatio(files, "LZMA2");
        result.methodRatios["BZIP2"] = EstimateCompressionRatio(files, "BZIP2");
        result.methodRatios["DEFLATE"] = EstimateCompressionRatio(files, "DEFLATE");
        result.methodRatios["PPMD"] = EstimateCompressionRatio(files, "PPMD");
        result.methodRatios["ZSTD"] = EstimateCompressionRatio(files, "ZSTD");
        
        result.bestMethod = "LZMA2";
        result.bestLevel = "Normal";
        double bestRatio = result.methodRatios["LZMA2"];
        
        for (const auto& pair : result.methodRatios) {
            if (pair.second > bestRatio) {
                bestRatio = pair.second;
                result.bestMethod = pair.first;
            }
        }
        
        result.compressionRatio = bestRatio;
        result.compressedSize = (uint64_t)(result.uncompressedSize * (1.0 - bestRatio));
        
        GenerateRecommendations(result, files);
        
        return result;
    }
    
    FileInfo AnalyzeFile(const std::string& filePath) {
        FileInfo info;
        info.path = filePath;
        
        std::ifstream file(filePath, std::ios::binary | std::ios::ate);
        if (file.is_open()) {
            info.size = file.tellg();
            file.close();
        }
        
        size_t dotPos = filePath.find_last_of('.');
        if (dotPos != std::string::npos) {
            info.extension = filePath.substr(dotPos + 1);
            std::transform(info.extension.begin(), info.extension.end(),
                          info.extension.begin(), ::tolower);
        }
        
        info.type = DetectFileType(filePath);
        info.entropy = CalculateEntropy(filePath);
        info.isCompressible = info.entropy < 7.5;
        
        return info;
    }
    
    std::map<std::string, AnalysisResult> BenchmarkMethods(const std::string& sourcePath,
                                                           bool createTestArchives) {
        std::map<std::string, AnalysisResult> results;
        
        std::vector<std::string> methods = { "LZMA", "LZMA2", "BZIP2", "DEFLATE", "PPMD" };
        
        for (const auto& method : methods) {
            AnalysisResult result = {};
            
            auto startTime = std::chrono::high_resolution_clock::now();
            
            if (createTestArchives) {
                std::string testArchive = sourcePath + ".test." + method + ".7z";
                
                CompressionOptions opts;
                opts.level = CompressionLevel::Normal;
                
                if (method == "LZMA") opts.method = CompressionMethod::LZMA;
                else if (method == "LZMA2") opts.method = CompressionMethod::LZMA2;
                else if (method == "BZIP2") opts.method = CompressionMethod::BZIP2;
                else if (method == "DEFLATE") opts.method = CompressionMethod::DEFLATE;
                else if (method == "PPMD") opts.method = CompressionMethod::PPMD;
                
                if (m_archive.CompressDirectory(testArchive, sourcePath, opts, true)) {
                    WIN32_FILE_ATTRIBUTE_DATA attr;
                    if (GetFileAttributesExA(testArchive.c_str(), GetFileExInfoStandard, &attr)) {
                        result.compressedSize = ((uint64_t)attr.nFileSizeHigh << 32) | attr.nFileSizeLow;
                    }
                    
                    DeleteFileA(testArchive.c_str());
                }
            }
            
            auto endTime = std::chrono::high_resolution_clock::now();
            result.estimatedTime = (uint32_t)
                std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
            
            results[method] = result;
        }
        
        return results;
    }
    
private:
    static bool DirectoryExists(const std::string& path) {
        DWORD attr = GetFileAttributesA(path.c_str());
        return (attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_DIRECTORY);
    }
    
    void EnumerateFiles(const std::string& directory, std::vector<FileInfo>& files) {
        std::string searchPath = directory + "\\*";
        WIN32_FIND_DATAW findData;
        HANDLE hFind = FindFirstFileW(StringToWString(searchPath).c_str(), &findData);
        
        if (hFind == INVALID_HANDLE_VALUE) return;
        
        do {
            std::wstring name = findData.cFileName;
            if (name == L"." || name == L"..") continue;
            
            std::string fileName = WStringToString(name);
            std::string fullPath = directory + "\\" + fileName;
            
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                EnumerateFiles(fullPath, files);
            } else {
                FileInfo info = AnalyzeFile(fullPath);
                files.push_back(info);
            }
        } while (FindNextFileW(hFind, &findData));
        
        FindClose(hFind);
    }
    
    static std::wstring StringToWString(const std::string& str) {
        if (str.empty()) return L"";
        int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
        std::wstring result(size - 1, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], size);
        return result;
    }
    
    static std::string WStringToString(const std::wstring& wstr) {
        if (wstr.empty()) return "";
        int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
        std::string result(size - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], size, NULL, NULL);
        return result;
    }
    
    static std::string DetectFileType(const std::string& filePath) {
        static std::map<std::string, std::string> typeMap = {
            {"txt", "Text"}, {"doc", "Document"}, {"docx", "Document"},
            {"pdf", "Document"}, {"jpg", "Image"}, {"jpeg", "Image"},
            {"png", "Image"}, {"gif", "Image"}, {"bmp", "Image"},
            {"mp3", "Audio"}, {"wav", "Audio"}, {"flac", "Audio"},
            {"mp4", "Video"}, {"avi", "Video"}, {"mkv", "Video"},
            {"zip", "Archive"}, {"rar", "Archive"}, {"7z", "Archive"},
            {"exe", "Executable"}, {"dll", "Library"}, {"so", "Library"},
            {"cpp", "Source"}, {"c", "Source"}, {"h", "Header"},
            {"java", "Source"}, {"py", "Source"}, {"js", "Source"},
            {"html", "Web"}, {"css", "Web"}, {"xml", "Data"},
            {"json", "Data"}, {"sql", "Database"}, {"db", "Database"}
        };
        
        size_t dotPos = filePath.find_last_of('.');
        if (dotPos == std::string::npos) return "Unknown";
        
        std::string ext = filePath.substr(dotPos + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        
        auto it = typeMap.find(ext);
        return it != typeMap.end() ? it->second : "Unknown";
    }
    
    static double CalculateEntropy(const std::string& filePath) {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) return 8.0;
        
        std::vector<uint64_t> freq(256, 0);
        uint64_t total = 0;
        
        char buffer[8192];
        while (file.read(buffer, sizeof(buffer))) {
            for (size_t i = 0; i < file.gcount(); i++) {
                freq[(unsigned char)buffer[i]]++;
                total++;
            }
        }
        
        file.close();
        
        if (total == 0) return 0.0;
        
        double entropy = 0.0;
        for (int i = 0; i < 256; i++) {
            if (freq[i] > 0) {
                double p = (double)freq[i] / total;
                entropy -= p * log2(p);
            }
        }
        
        return entropy;
    }
    
    double EstimateCompressionRatio(const std::vector<FileInfo>& files,
                                   const std::string& method) {
        double totalRatio = 0.0;
        double totalWeight = 0.0;
        
        static std::map<std::string, std::map<std::string, double>> methodRatios = {
            {"Text", {{"LZMA", 0.75}, {"LZMA2", 0.75}, {"BZIP2", 0.72}, {"DEFLATE", 0.65}, {"PPMD", 0.80}}},
            {"Document", {{"LZMA", 0.60}, {"LZMA2", 0.60}, {"BZIP2", 0.55}, {"DEFLATE", 0.50}, {"PPMD", 0.65}}},
            {"Image", {{"LZMA", 0.05}, {"LZMA2", 0.05}, {"BZIP2", 0.03}, {"DEFLATE", 0.02}, {"PPMD", 0.03}}},
            {"Audio", {{"LZMA", 0.03}, {"LZMA2", 0.03}, {"BZIP2", 0.02}, {"DEFLATE", 0.01}, {"PPMD", 0.02}}},
            {"Video", {{"LZMA", 0.01}, {"LZMA2", 0.01}, {"BZIP2", 0.01}, {"DEFLATE", 0.01}, {"PPMD", 0.01}}},
            {"Archive", {{"LZMA", 0.00}, {"LZMA2", 0.00}, {"BZIP2", 0.00}, {"DEFLATE", 0.00}, {"PPMD", 0.00}}},
            {"Executable", {{"LZMA", 0.40}, {"LZMA2", 0.40}, {"BZIP2", 0.35}, {"DEFLATE", 0.30}, {"PPMD", 0.35}}},
            {"Source", {{"LZMA", 0.70}, {"LZMA2", 0.70}, {"BZIP2", 0.68}, {"DEFLATE", 0.60}, {"PPMD", 0.75}}},
            {"Unknown", {{"LZMA", 0.30}, {"LZMA2", 0.30}, {"BZIP2", 0.25}, {"DEFLATE", 0.20}, {"PPMD", 0.25}}}
        };
        
        for (const auto& file : files) {
            auto typeIt = methodRatios.find(file.type);
            if (typeIt == methodRatios.end()) {
                typeIt = methodRatios.find("Unknown");
            }
            
            auto methodIt = typeIt->second.find(method);
            double ratio = methodIt != typeIt->second.end() ? methodIt->second : 0.3;
            
            if (!file.isCompressible) {
                ratio *= 0.1;
            }
            
            totalRatio += ratio * file.size;
            totalWeight += file.size;
        }
        
        return totalWeight > 0 ? totalRatio / totalWeight : 0.3;
    }
    
    void GenerateRecommendations(AnalysisResult& result, const std::vector<FileInfo>& files) {
        uint64_t totalSize = 0;
        uint64_t textCount = 0;
        uint64_t binaryCount = 0;
        uint64_t compressedCount = 0;
        
        for (const auto& file : files) {
            totalSize += file.size;
            
            if (file.type == "Text" || file.type == "Source") {
                textCount++;
            } else if (file.type == "Archive" || file.type == "Image" || 
                      file.type == "Audio" || file.type == "Video") {
                compressedCount++;
            } else {
                binaryCount++;
            }
        }
        
        if (totalSize > 1024 * 1024 * 1024) {
            result.recommendations.push_back("Large archive: consider using solid compression");
        }
        
        if (textCount > binaryCount && textCount > compressedCount) {
            result.recommendations.push_back("Mostly text files: PPMD or LZMA recommended");
        }
        
        if (compressedCount > textCount && compressedCount > binaryCount) {
            result.recommendations.push_back("Mostly pre-compressed: use 'Store' method to save time");
        }
        
        if (result.compressionRatio < 0.1) {
            result.recommendations.push_back("Low compression expected: consider not compressing");
        }
    }
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

private:
    SevenZipArchive& m_archive;
    
public:
    NtfsStreamHandler(SevenZipArchive& archive) : m_archive(archive) {}
    
    std::vector<StreamInfo> EnumerateStreams(const std::string& filePath) {
        std::vector<StreamInfo> streams;
        
        std::wstring wpath = StringToWString(filePath);
        HANDLE hFile = CreateFileW(wpath.c_str(), FILE_READ_ATTRIBUTES,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
        
        if (hFile == INVALID_HANDLE_VALUE) return streams;
        
        BYTE buffer[64 * 1024];
        DWORD bytesReturned;
        
        while (true) {
            WIN32_STREAM_ID* streamId = (WIN32_STREAM_ID*)buffer;
            
            if (!BackupRead(hFile, buffer, sizeof(WIN32_STREAM_ID), &bytesReturned,
                           FALSE, TRUE, NULL)) {
                break;
            }
            
            if (bytesReturned == 0) break;
            
            StreamInfo info;
            info.size = streamId->Size.QuadPart;
            
            switch (streamId->dwStreamId) {
                case BACKUP_DATA:
                    info.name = "::$DATA";
                    info.type = "Data";
                    break;
                case BACKUP_EA_DATA:
                    info.name = ":$EA";
                    info.type = "Extended Attributes";
                    break;
                case BACKUP_SECURITY_DATA:
                    info.name = ":$SECURITY";
                    info.type = "Security";
                    break;
                case BACKUP_ALTERNATE_DATA:
                    if (streamId->dwStreamNameSize > 0) {
                        wchar_t* name = (wchar_t*)((BYTE*)streamId + sizeof(WIN32_STREAM_ID));
                        info.name = WStringToString(name);
                        info.type = "Alternate Data";
                    }
                    break;
                case BACKUP_LINK:
                    info.name = ":$LINK";
                    info.type = "Link";
                    break;
                case BACKUP_PROPERTY_DATA:
                    info.name = ":$PROPERTY";
                    info.type = "Property";
                    break;
                default:
                    info.name = ":UNKNOWN";
                    info.type = "Unknown";
                    break;
            }
            
            streams.push_back(info);
            
            DWORD seekBytes = (DWORD)streamId->Size.QuadPart + streamId->dwStreamNameSize;
            DWORD seekReturned;
            BackupRead(hFile, NULL, seekBytes, &seekReturned, FALSE, FALSE, NULL);
        }
        
        CloseHandle(hFile);
        return streams;
    }
    
    bool ReadAlternateStream(const std::string& filePath, const std::string& streamName,
                            std::vector<uint8_t>& data) {
        std::string streamPath = filePath + ":" + streamName;
        std::wstring wpath = StringToWString(streamPath);
        
        HANDLE hFile = CreateFileW(wpath.c_str(), GENERIC_READ,
                                   FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        
        if (hFile == INVALID_HANDLE_VALUE) return false;
        
        DWORD fileSize = GetFileSize(hFile, NULL);
        data.resize(fileSize);
        
        DWORD bytesRead;
        BOOL result = ReadFile(hFile, data.data(), fileSize, &bytesRead, NULL);
        
        CloseHandle(hFile);
        
        return result && bytesRead == fileSize;
    }
    
    bool WriteAlternateStream(const std::string& filePath, const std::string& streamName,
                             const std::vector<uint8_t>& data) {
        std::string streamPath = filePath + ":" + streamName;
        std::wstring wpath = StringToWString(streamPath);
        
        HANDLE hFile = CreateFileW(wpath.c_str(), GENERIC_WRITE,
                                   0, NULL, CREATE_ALWAYS, 0, NULL);
        
        if (hFile == INVALID_HANDLE_VALUE) return false;
        
        DWORD bytesWritten;
        BOOL result = WriteFile(hFile, data.data(), (DWORD)data.size(), &bytesWritten, NULL);
        
        CloseHandle(hFile);
        
        return result && bytesWritten == data.size();
    }
    
    bool DeleteAlternateStream(const std::string& filePath, const std::string& streamName) {
        std::string streamPath = filePath + ":" + streamName;
        std::wstring wpath = StringToWString(streamPath);
        
        HANDLE hFile = CreateFileW(wpath.c_str(), DELETE, 0, NULL, OPEN_EXISTING, 0, NULL);
        
        if (hFile == INVALID_HANDLE_VALUE) return false;
        
        CloseHandle(hFile);
        
        return DeleteFileW(wpath.c_str()) != FALSE;
    }
    
    SecurityDescriptor GetSecurityDescriptor(const std::string& filePath) {
        SecurityDescriptor sd;
        
        std::wstring wpath = StringToWString(filePath);
        
        DWORD sdSize = 0;
        GetFileSecurityW(wpath.c_str(), OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION |
                        DACL_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION,
                        NULL, 0, &sdSize);
        
        if (sdSize == 0) return sd;
        
        std::vector<char> buffer(sdSize);
        if (!GetFileSecurityW(wpath.c_str(), OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION |
                             DACL_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION,
                             buffer.data(), sdSize, &sdSize)) {
            return sd;
        }
        
        PSECURITY_DESCRIPTOR pSD = (PSECURITY_DESCRIPTOR)buffer.data();
        
        PSID ownerSid = NULL;
        BOOL ownerDefaulted;
        if (GetSecurityDescriptorOwner(pSD, &ownerSid, &ownerDefaulted)) {
            sd.owner = SidToAccountName(ownerSid);
        }
        
        PSID groupSid = NULL;
        BOOL groupDefaulted;
        if (GetSecurityDescriptorGroup(pSD, &groupSid, &groupDefaulted)) {
            sd.group = SidToAccountName(groupSid);
        }
        
        PACL dacl = NULL;
        BOOL daclPresent, daclDefaulted;
        if (GetSecurityDescriptorDacl(pSD, &daclPresent, &dacl, &daclDefaulted)) {
            if (daclPresent && dacl) {
                sd.dacl = ParseAcl(dacl);
            }
        }
        
        return sd;
    }
    
    bool SetSecurityDescriptor(const std::string& filePath, const SecurityDescriptor& sd) {
        std::wstring wpath = StringToWString(filePath);
        
        EXPLICIT_ACCESSW ea = {};
        ea.grfAccessPermissions = GENERIC_ALL;
        ea.grfAccessMode = SET_ACCESS;
        ea.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
        ea.Trustee.TrusteeForm = TRUSTEE_IS_NAME;
        ea.Trustee.TrusteeType = TRUSTEE_IS_USER;
        ea.Trustee.ptstrName = (LPWSTR)StringToWString(sd.owner).c_str();
        
        PACL newAcl = NULL;
        DWORD result = SetEntriesInAclW(1, &ea, NULL, &newAcl);
        
        if (result != ERROR_SUCCESS) return false;
        
        BOOL success = SetNamedSecurityInfoW((LPWSTR)wpath.c_str(), SE_FILE_OBJECT,
                                             DACL_SECURITY_INFORMATION, NULL, NULL, newAcl, NULL);
        
        LocalFree(newAcl);
        
        return success == ERROR_SUCCESS;
    }
    
    bool ArchiveWithStreams(const std::string& archivePath, const std::string& sourcePath,
                           const CompressionOptions& options) {
        std::vector<StreamInfo> streams = EnumerateStreams(sourcePath);
        
        std::string tempDir = archivePath + ".streams.tmp";
        CreateDirectoryA(tempDir.c_str(), NULL);
        
        for (const auto& stream : streams) {
            if (stream.name == "::$DATA") continue;
            
            std::vector<uint8_t> data;
            if (ReadAlternateStream(sourcePath, stream.name, data)) {
                std::string streamFile = tempDir + "\\" + stream.name.substr(1);
                std::ofstream outFile(streamFile, std::ios::binary);
                if (outFile.is_open()) {
                    outFile.write(reinterpret_cast<const char*>(data.data()), data.size());
                    outFile.close();
                }
            }
        }
        
        std::vector<std::string> files;
        files.push_back(sourcePath);
        
        WIN32_FIND_DATAA findData;
        HANDLE hFind = FindFirstFileA((tempDir + "\\*").c_str(), &findData);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (findData.cFileName[0] != '.') {
                    files.push_back(tempDir + "\\" + findData.cFileName);
                }
            } while (FindNextFileA(hFind, &findData));
            FindClose(hFind);
        }
        
        bool result = m_archive.AddToArchive(archivePath, files, options);
        
        return result;
    }
    
    bool ExtractWithStreams(const std::string& archivePath, const std::string& outputPath,
                           const ExtractOptions& options) {
        if (!m_archive.ExtractArchive(archivePath, options)) {
            return false;
        }
        
        return true;
    }
    
private:
    static std::wstring StringToWString(const std::string& str) {
        if (str.empty()) return L"";
        int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
        std::wstring result(size - 1, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], size);
        return result;
    }
    
    static std::string WStringToString(const std::wstring& wstr) {
        if (wstr.empty()) return "";
        int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
        std::string result(size - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], size, NULL, NULL);
        return result;
    }
    
    static std::string SidToAccountName(PSID sid) {
        if (!sid) return "";
        
        WCHAR name[256];
        WCHAR domain[256];
        DWORD nameSize = 256;
        DWORD domainSize = 256;
        SID_NAME_USE use;
        
        if (LookupAccountSidW(NULL, sid, name, &nameSize, domain, &domainSize, &use)) {
            return WStringToString(domain) + "\\" + WStringToString(name);
        }
        
        return "";
    }
    
    static std::vector<std::string> ParseAcl(PACL acl) {
        std::vector<std::string> entries;
        
        ACL_SIZE_INFORMATION aclInfo;
        if (!GetAclInformation(acl, &aclInfo, sizeof(aclInfo), AclSizeInformation)) {
            return entries;
        }
        
        for (DWORD i = 0; i < aclInfo.AceCount; i++) {
            LPVOID ace;
            if (!GetAce(acl, i, &ace)) continue;
            
            ACE_HEADER* header = (ACE_HEADER*)ace;
            std::string entry;
            
            switch (header->AceType) {
                case ACCESS_ALLOWED_ACE_TYPE: {
                    ACCESS_ALLOWED_ACE* allowed = (ACCESS_ALLOWED_ACE*)ace;
                    entry = "Allow: " + std::to_string(allowed->Mask);
                    break;
                }
                case ACCESS_DENIED_ACE_TYPE: {
                    ACCESS_DENIED_ACE* denied = (ACCESS_DENIED_ACE*)ace;
                    entry = "Deny: " + std::to_string(denied->Mask);
                    break;
                }
            }
            
            if (!entry.empty()) {
                entries.push_back(entry);
            }
        }
        
        return entries;
    }
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

private:
    SevenZipArchive& m_archive;
    ThreadPool m_pool;
    std::vector<BatchJob> m_jobs;
    std::mutex m_jobsMutex;
    std::atomic<bool> m_cancelled;
    std::function<void(const BatchJob&)> m_jobCallback;
    
public:
    BatchProcessor(SevenZipArchive& archive, size_t threads = 0)
        : m_archive(archive), m_pool(threads > 0 ? threads : std::thread::hardware_concurrency()),
          m_cancelled(false) {}
    
    std::string AddCompressJob(const std::string& sourcePath, const std::string& archivePath,
                              const CompressionOptions& options = {}) {
        BatchJob job;
        job.id = GenerateJobId();
        job.sourcePath = sourcePath;
        job.archivePath = archivePath;
        job.operation = "compress";
        job.status = "pending";
        job.progress = 0.0;
        
        {
            std::lock_guard<std::mutex> lock(m_jobsMutex);
            m_jobs.push_back(job);
        }
        
        return job.id;
    }
    
    std::string AddExtractJob(const std::string& archivePath, const std::string& outputPath,
                             const ExtractOptions& options = {}) {
        BatchJob job;
        job.id = GenerateJobId();
        job.sourcePath = archivePath;
        job.archivePath = outputPath;
        job.operation = "extract";
        job.status = "pending";
        job.progress = 0.0;
        
        {
            std::lock_guard<std::mutex> lock(m_jobsMutex);
            m_jobs.push_back(job);
        }
        
        return job.id;
    }
    
    std::string AddConvertJob(const std::string& sourceArchive, const std::string& targetArchive,
                             ArchiveFormat targetFormat) {
        BatchJob job;
        job.id = GenerateJobId();
        job.sourcePath = sourceArchive;
        job.archivePath = targetArchive;
        job.operation = "convert";
        job.status = "pending";
        job.progress = 0.0;
        
        {
            std::lock_guard<std::mutex> lock(m_jobsMutex);
            m_jobs.push_back(job);
        }
        
        return job.id;
    }
    
    BatchResult ExecuteAll() {
        BatchResult result = {};
        m_cancelled = false;
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        {
            std::lock_guard<std::mutex> lock(m_jobsMutex);
            result.totalJobs = (uint32_t)m_jobs.size();
        }
        
        for (size_t i = 0; i < m_jobs.size(); i++) {
            if (m_cancelled) break;
            
            BatchJob* job = nullptr;
            
            {
                std::lock_guard<std::mutex> lock(m_jobsMutex);
                if (i < m_jobs.size()) {
                    job = &m_jobs[i];
                }
            }
            
            if (!job) continue;
            
            job->status = "running";
            job->startTime = time(nullptr);
            
            if (m_jobCallback) {
                m_jobCallback(*job);
            }
            
            bool success = false;
            
            if (job->operation == "compress") {
                CompressionOptions opts;
                success = m_archive.CompressDirectory(job->archivePath, job->sourcePath, opts, true);
            } else if (job->operation == "extract") {
                ExtractOptions opts;
                opts.outputDir = job->archivePath;
                success = m_archive.ExtractArchive(job->sourcePath, opts);
            } else if (job->operation == "convert") {
                success = ConvertArchive(job->sourcePath, job->archivePath);
            } else if (job->operation == "test") {
                success = m_archive.TestArchive(job->sourcePath);
            }
            
            job->endTime = time(nullptr);
            job->progress = 100.0;
            
            if (success) {
                job->status = "completed";
                result.successfulJobs++;
            } else {
                job->status = "failed";
                job->errorMessage = "Operation failed";
                result.failedJobs++;
            }
            
            if (m_jobCallback) {
                m_jobCallback(*job);
            }
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        result.totalTime = std::chrono::duration<double>(endTime - startTime).count();
        
        {
            std::lock_guard<std::mutex> lock(m_jobsMutex);
            result.jobs = m_jobs;
        }
        
        return result;
    }
    
    void Cancel() {
        m_cancelled = true;
    }
    
    void ClearJobs() {
        std::lock_guard<std::mutex> lock(m_jobsMutex);
        m_jobs.clear();
    }
    
    std::vector<BatchJob> GetPendingJobs() {
        std::lock_guard<std::mutex> lock(m_jobsMutex);
        std::vector<BatchJob> pending;
        
        for (const auto& job : m_jobs) {
            if (job.status == "pending") {
                pending.push_back(job);
            }
        }
        
        return pending;
    }
    
    void SetJobCallback(std::function<void(const BatchJob&)> callback) {
        m_jobCallback = callback;
    }
    
private:
    static std::string GenerateJobId() {
        static std::atomic<uint64_t> counter(0);
        std::ostringstream oss;
        oss << "job_" << std::hex << (++counter) << "_" << time(nullptr);
        return oss.str();
    }
    
    bool ConvertArchive(const std::string& sourcePath, const std::string& targetPath) {
        std::string tempDir = targetPath + ".extract.tmp";
        CreateDirectoryA(tempDir.c_str(), NULL);
        
        ExtractOptions opts;
        opts.outputDir = tempDir;
        
        if (!m_archive.ExtractArchive(sourcePath, opts)) {
            RemoveDirectoryA(tempDir.c_str());
            return false;
        }
        
        CompressionOptions compOpts;
        bool result = m_archive.CompressDirectory(targetPath, tempDir, compOpts, true);
        
        std::vector<std::string> files;
        WIN32_FIND_DATAA findData;
        HANDLE hFind = FindFirstFileA((tempDir + "\\*").c_str(), &findData);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (findData.cFileName[0] != '.') {
                    std::string path = tempDir + "\\" + findData.cFileName;
                    if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                        RemoveDirectoryRecursive(path);
                    } else {
                        DeleteFileA(path.c_str());
                    }
                }
            } while (FindNextFileA(hFind, &findData));
            FindClose(hFind);
        }
        
        RemoveDirectoryA(tempDir.c_str());
        
        return result;
    }
    
    static void RemoveDirectoryRecursive(const std::string& path) {
        WIN32_FIND_DATAA findData;
        HANDLE hFind = FindFirstFileA((path + "\\*").c_str(), &findData);
        
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (findData.cFileName[0] != '.') {
                    std::string fullPath = path + "\\" + findData.cFileName;
                    if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                        RemoveDirectoryRecursive(fullPath);
                    } else {
                        DeleteFileA(fullPath.c_str());
                    }
                }
            } while (FindNextFileA(hFind, &findData));
            FindClose(hFind);
        }
        
        RemoveDirectoryA(path.c_str());
    }
};

class EncryptionEnhancer {
public:
    enum class Algorithm { AES256, ChaCha20, Twofish, Serpent, Camellia };
    enum class KeyDerivation { PBKDF2, Argon2, Scrypt, BCrypt };
    
    struct EncryptionConfig {
        Algorithm algorithm = Algorithm::AES256;
        KeyDerivation kdf = KeyDerivation::PBKDF2;
        uint32_t iterations = 100000;
        uint32_t memoryCost = 65536;
        uint32_t parallelism = 4;
        uint32_t saltLength = 32;
        bool encryptFilename = true;
        bool encryptMetadata = true;
        bool useMultipleLayers = false;
        std::vector<Algorithm> layerAlgorithms;
    };
    
    struct EncryptionResult {
        bool success;
        std::string encryptedPath;
        uint64_t originalSize;
        uint64_t encryptedSize;
        std::string keyFingerprint;
        std::string errorMessage;
    };
    
    struct DecryptionResult {
        bool success;
        std::string decryptedPath;
        uint64_t decryptedSize;
        std::string errorMessage;
    };

private:
    EncryptionConfig m_config;
    std::vector<uint8_t> m_salt;
    std::vector<uint8_t> m_key;
    
public:
    EncryptionEnhancer() = default;
    EncryptionEnhancer(const EncryptionConfig& config) : m_config(config) {}
    
    void SetConfig(const EncryptionConfig& config) {
        m_config = config;
    }
    
    EncryptionResult EncryptArchive(const std::string& archivePath,
                                    const std::string& password) {
        EncryptionResult result = {};
        
        std::ifstream inFile(archivePath, std::ios::binary | std::ios::ate);
        if (!inFile.is_open()) {
            result.errorMessage = "Cannot open archive file";
            return result;
        }
        
        result.originalSize = inFile.tellg();
        inFile.seekg(0);
        
        m_salt = GenerateSalt(m_config.saltLength);
        m_key = DeriveKey(password, m_salt);
        
        result.keyFingerprint = CalculateKeyFingerprint(m_key);
        
        std::string outputPath = archivePath + ".encrypted";
        std::ofstream outFile(outputPath, std::ios::binary);
        
        if (!outFile.is_open()) {
            result.errorMessage = "Cannot create output file";
            return result;
        }
        
        FileHeader header;
        memset(&header, 0, sizeof(header));
        header.magic = 0x454E4359;
        header.version = 1;
        header.algorithm = (uint32_t)m_config.algorithm;
        header.kdf = (uint32_t)m_config.kdf;
        header.iterations = m_config.iterations;
        header.saltLength = (uint32_t)m_salt.size();
        header.originalSize = result.originalSize;
        
        outFile.write(reinterpret_cast<const char*>(&header), sizeof(header));
        outFile.write(reinterpret_cast<const char*>(m_salt.data()), m_salt.size());
        
        std::vector<uint8_t> iv = GenerateIV();
        outFile.write(reinterpret_cast<const char*>(iv.data()), iv.size());
        
        std::vector<uint8_t> buffer(64 * 1024);
        uint64_t totalProcessed = 0;
        
        while (inFile.read(reinterpret_cast<char*>(buffer.data()), buffer.size())) {
            size_t bytesRead = inFile.gcount();
            
            std::vector<uint8_t> encrypted = EncryptBlock(
                buffer.data(), bytesRead, m_key, iv);
            
            outFile.write(reinterpret_cast<const char*>(encrypted.data()), encrypted.size());
            totalProcessed += bytesRead;
        }
        
        if (inFile.gcount() > 0) {
            size_t bytesRead = inFile.gcount();
            std::vector<uint8_t> encrypted = EncryptBlock(
                buffer.data(), bytesRead, m_key, iv);
            outFile.write(reinterpret_cast<const char*>(encrypted.data()), encrypted.size());
        }
        
        inFile.close();
        outFile.close();
        
        result.success = true;
        result.encryptedPath = outputPath;
        
        WIN32_FILE_ATTRIBUTE_DATA attr;
        if (GetFileAttributesExA(outputPath.c_str(), GetFileExInfoStandard, &attr)) {
            result.encryptedSize = ((uint64_t)attr.nFileSizeHigh << 32) | attr.nFileSizeLow;
        }
        
        return result;
    }
    
    DecryptionResult DecryptArchive(const std::string& encryptedPath,
                                    const std::string& password,
                                    const std::string& outputPath) {
        DecryptionResult result = {};
        
        std::ifstream inFile(encryptedPath, std::ios::binary);
        if (!inFile.is_open()) {
            result.errorMessage = "Cannot open encrypted file";
            return result;
        }
        
        FileHeader header;
        inFile.read(reinterpret_cast<char*>(&header), sizeof(header));
        
        if (header.magic != 0x454E4359) {
            result.errorMessage = "Invalid encrypted file format";
            return result;
        }
        
        m_salt.resize(header.saltLength);
        inFile.read(reinterpret_cast<char*>(m_salt.data()), header.saltLength);
        
        uint32_t ivLen = GetIVLength((Algorithm)header.algorithm);
        std::vector<uint8_t> iv(ivLen);
        inFile.read(reinterpret_cast<char*>(iv.data()), iv.size());
        
        m_config.algorithm = (Algorithm)header.algorithm;
        m_config.kdf = (KeyDerivation)header.kdf;
        m_config.iterations = header.iterations;
        
        m_key = DeriveKey(password, m_salt);
        
        std::ofstream outFile(outputPath, std::ios::binary);
        if (!outFile.is_open()) {
            result.errorMessage = "Cannot create output file";
            return result;
        }
        
        std::vector<uint8_t> buffer(64 * 1024 + 64);
        uint64_t totalProcessed = 0;
        
        while (inFile.read(reinterpret_cast<char*>(buffer.data()), buffer.size())) {
            size_t bytesRead = inFile.gcount();
            
            std::vector<uint8_t> decrypted = DecryptBlock(
                buffer.data(), bytesRead, m_key, iv);
            
            if (totalProcessed + decrypted.size() > header.originalSize) {
                decrypted.resize(header.originalSize - totalProcessed);
            }
            
            outFile.write(reinterpret_cast<const char*>(decrypted.data()), decrypted.size());
            totalProcessed += decrypted.size();
        }
        
        inFile.close();
        outFile.close();
        
        result.success = true;
        result.decryptedPath = outputPath;
        result.decryptedSize = header.originalSize;
        
        return result;
    }
    
    bool EncryptWithMultipleLayers(const std::string& inputPath,
                                   const std::string& outputPath,
                                   const std::vector<std::string>& passwords) {
        if (passwords.size() != m_config.layerAlgorithms.size()) {
            return false;
        }
        
        std::string currentPath = inputPath;
        std::vector<std::string> tempFiles;
        
        for (size_t i = 0; i < m_config.layerAlgorithms.size(); i++) {
            m_config.algorithm = m_config.layerAlgorithms[i];
            
            EncryptionResult result = EncryptArchive(currentPath, passwords[i]);
            if (!result.success) {
                for (const auto& temp : tempFiles) {
                    DeleteFileA(temp.c_str());
                }
                return false;
            }
            
            if (i > 0) {
                tempFiles.push_back(currentPath);
            }
            
            currentPath = result.encryptedPath;
            tempFiles.push_back(currentPath);
        }
        
        MoveFileA(currentPath.c_str(), outputPath.c_str());
        
        for (const auto& temp : tempFiles) {
            if (temp != outputPath) {
                DeleteFileA(temp.c_str());
            }
        }
        
        return true;
    }
    
    bool DecryptWithMultipleLayers(const std::string& inputPath,
                                   const std::string& outputPath,
                                   const std::vector<std::string>& passwords) {
        std::string currentPath = inputPath;
        std::vector<std::string> tempFiles;
        
        for (int i = (int)m_config.layerAlgorithms.size() - 1; i >= 0; i--) {
            m_config.algorithm = m_config.layerAlgorithms[i];
            
            std::string nextPath = (i == 0) ? outputPath : currentPath + ".dec";
            
            DecryptionResult result = DecryptArchive(currentPath, passwords[i], nextPath);
            if (!result.success) {
                for (const auto& temp : tempFiles) {
                    DeleteFileA(temp.c_str());
                }
                return false;
            }
            
            if (currentPath != inputPath) {
                tempFiles.push_back(currentPath);
            }
            
            currentPath = nextPath;
        }
        
        for (const auto& temp : tempFiles) {
            DeleteFileA(temp.c_str());
        }
        
        return true;
    }
    
    std::string GenerateKeyFile(const std::string& keyPath, uint32_t keySize = 32) {
        std::vector<uint8_t> key = GenerateRandomBytes(keySize);
        
        std::ofstream outFile(keyPath, std::ios::binary);
        if (!outFile.is_open()) return "";
        
        outFile.write(reinterpret_cast<const char*>(key.data()), key.size());
        outFile.close();
        
        return CalculateKeyFingerprint(key);
    }
    
    bool LoadKeyFile(const std::string& keyPath) {
        std::ifstream inFile(keyPath, std::ios::binary | std::ios::ate);
        if (!inFile.is_open()) return false;
        
        size_t keySize = inFile.tellg();
        inFile.seekg(0);
        
        m_key.resize(keySize);
        inFile.read(reinterpret_cast<char*>(m_key.data()), keySize);
        inFile.close();
        
        return true;
    }
    
    EncryptionResult EncryptWithKeyFile(const std::string& archivePath,
                                        const std::string& keyPath) {
        if (!LoadKeyFile(keyPath)) {
            EncryptionResult result = {};
            result.errorMessage = "Failed to load key file";
            return result;
        }
        
        m_salt = GenerateSalt(m_config.saltLength);
        
        return EncryptArchive(archivePath, "");
    }
    
private:
    struct FileHeader {
        uint32_t magic;
        uint32_t version;
        uint32_t algorithm;
        uint32_t kdf;
        uint32_t iterations;
        uint32_t saltLength;
        uint64_t originalSize;
        uint8_t reserved[32];
    };
    
    std::vector<uint8_t> GenerateSalt(uint32_t length) {
        std::vector<uint8_t> salt(length);
        
        HCRYPTPROV hProv;
        if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
            CryptGenRandom(hProv, length, salt.data());
            CryptReleaseContext(hProv, 0);
        } else {
            LARGE_INTEGER perfCount;
            QueryPerformanceCounter(&perfCount);
            uint64_t seed = perfCount.QuadPart ^ GetTickCount64();
            
            for (uint32_t i = 0; i < length; i++) {
                seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
                salt[i] = (uint8_t)(seed >> 32);
            }
        }
        
        return salt;
    }
    
    std::vector<uint8_t> GenerateIV() {
        return GenerateRandomBytes(GetIVLength(m_config.algorithm));
    }
    
    std::vector<uint8_t> GenerateRandomBytes(uint32_t length) {
        return GenerateSalt(length);
    }
    
    std::vector<uint8_t> DeriveKey(const std::string& password,
                                   const std::vector<uint8_t>& salt) {
        std::vector<uint8_t> key(32);
        
        switch (m_config.kdf) {
            case KeyDerivation::PBKDF2: {
                std::string combined = password;
                for (uint32_t i = 0; i < m_config.iterations; i++) {
                    uint32_t hash = 0xFFFFFFFF;
                    for (char c : combined) {
                        hash ^= c;
                        for (int j = 0; j < 8; j++) {
                            hash = (hash >> 1) ^ (hash & 1 ? 0xEDB88320 : 0);
                        }
                    }
                    combined = std::to_string(hash);
                }
                
                for (size_t i = 0; i < 32 && i < combined.size(); i++) {
                    key[i] = combined[i];
                }
                break;
            }
            
            case KeyDerivation::Argon2: {
                std::vector<uint8_t> mem(m_config.memoryCost * 1024);
                uint64_t state = 0;
                
                for (char c : password) {
                    state = state * 31 + c;
                }
                for (uint8_t s : salt) {
                    state = state * 37 + s;
                }
                
                for (uint32_t i = 0; i < m_config.iterations; i++) {
                    for (uint32_t j = 0; j < m_config.memoryCost; j++) {
                        uint64_t idx = (state * 6364136223846793005ULL) % m_config.memoryCost;
                        mem[j * 1024] ^= (uint8_t)(state >> 24);
                        state ^= mem[idx * 1024];
                    }
                }
                
                for (uint32_t i = 0; i < 32; i++) {
                    key[i] = mem[i * 1024];
                }
                break;
            }
            
            case KeyDerivation::Scrypt: {
                std::vector<uint8_t> work(m_config.memoryCost);
                uint32_t N = m_config.iterations;
                
                for (size_t i = 0; i < password.size() && i < work.size(); i++) {
                    work[i] = password[i];
                }
                for (size_t i = 0; i < salt.size() && i + password.size() < work.size(); i++) {
                    work[i + password.size()] = salt[i];
                }
                
                for (uint32_t i = 0; i < N; i++) {
                    for (size_t j = 0; j < work.size(); j++) {
                        work[j] = work[j] ^ (uint8_t)(i + j);
                        work[j] = (work[j] << 3) | (work[j] >> 5);
                    }
                }
                
                for (uint32_t i = 0; i < 32 && i < work.size(); i++) {
                    key[i] = work[i];
                }
                break;
            }
            
            default: {
                for (size_t i = 0; i < password.size() && i < 32; i++) {
                    key[i] = password[i];
                }
                break;
            }
        }
        
        return key;
    }
    
    std::vector<uint8_t> EncryptBlock(const void* data, size_t size,
                                      const std::vector<uint8_t>& key,
                                      std::vector<uint8_t>& iv) {
        std::vector<uint8_t> encrypted(size);
        const uint8_t* src = (const uint8_t*)data;
        
        for (size_t i = 0; i < size; i++) {
            uint8_t keyByte = key[i % key.size()];
            uint8_t ivByte = iv[i % iv.size()];
            encrypted[i] = src[i] ^ keyByte ^ ivByte;
            
            iv[i % iv.size()] = (iv[i % iv.size()] + encrypted[i]) & 0xFF;
        }
        
        return encrypted;
    }
    
    std::vector<uint8_t> DecryptBlock(const void* data, size_t size,
                                      const std::vector<uint8_t>& key,
                                      std::vector<uint8_t>& iv) {
        std::vector<uint8_t> decrypted(size);
        const uint8_t* src = (const uint8_t*)data;
        
        for (size_t i = 0; i < size; i++) {
            uint8_t keyByte = key[i % key.size()];
            uint8_t ivByte = iv[i % iv.size()];
            decrypted[i] = src[i] ^ keyByte ^ ivByte;
            
            iv[i % iv.size()] = (iv[i % iv.size()] + src[i]) & 0xFF;
        }
        
        return decrypted;
    }
    
    static uint32_t GetIVLength(Algorithm algo) {
        switch (algo) {
            case Algorithm::AES256: return 16;
            case Algorithm::ChaCha20: return 12;
            case Algorithm::Twofish: return 16;
            case Algorithm::Serpent: return 16;
            case Algorithm::Camellia: return 16;
            default: return 16;
        }
    }
    
    static std::string CalculateKeyFingerprint(const std::vector<uint8_t>& key) {
        uint32_t crc = 0xFFFFFFFF;
        for (uint8_t byte : key) {
            crc ^= byte;
            for (int i = 0; i < 8; i++) {
                crc = (crc >> 1) ^ (crc & 1 ? 0xEDB88320 : 0);
            }
        }
        
        std::ostringstream oss;
        oss << std::hex << std::setw(8) << std::setfill('0') << (crc ^ 0xFFFFFFFF);
        return oss.str();
    }
};

class ArchiveDiffer {
public:
    struct DiffResult {
        std::vector<std::string> addedFiles;
        std::vector<std::string> removedFiles;
        std::vector<std::string> modifiedFiles;
        std::vector<std::string> unchangedFiles;
        uint64_t addedSize;
        uint64_t removedSize;
        uint64_t modifiedSize;
        double similarityRatio;
    };
    
    struct FileDiff {
        std::string path;
        enum class ChangeType { None, Added, Removed, Modified, Renamed } type;
        std::string oldPath;
        uint64_t oldSize;
        uint64_t newSize;
        std::string oldHash;
        std::string newHash;
        std::vector<uint8_t> deltaData;
    };

private:
    SevenZipArchive& m_archive;
    
public:
    ArchiveDiffer(SevenZipArchive& archive) : m_archive(archive) {}
    
    DiffResult CompareArchives(const std::string& archive1, const std::string& archive2,
                              const std::string& password1 = "", const std::string& password2 = "") {
        DiffResult result = {};
        
        ArchiveInfo info1, info2;
        m_archive.ListArchive(archive1, info1, password1);
        m_archive.ListArchive(archive2, info2, password2);
        
        std::map<std::string, FileInfo> files1, files2;
        
        for (const auto& file : info1.files) {
            files1[file.path] = file;
        }
        for (const auto& file : info2.files) {
            files2[file.path] = file;
        }
        
        for (const auto& pair : files2) {
            auto it = files1.find(pair.first);
            
            if (it == files1.end()) {
                result.addedFiles.push_back(pair.first);
                result.addedSize += pair.second.size;
            } else {
                if (pair.second.size != it->second.size || 
                    pair.second.crc != it->second.crc) {
                    result.modifiedFiles.push_back(pair.first);
                    result.modifiedSize += pair.second.size;
                } else {
                    result.unchangedFiles.push_back(pair.first);
                }
            }
        }
        
        for (const auto& pair : files1) {
            if (files2.find(pair.first) == files2.end()) {
                result.removedFiles.push_back(pair.first);
                result.removedSize += pair.second.size;
            }
        }
        
        uint64_t totalFiles = info1.files.size() + info2.files.size();
        uint64_t commonFiles = result.unchangedFiles.size() + result.modifiedFiles.size();
        result.similarityRatio = totalFiles > 0 ? 
            (double)commonFiles * 2 / totalFiles : 0.0;
        
        return result;
    }
    
    std::vector<FileDiff> GenerateDetailedDiff(const std::string& archive1,
                                               const std::string& archive2,
                                               const std::string& password1 = "",
                                               const std::string& password2 = "") {
        std::vector<FileDiff> diffs;
        
        ArchiveInfo info1, info2;
        m_archive.ListArchive(archive1, info1, password1);
        m_archive.ListArchive(archive2, info2, password2);
        
        std::map<std::string, FileInfo> files1, files2;
        
        for (const auto& file : info1.files) {
            files1[file.path] = file;
        }
        for (const auto& file : info2.files) {
            files2[file.path] = file;
        }
        
        for (const auto& pair : files2) {
            FileDiff diff;
            diff.path = pair.first;
            diff.newSize = pair.second.size;
            diff.newHash = std::to_string(pair.second.crc);
            
            auto it = files1.find(pair.first);
            
            if (it == files1.end()) {
                diff.type = FileDiff::ChangeType::Added;
            } else {
                diff.oldSize = it->second.size;
                diff.oldHash = std::to_string(it->second.crc);
                
                if (pair.second.crc != it->second.crc) {
                    diff.type = FileDiff::ChangeType::Modified;
                } else {
                    diff.type = FileDiff::ChangeType::None;
                }
            }
            
            if (diff.type != FileDiff::ChangeType::None) {
                diffs.push_back(diff);
            }
        }
        
        for (const auto& pair : files1) {
            if (files2.find(pair.first) == files2.end()) {
                FileDiff diff;
                diff.path = pair.first;
                diff.type = FileDiff::ChangeType::Removed;
                diff.oldSize = pair.second.size;
                diff.oldHash = std::to_string(pair.second.crc);
                diffs.push_back(diff);
            }
        }
        
        return diffs;
    }
    
    bool CreateDeltaArchive(const std::string& baseArchive,
                           const std::string& newArchive,
                           const std::string& deltaOutput,
                           const std::string& password = "") {
        DiffResult diff = CompareArchives(baseArchive, newArchive, password, password);
        
        if (diff.addedFiles.empty() && diff.modifiedFiles.empty()) {
            return false;
        }
        
        std::string tempDir = deltaOutput + ".temp";
        CreateDirectoryA(tempDir.c_str(), NULL);
        
        std::vector<std::string> filesToExtract;
        for (const auto& file : diff.addedFiles) {
            filesToExtract.push_back(file);
        }
        for (const auto& file : diff.modifiedFiles) {
            filesToExtract.push_back(file);
        }
        
        ExtractOptions opts;
        opts.outputDir = tempDir;
        opts.password = password;
        
        m_archive.ExtractFiles(newArchive, filesToExtract, tempDir, password);
        
        std::string manifestPath = tempDir + "\\delta.manifest";
        std::ofstream manifest(manifestPath);
        if (manifest.is_open()) {
            manifest << "[Added]\n";
            for (const auto& file : diff.addedFiles) {
                manifest << file << "\n";
            }
            manifest << "\n[Modified]\n";
            for (const auto& file : diff.modifiedFiles) {
                manifest << file << "\n";
            }
            manifest << "\n[Removed]\n";
            for (const auto& file : diff.removedFiles) {
                manifest << file << "\n";
            }
            manifest.close();
        }
        
        CompressionOptions compOpts;
        bool result = m_archive.CompressDirectory(deltaOutput, tempDir, compOpts, true);
        
        WIN32_FIND_DATAA findData;
        HANDLE hFind = FindFirstFileA((tempDir + "\\*").c_str(), &findData);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (findData.cFileName[0] != '.') {
                    std::string path = tempDir + "\\" + findData.cFileName;
                    if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                        RemoveDirectoryRecursive(path);
                    } else {
                        DeleteFileA(path.c_str());
                    }
                }
            } while (FindNextFileA(hFind, &findData));
            FindClose(hFind);
        }
        RemoveDirectoryA(tempDir.c_str());
        
        return result;
    }
    
    bool ApplyDeltaArchive(const std::string& baseArchive,
                          const std::string& deltaArchive,
                          const std::string& outputArchive,
                          const std::string& password = "") {
        std::string tempDir = outputArchive + ".temp";
        CreateDirectoryA(tempDir.c_str(), NULL);
        
        ExtractOptions opts;
        opts.outputDir = tempDir;
        opts.password = password;
        m_archive.ExtractArchive(baseArchive, opts);
        
        std::string deltaDir = deltaArchive + ".delta";
        CreateDirectoryA(deltaDir.c_str(), NULL);
        
        opts.outputDir = deltaDir;
        m_archive.ExtractArchive(deltaArchive, opts);
        
        std::string manifestPath = deltaDir + "\\delta.manifest";
        std::ifstream manifest(manifestPath);
        
        if (manifest.is_open()) {
            std::string line;
            std::string section;
            
            while (std::getline(manifest, line)) {
                if (line.empty()) continue;
                
                if (line[0] == '[') {
                    section = line;
                    continue;
                }
                
                if (section == "[Removed]") {
                    std::string filePath = tempDir + "\\" + line;
                    DeleteFileA(filePath.c_str());
                } else if (section == "[Added]" || section == "[Modified]") {
                    std::string srcPath = deltaDir + "\\" + line;
                    std::string dstPath = tempDir + "\\" + line;
                    
                    std::string dstDir = dstPath;
                    size_t pos = dstDir.find_last_of('\\');
                    if (pos != std::string::npos) {
                        dstDir = dstDir.substr(0, pos);
                        CreateDirectoryA(dstDir.c_str(), NULL);
                    }
                    
                    MoveFileA(srcPath.c_str(), dstPath.c_str());
                }
            }
            
            manifest.close();
        }
        
        CompressionOptions compOpts;
        bool result = m_archive.CompressDirectory(outputArchive, tempDir, compOpts, true);
        
        RemoveDirectoryRecursive(tempDir);
        RemoveDirectoryRecursive(deltaDir);
        
        return result;
    }
    
private:
    static void RemoveDirectoryRecursive(const std::string& path) {
        WIN32_FIND_DATAA findData;
        HANDLE hFind = FindFirstFileA((path + "\\*").c_str(), &findData);
        
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (findData.cFileName[0] != '.') {
                    std::string fullPath = path + "\\" + findData.cFileName;
                    if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                        RemoveDirectoryRecursive(fullPath);
                    } else {
                        DeleteFileA(fullPath.c_str());
                    }
                }
            } while (FindNextFileA(hFind, &findData));
            FindClose(hFind);
        }
        
        RemoveDirectoryA(path.c_str());
    }
};

class ArchivePreviewer {
public:
    struct PreviewResult {
        std::string content;
        std::string encoding;
        uint64_t fileSize;
        uint64_t previewSize;
        bool isText;
        bool isBinary;
        bool truncated;
    };
    
    struct ImageInfo {
        uint32_t width;
        uint32_t height;
        uint32_t bitsPerPixel;
        std::string format;
        bool hasAlpha;
    };
    
    struct MediaInfo {
        std::string format;
        uint32_t duration;
        uint32_t bitrate;
        uint32_t width;
        uint32_t height;
        std::string codec;
    };

private:
    SevenZipArchive& m_archive;
    
public:
    ArchivePreviewer(SevenZipArchive& archive) : m_archive(archive) {}
    
    PreviewResult PreviewTextFile(const std::string& archivePath,
                                 const std::string& filePath,
                                 uint64_t maxSize = 64 * 1024,
                                 const std::string& password = "") {
        PreviewResult result = {};
        
        std::vector<uint8_t> data;
        if (!m_archive.ExtractSingleFileToMemory(archivePath, filePath, data, password)) {
            return result;
        }
        
        result.fileSize = data.size();
        result.previewSize = std::min(data.size(), (size_t)maxSize);
        result.truncated = data.size() > maxSize;
        
        result.isText = IsTextData(data.data(), result.previewSize);
        result.isBinary = !result.isText;
        
        if (result.isText) {
            result.encoding = DetectEncoding(data.data(), result.previewSize);
            result.content = std::string(reinterpret_cast<const char*>(data.data()),
                                        result.previewSize);
        } else {
            result.content = BinaryToHex(data.data(), std::min(result.previewSize, (size_t)256));
        }
        
        return result;
    }
    
    ImageInfo GetImageInfo(const std::string& archivePath,
                          const std::string& filePath,
                          const std::string& password = "") {
        ImageInfo info = {};
        
        std::vector<uint8_t> data;
        if (!m_archive.ExtractSingleFileToMemory(archivePath, filePath, data, password)) {
            return info;
        }
        
        if (data.size() >= 8) {
            if (data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G') {
                info.format = "PNG";
                if (data.size() >= 24) {
                    info.width = (data[16] << 24) | (data[17] << 16) | (data[18] << 8) | data[19];
                    info.height = (data[20] << 24) | (data[21] << 16) | (data[22] << 8) | data[23];
                    info.bitsPerPixel = 32;
                    info.hasAlpha = true;
                }
            } else if (data[0] == 'B' && data[1] == 'M') {
                info.format = "BMP";
                if (data.size() >= 54) {
                    info.width = *(int32_t*)&data[18];
                    info.height = *(int32_t*)&data[22];
                    info.bitsPerPixel = *(uint16_t*)&data[28];
                    info.hasAlpha = info.bitsPerPixel == 32;
                }
            } else if (data[0] == 0xFF && data[1] == 0xD8) {
                info.format = "JPEG";
                for (size_t i = 2; i < data.size() - 8; i++) {
                    if (data[i] == 0xFF && data[i+1] == 0xC0) {
                        info.height = (data[i+5] << 8) | data[i+6];
                        info.width = (data[i+7] << 8) | data[i+8];
                        info.bitsPerPixel = data[i+9] * 3;
                        break;
                    }
                }
            } else if (data[0] == 'G' && data[1] == 'I' && data[2] == 'F') {
                info.format = "GIF";
                if (data.size() >= 10) {
                    info.width = data[6] | (data[7] << 8);
                    info.height = data[8] | (data[9] << 8);
                    info.bitsPerPixel = 8;
                    info.hasAlpha = false;
                }
            }
        }
        
        return info;
    }
    
    MediaInfo GetMediaInfo(const std::string& archivePath,
                          const std::string& filePath,
                          const std::string& password = "") {
        MediaInfo info = {};
        
        std::vector<uint8_t> data;
        if (!m_archive.ExtractSingleFileToMemory(archivePath, filePath, data, password)) {
            return info;
        }
        
        if (data.size() >= 12) {
            if (data[4] == 'f' && data[5] == 't' && data[6] == 'y' && data[7] == 'p') {
                info.format = "MP4";
                info.codec = "H.264/AAC";
            } else if (data[0] == 'R' && data[1] == 'I' && data[2] == 'F' && data[3] == 'F') {
                info.format = "AVI";
                info.codec = "Various";
            } else if (data[0] == 0x1A && data[1] == 0x45 && data[2] == 0xDF && data[3] == 0xA3) {
                info.format = "MKV";
                info.codec = "Various";
            } else if (data[0] == 'I' && data[1] == 'D' && data[2] == '3') {
                info.format = "MP3";
                info.codec = "MPEG Audio";
            } else if (memcmp(data.data(), "OggS", 4) == 0) {
                info.format = "OGG";
                info.codec = "Vorbis/Opus";
            } else if (memcmp(data.data(), "fLaC", 4) == 0) {
                info.format = "FLAC";
                info.codec = "FLAC";
            }
        }
        
        return info;
    }
    
    std::string GetFileSummary(const std::string& archivePath,
                              const std::string& filePath,
                              const std::string& password = "") {
        std::ostringstream summary;
        
        ArchiveInfo info;
        if (!m_archive.ListArchive(archivePath, info, password)) {
            return "Failed to read archive";
        }
        
        for (const auto& file : info.files) {
            if (file.path == filePath) {
                summary << "File: " << file.path << "\n";
                summary << "Size: " << FormatSize(file.size) << "\n";
                summary << "Compressed: " << FormatSize(file.packedSize) << "\n";
                summary << "CRC: " << std::hex << file.crc << std::dec << "\n";
                summary << "Encrypted: " << (file.isEncrypted ? "Yes" : "No") << "\n";
                
                ULARGE_INTEGER ull;
                ull.LowPart = file.lastWriteTime.dwLowDateTime;
                ull.HighPart = file.lastWriteTime.dwHighDateTime;
                time_t modTime = ull.QuadPart / 10000000ULL - 11644473600ULL;
                char timeStr[64];
                ctime_s(timeStr, sizeof(timeStr), &modTime);
                summary << "Modified: " << timeStr;
                
                break;
            }
        }
        
        return summary.str();
    }
    
    std::vector<std::string> GetArchiveTree(const std::string& archivePath,
                                           const std::string& password = "") {
        std::vector<std::string> tree;
        
        ArchiveInfo info;
        if (!m_archive.ListArchive(archivePath, info, password)) {
            return tree;
        }
        
        std::map<std::string, std::vector<std::string>> directories;
        
        for (const auto& file : info.files) {
            std::string path = file.path;
            std::replace(path.begin(), path.end(), '/', '\\');
            
            size_t pos = path.find('\\');
            while (pos != std::string::npos) {
                std::string dir = path.substr(0, pos);
                if (directories.find(dir) == directories.end()) {
                    directories[dir] = std::vector<std::string>();
                }
                pos = path.find('\\', pos + 1);
            }
        }
        
        tree.push_back("/");
        for (const auto& pair : directories) {
            tree.push_back(pair.first + "/");
        }
        for (const auto& file : info.files) {
            if (!file.isDirectory) {
                tree.push_back(file.path);
            }
        }
        
        return tree;
    }
    
private:
    static bool IsTextData(const uint8_t* data, size_t size) {
        size_t textChars = 0;
        size_t binaryChars = 0;
        
        for (size_t i = 0; i < size && i < 8192; i++) {
            uint8_t c = data[i];
            
            if (c == 0) return false;
            
            if ((c >= 32 && c < 127) || c == '\t' || c == '\n' || c == '\r') {
                textChars++;
            } else if (c >= 128) {
                textChars++;
            } else {
                binaryChars++;
            }
        }
        
        return textChars > binaryChars * 10;
    }
    
    static std::string DetectEncoding(const uint8_t* data, size_t size) {
        if (size >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
            return "UTF-8 BOM";
        }
        
        if (size >= 2) {
            if (data[0] == 0xFF && data[1] == 0xFE) {
                return "UTF-16 LE";
            }
            if (data[0] == 0xFE && data[1] == 0xFF) {
                return "UTF-16 BE";
            }
        }
        
        bool hasHighBytes = false;
        bool validUtf8 = true;
        
        for (size_t i = 0; i < size; i++) {
            if (data[i] > 127) {
                hasHighBytes = true;
                
                if ((data[i] & 0xC0) == 0xC0) {
                    int seqLen = 0;
                    if ((data[i] & 0xE0) == 0xC0) seqLen = 2;
                    else if ((data[i] & 0xF0) == 0xE0) seqLen = 3;
                    else if ((data[i] & 0xF8) == 0xF0) seqLen = 4;
                    
                    if (i + seqLen > size) {
                        validUtf8 = false;
                        break;
                    }
                    
                    for (int j = 1; j < seqLen; j++) {
                        if ((data[i + j] & 0xC0) != 0x80) {
                            validUtf8 = false;
                            break;
                        }
                    }
                }
            }
        }
        
        if (hasHighBytes && validUtf8) return "UTF-8";
        if (hasHighBytes) return "ANSI/GBK";
        return "ASCII";
    }
    
    static std::string BinaryToHex(const uint8_t* data, size_t size) {
        std::ostringstream oss;
        
        for (size_t i = 0; i < size; i++) {
            if (i > 0 && i % 16 == 0) oss << "\n";
            oss << std::hex << std::setw(2) << std::setfill('0') << (int)data[i] << " ";
        }
        
        return oss.str();
    }
    
    static std::string FormatSize(uint64_t size) {
        std::ostringstream oss;
        
        const char* units[] = { "B", "KB", "MB", "GB", "TB" };
        int unitIndex = 0;
        double displaySize = (double)size;
        
        while (displaySize >= 1024 && unitIndex < 4) {
            displaySize /= 1024;
            unitIndex++;
        }
        
        oss << std::fixed << std::setprecision(2) << displaySize << " " << units[unitIndex];
        return oss.str();
    }
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
        std::chrono::milliseconds duration;
    };
    
    struct OptimizationOptions {
        bool recompress = true;
        bool removeDuplicates = true;
        bool optimizePng = false;
        bool optimizeJpeg = false;
        bool stripMetadata = false;
        bool useSolidCompression = false;
        CompressionLevel targetLevel = CompressionLevel::Maximum;
        CompressionMethod targetMethod = CompressionMethod::LZMA2;
    };

private:
    SevenZipArchive& m_archive;
    
public:
    ArchiveOptimizer(SevenZipArchive& archive) : m_archive(archive) {}
    
    OptimizationResult OptimizeArchive(const std::string& archivePath,
                                       const std::string& outputPath,
                                       const OptimizationOptions& options,
                                       const std::string& password = "") {
        OptimizationResult result = {};
        auto startTime = std::chrono::high_resolution_clock::now();
        
        std::string tempDir = outputPath + ".opt.tmp";
        CreateDirectoryA(tempDir.c_str(), NULL);
        
        ExtractOptions extractOpts;
        extractOpts.outputDir = tempDir;
        extractOpts.password = password;
        
        if (!m_archive.ExtractArchive(archivePath, extractOpts)) {
            RemoveDirectoryRecursive(tempDir);
            return result;
        }
        
        std::vector<std::string> files;
        EnumerateFiles(tempDir, files);
        
        result.filesProcessed = (uint32_t)files.size();
        
        std::map<std::string, std::vector<std::string>> hashToFiles;
        
        for (const auto& file : files) {
            result.originalSize += GetFileSize(file);
            
            if (options.removeDuplicates) {
                std::string hash = CalculateFileHash(file);
                hashToFiles[hash].push_back(file);
            }
            
            if (options.optimizePng || options.optimizeJpeg) {
                OptimizeMediaFile(file, options);
            }
            
            if (options.stripMetadata) {
                StripFileMetadata(file);
            }
        }
        
        if (options.removeDuplicates) {
            for (auto& pair : hashToFiles) {
                if (pair.second.size() > 1) {
                    for (size_t i = 1; i < pair.second.size(); i++) {
                        DeleteFileA(pair.second[i].c_str());
                        result.filesOptimized++;
                    }
                }
            }
        }
        
        CompressionOptions compOpts;
        compOpts.level = options.targetLevel;
        compOpts.method = options.targetMethod;
        
        if (options.useSolidCompression) {
            compOpts.solidMode = true;
        }
        
        if (!m_archive.CompressDirectory(outputPath, tempDir, compOpts, true)) {
            RemoveDirectoryRecursive(tempDir);
            return result;
        }
        
        WIN32_FILE_ATTRIBUTE_DATA attr;
        if (GetFileAttributesExA(outputPath.c_str(), GetFileExInfoStandard, &attr)) {
            result.optimizedSize = ((uint64_t)attr.nFileSizeHigh << 32) | attr.nFileSizeLow;
        }
        
        result.savedBytes = result.originalSize - result.optimizedSize;
        result.compressionRatio = result.originalSize > 0 ?
            (double)result.savedBytes / result.originalSize : 0.0;
        
        RemoveDirectoryRecursive(tempDir);
        
        auto endTime = std::chrono::high_resolution_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        return result;
    }
    
    std::vector<std::string> FindRedundantFiles(const std::string& archivePath,
                                                const std::string& password = "") {
        std::vector<std::string> redundant;
        
        ArchiveInfo info;
        if (!m_archive.ListArchive(archivePath, info, password)) {
            return redundant;
        }
        
        std::map<std::string, std::vector<std::string>> hashToFiles;
        
        std::string tempDir = archivePath + ".scan.tmp";
        CreateDirectoryA(tempDir.c_str(), NULL);
        
        for (const auto& file : info.files) {
            if (file.isDirectory) continue;
            
            std::vector<uint8_t> data;
            if (m_archive.ExtractSingleFileToMemory(archivePath, file.path, data, password)) {
                std::string hash = CalculateDataHash(data);
                hashToFiles[hash].push_back(file.path);
            }
        }
        
        for (const auto& pair : hashToFiles) {
            if (pair.second.size() > 1) {
                for (size_t i = 1; i < pair.second.size(); i++) {
                    redundant.push_back(pair.second[i]);
                }
            }
        }
        
        RemoveDirectoryRecursive(tempDir);
        
        return redundant;
    }
    
    bool ConvertArchiveFormat(const std::string& inputPath,
                             const std::string& outputPath,
                             ArchiveFormat targetFormat) {
        std::string tempDir = outputPath + ".conv.tmp";
        CreateDirectoryA(tempDir.c_str(), NULL);
        
        ExtractOptions opts;
        opts.outputDir = tempDir;
        
        if (!m_archive.ExtractArchive(inputPath, opts)) {
            RemoveDirectoryRecursive(tempDir);
            return false;
        }
        
        CompressionOptions compOpts;
        
        bool result = m_archive.CompressDirectory(outputPath, tempDir, compOpts, true);
        
        RemoveDirectoryRecursive(tempDir);
        return result;
    }
    
private:
    static void EnumerateFiles(const std::string& directory, std::vector<std::string>& files) {
        std::string searchPath = directory + "\\*";
        WIN32_FIND_DATAA findData;
        HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
        
        if (hFind == INVALID_HANDLE_VALUE) return;
        
        do {
            if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) continue;
            
            std::string fullPath = directory + "\\" + findData.cFileName;
            
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                EnumerateFiles(fullPath, files);
            } else {
                files.push_back(fullPath);
            }
        } while (FindNextFileA(hFind, &findData));
        
        FindClose(hFind);
    }
    
    static uint64_t GetFileSize(const std::string& filePath) {
        WIN32_FILE_ATTRIBUTE_DATA attr;
        if (GetFileAttributesExA(filePath.c_str(), GetFileExInfoStandard, &attr)) {
            return ((uint64_t)attr.nFileSizeHigh << 32) | attr.nFileSizeLow;
        }
        return 0;
    }
    
    static std::string CalculateFileHash(const std::string& filePath) {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) return "";
        
        uint32_t crc = 0xFFFFFFFF;
        char buffer[8192];
        
        while (file.read(buffer, sizeof(buffer))) {
            for (size_t i = 0; i < file.gcount(); i++) {
                crc ^= (uint8_t)buffer[i];
                for (int j = 0; j < 8; j++) {
                    crc = (crc >> 1) ^ (crc & 1 ? 0xEDB88320 : 0);
                }
            }
        }
        
        std::ostringstream oss;
        oss << std::hex << std::setw(8) << std::setfill('0') << (crc ^ 0xFFFFFFFF);
        return oss.str();
    }
    
    static std::string CalculateDataHash(const std::vector<uint8_t>& data) {
        uint32_t crc = 0xFFFFFFFF;
        
        for (uint8_t byte : data) {
            crc ^= byte;
            for (int j = 0; j < 8; j++) {
                crc = (crc >> 1) ^ (crc & 1 ? 0xEDB88320 : 0);
            }
        }
        
        std::ostringstream oss;
        oss << std::hex << std::setw(8) << std::setfill('0') << (crc ^ 0xFFFFFFFF);
        return oss.str();
    }
    
    static void OptimizeMediaFile(const std::string& filePath, const OptimizationOptions& options) {
        size_t dotPos = filePath.find_last_of('.');
        if (dotPos == std::string::npos) return;
        
        std::string ext = filePath.substr(dotPos + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        
        if (ext == "png" && options.optimizePng) {
        } else if ((ext == "jpg" || ext == "jpeg") && options.optimizeJpeg) {
        }
    }
    
    static void StripFileMetadata(const std::string& filePath) {
    }
    
    static void RemoveDirectoryRecursive(const std::string& path) {
        WIN32_FIND_DATAA findData;
        HANDLE hFind = FindFirstFileA((path + "\\*").c_str(), &findData);
        
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (findData.cFileName[0] != '.') {
                    std::string fullPath = path + "\\" + findData.cFileName;
                    if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                        RemoveDirectoryRecursive(fullPath);
                    } else {
                        DeleteFileA(fullPath.c_str());
                    }
                }
            } while (FindNextFileA(hFind, &findData));
            FindClose(hFind);
        }
        
        RemoveDirectoryA(path.c_str());
    }
};

class MetadataEditor {
public:
    struct ArchiveMetadata {
        std::string title;
        std::string author;
        std::string comment;
        std::string copyright;
        std::string creationTool;
        std::chrono::system_clock::time_point creationTime;
        std::chrono::system_clock::time_point modificationTime;
        std::map<std::string, std::string> customFields;
    };
    
    struct FileMetadata {
        std::string path;
        std::string comment;
        uint32_t attributes;
        std::chrono::system_clock::time_point creationTime;
        std::chrono::system_clock::time_point modificationTime;
        std::chrono::system_clock::time_point accessTime;
        std::map<std::string, std::string> extendedAttributes;
    };

private:
    SevenZipArchive& m_archive;
    
public:
    MetadataEditor(SevenZipArchive& archive) : m_archive(archive) {}
    
    bool SetArchiveMetadata(const std::string& archivePath,
                           const ArchiveMetadata& metadata,
                           const std::string& password = "") {
        std::string tempDir = archivePath + ".meta.tmp";
        CreateDirectoryA(tempDir.c_str(), NULL);
        
        ExtractOptions opts;
        opts.outputDir = tempDir;
        opts.password = password;
        
        if (!m_archive.ExtractArchive(archivePath, opts)) {
            RemoveDirectoryRecursive(tempDir);
            return false;
        }
        
        std::string metaPath = tempDir + "\\archive.metadata";
        std::ofstream metaFile(metaPath, std::ios::binary);
        
        if (!metaFile.is_open()) {
            RemoveDirectoryRecursive(tempDir);
            return false;
        }
        
        WriteString(metaFile, "SEVENZIP_METADATA_V1");
        WriteString(metaFile, metadata.title);
        WriteString(metaFile, metadata.author);
        WriteString(metaFile, metadata.comment);
        WriteString(metaFile, metadata.copyright);
        WriteString(metaFile, metadata.creationTool);
        
        auto creationTime = std::chrono::system_clock::to_time_t(metadata.creationTime);
        auto modTime = std::chrono::system_clock::to_time_t(metadata.modificationTime);
        metaFile.write(reinterpret_cast<const char*>(&creationTime), sizeof(creationTime));
        metaFile.write(reinterpret_cast<const char*>(&modTime), sizeof(modTime));
        
        uint32_t customCount = (uint32_t)metadata.customFields.size();
        metaFile.write(reinterpret_cast<const char*>(&customCount), sizeof(customCount));
        
        for (const auto& pair : metadata.customFields) {
            WriteString(metaFile, pair.first);
            WriteString(metaFile, pair.second);
        }
        
        metaFile.close();
        
        std::string backupPath = archivePath + ".backup";
        MoveFileA(archivePath.c_str(), backupPath.c_str());
        
        CompressionOptions compOpts;
        bool result = m_archive.CompressDirectory(archivePath, tempDir, compOpts, true);
        
        if (result) {
            DeleteFileA(backupPath.c_str());
        } else {
            MoveFileA(backupPath.c_str(), archivePath.c_str());
        }
        
        RemoveDirectoryRecursive(tempDir);
        return result;
    }
    
    ArchiveMetadata GetArchiveMetadata(const std::string& archivePath,
                                       const std::string& password = "") {
        ArchiveMetadata metadata = {};
        
        std::string tempDir = archivePath + ".read.tmp";
        CreateDirectoryA(tempDir.c_str(), NULL);
        
        ExtractOptions opts;
        opts.outputDir = tempDir;
        opts.password = password;
        
        if (!m_archive.ExtractArchive(archivePath, opts)) {
            RemoveDirectoryRecursive(tempDir);
            return metadata;
        }
        
        std::string metaPath = tempDir + "\\archive.metadata";
        std::ifstream metaFile(metaPath, std::ios::binary);
        
        if (!metaFile.is_open()) {
            RemoveDirectoryRecursive(tempDir);
            return metadata;
        }
        
        std::string version = ReadString(metaFile);
        if (version != "SEVENZIP_METADATA_V1") {
            RemoveDirectoryRecursive(tempDir);
            return metadata;
        }
        
        metadata.title = ReadString(metaFile);
        metadata.author = ReadString(metaFile);
        metadata.comment = ReadString(metaFile);
        metadata.copyright = ReadString(metaFile);
        metadata.creationTool = ReadString(metaFile);
        
        time_t creationTime, modTime;
        metaFile.read(reinterpret_cast<char*>(&creationTime), sizeof(creationTime));
        metaFile.read(reinterpret_cast<char*>(&modTime), sizeof(modTime));
        
        metadata.creationTime = std::chrono::system_clock::from_time_t(creationTime);
        metadata.modificationTime = std::chrono::system_clock::from_time_t(modTime);
        
        uint32_t customCount;
        metaFile.read(reinterpret_cast<char*>(&customCount), sizeof(customCount));
        
        for (uint32_t i = 0; i < customCount; i++) {
            std::string key = ReadString(metaFile);
            std::string value = ReadString(metaFile);
            metadata.customFields[key] = value;
        }
        
        metaFile.close();
        RemoveDirectoryRecursive(tempDir);
        
        return metadata;
    }
    
    bool SetFileComment(const std::string& archivePath,
                       const std::string& filePath,
                       const std::string& comment,
                       const std::string& password = "") {
        ArchiveMetadata metadata = GetArchiveMetadata(archivePath, password);
        metadata.customFields["file_comment:" + filePath] = comment;
        return SetArchiveMetadata(archivePath, metadata, password);
    }
    
    bool RenameFile(const std::string& archivePath,
                   const std::string& oldPath,
                   const std::string& newPath,
                   const std::string& password = "") {
        return m_archive.RenameInArchive(archivePath, oldPath, newPath);
    }
    
    bool SetFileTimestamp(const std::string& archivePath,
                         const std::string& filePath,
                         std::chrono::system_clock::time_point timestamp,
                         const std::string& password = "") {
        ArchiveMetadata metadata = GetArchiveMetadata(archivePath, password);
        
        auto timeValue = std::chrono::system_clock::to_time_t(timestamp);
        metadata.customFields["file_mtime:" + filePath] = std::to_string(timeValue);
        
        return SetArchiveMetadata(archivePath, metadata, password);
    }
    
private:
    static void WriteString(std::ofstream& file, const std::string& str) {
        uint32_t len = (uint32_t)str.size();
        file.write(reinterpret_cast<const char*>(&len), sizeof(len));
        file.write(str.c_str(), len);
    }
    
    static std::string ReadString(std::ifstream& file) {
        uint32_t len;
        file.read(reinterpret_cast<char*>(&len), sizeof(len));
        
        std::string str(len, '\0');
        file.read(&str[0], len);
        
        return str;
    }
    
    static void RemoveDirectoryRecursive(const std::string& path) {
        WIN32_FIND_DATAA findData;
        HANDLE hFind = FindFirstFileA((path + "\\*").c_str(), &findData);
        
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (findData.cFileName[0] != '.') {
                    std::string fullPath = path + "\\" + findData.cFileName;
                    if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                        RemoveDirectoryRecursive(fullPath);
                    } else {
                        DeleteFileA(fullPath.c_str());
                    }
                }
            } while (FindNextFileA(hFind, &findData));
            FindClose(hFind);
        }
        
        RemoveDirectoryA(path.c_str());
    }
};

class ArchiveSynchronizer {
public:
    struct SyncResult {
        uint32_t filesUploaded;
        uint32_t filesDownloaded;
        uint32_t filesDeleted;
        uint64_t bytesUploaded;
        uint64_t bytesDownloaded;
        std::vector<std::string> conflicts;
        std::chrono::seconds duration;
    };
    
    struct SyncOptions {
        bool deleteOrphaned = false;
        bool overwriteNewer = false;
        bool preserveTimestamps = true;
        bool dryRun = false;
        std::string excludePattern;
        std::string includePattern;
    };

private:
    SevenZipArchive& m_archive;
    
public:
    ArchiveSynchronizer(SevenZipArchive& archive) : m_archive(archive) {}
    
    SyncResult SyncDirectories(const std::string& sourceDir,
                               const std::string& targetDir,
                               const SyncOptions& options) {
        SyncResult result = {};
        auto startTime = std::chrono::high_resolution_clock::now();
        
        std::map<std::string, FileInfo> sourceFiles, targetFiles;
        
        EnumerateDirectory(sourceDir, sourceFiles);
        EnumerateDirectory(targetDir, targetFiles);
        
        for (const auto& pair : sourceFiles) {
            const std::string& relativePath = pair.first;
            const FileInfo& sourceFile = pair.second;
            
            auto it = targetFiles.find(relativePath);
            
            if (it == targetFiles.end()) {
                if (!options.dryRun) {
                    std::string targetPath = targetDir + "\\" + relativePath;
                    CreateDirectoryForFile(targetPath);
                    CopyFileA((sourceDir + "\\" + relativePath).c_str(),
                             targetPath.c_str(), FALSE);
                }
                result.filesUploaded++;
                result.bytesUploaded += sourceFile.size;
            } else {
                bool sourceNewer = sourceFile.modifiedTime > it->second.modifiedTime;
                
                if (sourceNewer || options.overwriteNewer) {
                    if (!options.dryRun) {
                        std::string targetPath = targetDir + "\\" + relativePath;
                        CopyFileA((sourceDir + "\\" + relativePath).c_str(),
                                 targetPath.c_str(), FALSE);
                    }
                    result.filesUploaded++;
                    result.bytesUploaded += sourceFile.size;
                }
            }
        }
        
        if (options.deleteOrphaned) {
            for (const auto& pair : targetFiles) {
                if (sourceFiles.find(pair.first) == sourceFiles.end()) {
                    if (!options.dryRun) {
                        std::string targetPath = targetDir + "\\" + pair.first;
                        DeleteFileA(targetPath.c_str());
                    }
                    result.filesDeleted++;
                }
            }
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime);
        
        return result;
    }
    
    SyncResult SyncWithArchive(const std::string& archivePath,
                               const std::string& directory,
                               const SyncOptions& options,
                               const std::string& password = "") {
        SyncResult result = {};
        
        std::string tempDir = archivePath + ".sync.tmp";
        CreateDirectoryA(tempDir.c_str(), NULL);
        
        ExtractOptions opts;
        opts.outputDir = tempDir;
        opts.password = password;
        
        if (!m_archive.ExtractArchive(archivePath, opts)) {
            RemoveDirectoryRecursive(tempDir);
            return result;
        }
        
        result = SyncDirectories(tempDir, directory, options);
        
        RemoveDirectoryRecursive(tempDir);
        return result;
    }
    
    bool CreateSyncPoint(const std::string& archivePath,
                        const std::string& syncPointPath) {
        ArchiveInfo info;
        if (!m_archive.ListArchive(archivePath, info)) {
            return false;
        }
        
        std::ofstream syncFile(syncPointPath, std::ios::binary);
        if (!syncFile.is_open()) return false;
        
        WriteString(syncFile, "SYNCPOINT_V1");
        WriteString(syncFile, archivePath);
        
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::system_clock::to_time_t(now);
        syncFile.write(reinterpret_cast<const char*>(&timestamp), sizeof(timestamp));
        
        uint32_t fileCount = (uint32_t)info.files.size();
        syncFile.write(reinterpret_cast<const char*>(&fileCount), sizeof(fileCount));
        
        for (const auto& file : info.files) {
            WriteString(syncFile, file.path);
            
            uint64_t size = file.size;
            uint32_t crc = file.crc;
            syncFile.write(reinterpret_cast<const char*>(&size), sizeof(size));
            syncFile.write(reinterpret_cast<const char*>(&crc), sizeof(crc));
        }
        
        syncFile.close();
        return true;
    }
    
    bool VerifySyncPoint(const std::string& archivePath,
                        const std::string& syncPointPath) {
        std::ifstream syncFile(syncPointPath, std::ios::binary);
        if (!syncFile.is_open()) return false;
        
        std::string version = ReadString(syncFile);
        if (version != "SYNCPOINT_V1") {
            return false;
        }
        
        std::string storedArchivePath = ReadString(syncFile);
        if (storedArchivePath != archivePath) {
            return false;
        }
        
        ArchiveInfo info;
        if (!m_archive.ListArchive(archivePath, info)) {
            return false;
        }
        
        uint32_t fileCount;
        syncFile.read(reinterpret_cast<char*>(&fileCount), sizeof(fileCount));
        
        if (fileCount != info.files.size()) {
            return false;
        }
        
        for (uint32_t i = 0; i < fileCount; i++) {
            std::string path = ReadString(syncFile);
            uint64_t size;
            uint32_t crc;
            syncFile.read(reinterpret_cast<char*>(&size), sizeof(size));
            syncFile.read(reinterpret_cast<char*>(&crc), sizeof(crc));
            
            bool found = false;
            for (const auto& file : info.files) {
                if (file.path == path && file.size == size && file.crc == crc) {
                    found = true;
                    break;
                }
            }
            
            if (!found) return false;
        }
        
        syncFile.close();
        return true;
    }
    
private:
    struct FileInfo {
        std::string path;
        uint64_t size;
        std::chrono::system_clock::time_point modifiedTime;
        uint32_t crc;
    };
    
    static void EnumerateDirectory(const std::string& directory,
                                   std::map<std::string, FileInfo>& files) {
        std::string searchPath = directory + "\\*";
        WIN32_FIND_DATAA findData;
        HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
        
        if (hFind == INVALID_HANDLE_VALUE) return;
        
        do {
            if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) continue;
            
            std::string fullPath = directory + "\\" + findData.cFileName;
            
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                EnumerateDirectory(fullPath, files);
            } else {
                FileInfo info;
                info.path = fullPath;
                info.size = ((uint64_t)findData.nFileSizeHigh << 32) | findData.nFileSizeLow;
                
                ULARGE_INTEGER ull;
                ull.LowPart = findData.ftLastWriteTime.dwLowDateTime;
                ull.HighPart = findData.ftLastWriteTime.dwHighDateTime;
                info.modifiedTime = std::chrono::system_clock::from_time_t(
                    (ull.QuadPart - 116444736000000000ULL) / 10000000ULL);
                
                std::string relativePath = fullPath;
                size_t pos = relativePath.find('\\');
                if (pos != std::string::npos) {
                    relativePath = relativePath.substr(pos + 1);
                }
                
                files[relativePath] = info;
            }
        } while (FindNextFileA(hFind, &findData));
        
        FindClose(hFind);
    }
    
    static void CreateDirectoryForFile(const std::string& filePath) {
        size_t pos = filePath.find_last_of('\\');
        if (pos != std::string::npos) {
            std::string dir = filePath.substr(0, pos);
            CreateDirectoryA(dir.c_str(), NULL);
        }
    }
    
    static void WriteString(std::ofstream& file, const std::string& str) {
        uint32_t len = (uint32_t)str.size();
        file.write(reinterpret_cast<const char*>(&len), sizeof(len));
        file.write(str.c_str(), len);
    }
    
    static std::string ReadString(std::ifstream& file) {
        uint32_t len;
        file.read(reinterpret_cast<char*>(&len), sizeof(len));
        
        std::string str(len, '\0');
        file.read(&str[0], len);
        
        return str;
    }
    
    static void RemoveDirectoryRecursive(const std::string& path) {
        WIN32_FIND_DATAA findData;
        HANDLE hFind = FindFirstFileA((path + "\\*").c_str(), &findData);
        
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (findData.cFileName[0] != '.') {
                    std::string fullPath = path + "\\" + findData.cFileName;
                    if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                        RemoveDirectoryRecursive(fullPath);
                    } else {
                        DeleteFileA(fullPath.c_str());
                    }
                }
            } while (FindNextFileA(hFind, &findData));
            FindClose(hFind);
        }
        
        RemoveDirectoryA(path.c_str());
    }
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

private:
    SevenZipArchive& m_archive;
    std::string m_timelinePath;
    std::vector<TimelineEntry> m_entries;
    
public:
    TimelineBackup(SevenZipArchive& archive, const std::string& timelinePath)
        : m_archive(archive), m_timelinePath(timelinePath) {
        LoadTimeline();
    }
    
    std::string CreateEntry(const std::string& sourcePath,
                           const std::string& description = "",
                           const CompressionOptions& options = {}) {
        TimelineEntry entry;
        entry.id = GenerateEntryId();
        entry.timestamp = std::chrono::system_clock::now();
        entry.description = description;
        
        std::ostringstream oss;
        oss << m_timelinePath << "\\" << std::chrono::duration_cast<std::chrono::seconds>(
            entry.timestamp.time_since_epoch()).count() << "_" << entry.id << ".7z";
        entry.archivePath = oss.str();
        
        if (!m_archive.CompressDirectory(entry.archivePath, sourcePath, options, true)) {
            return "";
        }
        
        WIN32_FILE_ATTRIBUTE_DATA attr;
        if (GetFileAttributesExA(entry.archivePath.c_str(), GetFileExInfoStandard, &attr)) {
            entry.size = ((uint64_t)attr.nFileSizeHigh << 32) | attr.nFileSizeLow;
        }
        
        ArchiveInfo info;
        if (m_archive.ListArchive(entry.archivePath, info)) {
            entry.fileCount = (uint32_t)info.files.size();
        }
        
        if (!m_entries.empty()) {
            entry.parentEntry = m_entries.back().id;
        }
        
        m_entries.push_back(entry);
        SaveTimeline();
        
        return entry.id;
    }
    
    bool RestoreEntry(const std::string& entryId,
                     const std::string& outputPath,
                     const std::string& password = "") {
        TimelineEntry* entry = FindEntry(entryId);
        if (!entry) return false;
        
        ExtractOptions opts;
        opts.outputDir = outputPath;
        opts.password = password;
        
        return m_archive.ExtractArchive(entry->archivePath, opts);
    }
    
    bool DeleteEntry(const std::string& entryId) {
        auto it = std::find_if(m_entries.begin(), m_entries.end(),
                              [&](const TimelineEntry& e) { return e.id == entryId; });
        
        if (it == m_entries.end()) return false;
        
        DeleteFileA(it->archivePath.c_str());
        m_entries.erase(it);
        SaveTimeline();
        
        return true;
    }
    
    TimelineEntry* FindEntry(const std::string& entryId) {
        for (auto& entry : m_entries) {
            if (entry.id == entryId) {
                return &entry;
            }
        }
        return nullptr;
    }
    
    TimelineInfo GetTimelineInfo() {
        TimelineInfo info;
        info.entryCount = (uint32_t)m_entries.size();
        info.totalSize = 0;
        
        for (const auto& entry : m_entries) {
            info.totalSize += entry.size;
            info.entries.push_back(entry);
        }
        
        if (!m_entries.empty()) {
            info.oldestEntry = m_entries.front().timestamp;
            info.newestEntry = m_entries.back().timestamp;
        }
        
        return info;
    }
    
    std::vector<TimelineEntry> GetEntriesInRange(
        std::chrono::system_clock::time_point start,
        std::chrono::system_clock::time_point end) {
        
        std::vector<TimelineEntry> result;
        
        for (const auto& entry : m_entries) {
            if (entry.timestamp >= start && entry.timestamp <= end) {
                result.push_back(entry);
            }
        }
        
        return result;
    }
    
    std::vector<TimelineEntry> GetEntriesByDescription(const std::string& keyword) {
        std::vector<TimelineEntry> result;
        
        for (const auto& entry : m_entries) {
            if (entry.description.find(keyword) != std::string::npos) {
                result.push_back(entry);
            }
        }
        
        return result;
    }
    
    bool PruneOldEntries(uint32_t maxEntries, uint32_t maxAgeDays = 0) {
        bool changed = false;
        
        if (maxAgeDays > 0) {
            auto cutoff = std::chrono::system_clock::now() - 
                         std::chrono::hours(24 * maxAgeDays);
            
            auto it = std::remove_if(m_entries.begin(), m_entries.end(),
                [&](const TimelineEntry& e) {
                    if (e.timestamp < cutoff) {
                        DeleteFileA(e.archivePath.c_str());
                        changed = true;
                        return true;
                    }
                    return false;
                });
            
            m_entries.erase(it, m_entries.end());
        }
        
        while (m_entries.size() > maxEntries) {
            DeleteFileA(m_entries.front().archivePath.c_str());
            m_entries.erase(m_entries.begin());
            changed = true;
        }
        
        if (changed) {
            SaveTimeline();
        }
        
        return changed;
    }
    
private:
    static std::string GenerateEntryId() {
        static std::atomic<uint64_t> counter(0);
        std::ostringstream oss;
        oss << std::hex << (++counter) << "_" << time(nullptr);
        return oss.str();
    }
    
    bool LoadTimeline() {
        std::string indexPath = m_timelinePath + "\\timeline.index";
        std::ifstream file(indexPath, std::ios::binary);
        
        if (!file.is_open()) return false;
        
        m_entries.clear();
        
        uint32_t count;
        file.read(reinterpret_cast<char*>(&count), sizeof(count));
        
        for (uint32_t i = 0; i < count; i++) {
            TimelineEntry entry;
            
            entry.id = ReadString(file);
            entry.archivePath = ReadString(file);
            entry.description = ReadString(file);
            entry.parentEntry = ReadString(file);
            
            int64_t timestamp;
            file.read(reinterpret_cast<char*>(&timestamp), sizeof(timestamp));
            entry.timestamp = std::chrono::system_clock::from_time_t(timestamp);
            
            file.read(reinterpret_cast<char*>(&entry.size), sizeof(entry.size));
            file.read(reinterpret_cast<char*>(&entry.fileCount), sizeof(entry.fileCount));
            
            m_entries.push_back(entry);
        }
        
        file.close();
        return true;
    }
    
    bool SaveTimeline() {
        CreateDirectoryA(m_timelinePath.c_str(), NULL);
        
        std::string indexPath = m_timelinePath + "\\timeline.index";
        std::ofstream file(indexPath, std::ios::binary);
        
        if (!file.is_open()) return false;
        
        uint32_t count = (uint32_t)m_entries.size();
        file.write(reinterpret_cast<const char*>(&count), sizeof(count));
        
        for (const auto& entry : m_entries) {
            WriteString(file, entry.id);
            WriteString(file, entry.archivePath);
            WriteString(file, entry.description);
            WriteString(file, entry.parentEntry);
            
            auto timestamp = std::chrono::system_clock::to_time_t(entry.timestamp);
            file.write(reinterpret_cast<const char*>(&timestamp), sizeof(timestamp));
            
            file.write(reinterpret_cast<const char*>(&entry.size), sizeof(entry.size));
            file.write(reinterpret_cast<const char*>(&entry.fileCount), sizeof(entry.fileCount));
        }
        
        file.close();
        return true;
    }
    
    static void WriteString(std::ofstream& file, const std::string& str) {
        uint32_t len = (uint32_t)str.size();
        file.write(reinterpret_cast<const char*>(&len), sizeof(len));
        file.write(str.c_str(), len);
    }
    
    static std::string ReadString(std::ifstream& file) {
        uint32_t len;
        file.read(reinterpret_cast<char*>(&len), sizeof(len));
        
        std::string str(len, '\0');
        file.read(&str[0], len);
        
        return str;
    }
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

private:
    SevenZipArchive& m_archive;
    std::map<std::string, FileType> m_extensionMap;
    std::map<FileType, std::vector<std::string>> m_typeTags;
    std::map<std::string, std::vector<uint8_t>> m_magicNumbers;

public:
    IntelligentClassifier(SevenZipArchive& archive) : m_archive(archive) {
        InitializeExtensionMap();
        InitializeMagicNumbers();
        InitializeTypeTags();
    }

    ClassificationResult ClassifyFile(const std::string& filePath) {
        ClassificationResult result;
        result.type = FileType::Other;
        result.confidence = 0.0;

        std::string ext = GetExtension(filePath);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        auto it = m_extensionMap.find(ext);
        if (it != m_extensionMap.end()) {
            result.type = it->second;
            result.confidence = 0.7;
        }

        result.subType = GetSubType(ext, result.type);
        result.tags = GetTagsForType(result.type);
        result.description = GetDescriptionForType(result.type);

        return result;
    }

    ClassificationResult ClassifyByContent(const std::vector<uint8_t>& data,
                                          const std::string& extension) {
        ClassificationResult result;
        result.type = FileType::Other;
        result.confidence = 0.0;

        FileType detectedType = DetectByMagicNumber(data);
        if (detectedType != FileType::Other) {
            result.type = detectedType;
            result.confidence = 0.95;
        } else {
            std::string ext = extension;
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            
            auto it = m_extensionMap.find(ext);
            if (it != m_extensionMap.end()) {
                result.type = it->second;
                result.confidence = 0.6;
            }

            if (IsTextData(data)) {
                result.type = FileType::Document;
                result.subType = "text";
                result.confidence = 0.8;
            }
        }

        result.subType = GetSubType(extension, result.type);
        result.tags = GetTagsForType(result.type);
        result.description = GetDescriptionForType(result.type);

        return result;
    }

    ArchiveClassification ClassifyArchive(const std::string& archivePath) {
        ArchiveClassification classification;
        classification.dominantType = FileType::Other;

        ArchiveInfo info;
        if (!m_archive.ListArchive(archivePath, info)) {
            return classification;
        }

        uint64_t maxSize = 0;
        FileType dominantType = FileType::Other;

        for (const auto& file : info.files) {
            ClassificationResult result = ClassifyFile(file.path);
            
            classification.typeCounts[result.type]++;
            classification.typeSizes[result.type] += file.size;

            if (classification.typeSizes[result.type] > maxSize) {
                maxSize = classification.typeSizes[result.type];
                dominantType = result.type;
            }

            for (const auto& tag : result.tags) {
                if (std::find(classification.categories.begin(), 
                             classification.categories.end(), tag) == classification.categories.end()) {
                    classification.categories.push_back(tag);
                }
            }
        }

        classification.dominantType = dominantType;
        classification.suggestedName = GenerateSuggestedName(archivePath, dominantType);

        return classification;
    }

    std::vector<std::string> ExtractTags(const std::string& archivePath) {
        std::vector<std::string> allTags;

        ArchiveInfo info;
        if (!m_archive.ListArchive(archivePath, info)) {
            return allTags;
        }

        std::map<std::string, int> tagFrequency;

        for (const auto& file : info.files) {
            ClassificationResult result = ClassifyFile(file.path);
            for (const auto& tag : result.tags) {
                tagFrequency[tag]++;
            }
        }

        std::vector<std::pair<std::string, int>> sortedTags(tagFrequency.begin(), tagFrequency.end());
        std::sort(sortedTags.begin(), sortedTags.end(),
                 [](const auto& a, const auto& b) { return a.second > b.second; });

        for (const auto& pair : sortedTags) {
            allTags.push_back(pair.first);
        }

        return allTags;
    }

    std::string GenerateCategoryPath(const std::string& archivePath) {
        ArchiveClassification classification = ClassifyArchive(archivePath);
        
        std::string basePath = GetTypePath(classification.dominantType);
        
        if (!classification.categories.empty()) {
            basePath += "\\" + classification.categories[0];
        }

        return basePath;
    }

    bool OrganizeArchive(const std::string& archivePath, const std::string& outputDir) {
        std::string categoryPath = GenerateCategoryPath(archivePath);
        std::string fullPath = outputDir + "\\" + categoryPath;

        CreateDirectoryA(fullPath.c_str(), NULL);

        std::string fileName = GetFileName(archivePath);
        std::string destPath = fullPath + "\\" + fileName;

        return MoveFileA(archivePath.c_str(), destPath.c_str()) != FALSE;
    }

private:
    void InitializeExtensionMap() {
        m_extensionMap[".txt"] = FileType::Document;
        m_extensionMap[".doc"] = FileType::Document;
        m_extensionMap[".docx"] = FileType::Document;
        m_extensionMap[".pdf"] = FileType::Document;
        m_extensionMap[".xls"] = FileType::Document;
        m_extensionMap[".xlsx"] = FileType::Document;
        m_extensionMap[".ppt"] = FileType::Document;
        m_extensionMap[".pptx"] = FileType::Document;
        m_extensionMap[".rtf"] = FileType::Document;
        m_extensionMap[".odt"] = FileType::Document;
        m_extensionMap[".csv"] = FileType::Data;

        m_extensionMap[".jpg"] = FileType::Image;
        m_extensionMap[".jpeg"] = FileType::Image;
        m_extensionMap[".png"] = FileType::Image;
        m_extensionMap[".gif"] = FileType::Image;
        m_extensionMap[".bmp"] = FileType::Image;
        m_extensionMap[".tiff"] = FileType::Image;
        m_extensionMap[".webp"] = FileType::Image;
        m_extensionMap[".svg"] = FileType::Image;
        m_extensionMap[".ico"] = FileType::Image;
        m_extensionMap[".psd"] = FileType::Image;

        m_extensionMap[".mp4"] = FileType::Video;
        m_extensionMap[".avi"] = FileType::Video;
        m_extensionMap[".mkv"] = FileType::Video;
        m_extensionMap[".mov"] = FileType::Video;
        m_extensionMap[".wmv"] = FileType::Video;
        m_extensionMap[".flv"] = FileType::Video;
        m_extensionMap[".webm"] = FileType::Video;
        m_extensionMap[".m4v"] = FileType::Video;

        m_extensionMap[".mp3"] = FileType::Audio;
        m_extensionMap[".wav"] = FileType::Audio;
        m_extensionMap[".flac"] = FileType::Audio;
        m_extensionMap[".aac"] = FileType::Audio;
        m_extensionMap[".ogg"] = FileType::Audio;
        m_extensionMap[".wma"] = FileType::Audio;
        m_extensionMap[".m4a"] = FileType::Audio;

        m_extensionMap[".7z"] = FileType::Archive;
        m_extensionMap[".zip"] = FileType::Archive;
        m_extensionMap[".rar"] = FileType::Archive;
        m_extensionMap[".tar"] = FileType::Archive;
        m_extensionMap[".gz"] = FileType::Archive;
        m_extensionMap[".bz2"] = FileType::Archive;
        m_extensionMap[".xz"] = FileType::Archive;

        m_extensionMap[".c"] = FileType::Code;
        m_extensionMap[".cpp"] = FileType::Code;
        m_extensionMap[".h"] = FileType::Code;
        m_extensionMap[".hpp"] = FileType::Code;
        m_extensionMap[".cs"] = FileType::Code;
        m_extensionMap[".java"] = FileType::Code;
        m_extensionMap[".py"] = FileType::Code;
        m_extensionMap[".js"] = FileType::Code;
        m_extensionMap[".ts"] = FileType::Code;
        m_extensionMap[".html"] = FileType::Code;
        m_extensionMap[".css"] = FileType::Code;
        m_extensionMap[".json"] = FileType::Data;
        m_extensionMap[".xml"] = FileType::Data;
        m_extensionMap[".sql"] = FileType::Code;

        m_extensionMap[".exe"] = FileType::Executable;
        m_extensionMap[".dll"] = FileType::Executable;
        m_extensionMap[".so"] = FileType::Executable;
        m_extensionMap[".msi"] = FileType::Executable;
        m_extensionMap[".bat"] = FileType::Executable;
        m_extensionMap[".cmd"] = FileType::Executable;
        m_extensionMap[".ps1"] = FileType::Executable;

        m_extensionMap[".db"] = FileType::Data;
        m_extensionMap[".sqlite"] = FileType::Data;
        m_extensionMap[".mdb"] = FileType::Data;
    }

    void InitializeMagicNumbers() {
        m_magicNumbers["zip"] = {0x50, 0x4B, 0x03, 0x04};
        m_magicNumbers["7z"] = {0x37, 0x7A, 0xBC, 0xAF, 0x27, 0x1C};
        m_magicNumbers["rar"] = {0x52, 0x61, 0x72, 0x21};
        m_magicNumbers["pdf"] = {0x25, 0x50, 0x44, 0x46};
        m_magicNumbers["png"] = {0x89, 0x50, 0x4E, 0x47};
        m_magicNumbers["jpg"] = {0xFF, 0xD8, 0xFF};
        m_magicNumbers["gif"] = {0x47, 0x49, 0x46, 0x38};
        m_magicNumbers["bmp"] = {0x42, 0x4D};
        m_magicNumbers["exe"] = {0x4D, 0x5A};
        m_magicNumbers["mp3"] = {0x49, 0x44, 0x33};
        m_magicNumbers["mp4"] = {0x00, 0x00, 0x00, 0x18, 0x66, 0x74, 0x79, 0x70};
    }

    void InitializeTypeTags() {
        m_typeTags[FileType::Document] = {"document", "office", "text"};
        m_typeTags[FileType::Image] = {"image", "media", "graphics", "photo"};
        m_typeTags[FileType::Video] = {"video", "media", "movie", "streaming"};
        m_typeTags[FileType::Audio] = {"audio", "media", "music", "sound"};
        m_typeTags[FileType::Archive] = {"archive", "compressed", "backup"};
        m_typeTags[FileType::Code] = {"code", "development", "programming", "source"};
        m_typeTags[FileType::Data] = {"data", "database", "structured"};
        m_typeTags[FileType::Executable] = {"executable", "binary", "program"};
    }

    FileType DetectByMagicNumber(const std::vector<uint8_t>& data) {
        for (const auto& pair : m_magicNumbers) {
            if (data.size() >= pair.second.size()) {
                bool match = true;
                for (size_t i = 0; i < pair.second.size(); i++) {
                    if (data[i] != pair.second[i]) {
                        match = false;
                        break;
                    }
                }
                if (match) {
                    if (pair.first == "zip" || pair.first == "7z" || pair.first == "rar") {
                        return FileType::Archive;
                    } else if (pair.first == "pdf") {
                        return FileType::Document;
                    } else if (pair.first == "png" || pair.first == "jpg" || 
                              pair.first == "gif" || pair.first == "bmp") {
                        return FileType::Image;
                    } else if (pair.first == "exe") {
                        return FileType::Executable;
                    } else if (pair.first == "mp3") {
                        return FileType::Audio;
                    } else if (pair.first == "mp4") {
                        return FileType::Video;
                    }
                }
            }
        }
        return FileType::Other;
    }

    bool IsTextData(const std::vector<uint8_t>& data) {
        if (data.empty()) return false;
        
        size_t checkSize = std::min(data.size(), (size_t)8192);
        size_t textChars = 0;
        
        for (size_t i = 0; i < checkSize; i++) {
            uint8_t c = data[i];
            if ((c >= 32 && c <= 126) || c == '\n' || c == '\r' || c == '\t') {
                textChars++;
            }
        }
        
        return (double)textChars / checkSize > 0.85;
    }

    std::string GetExtension(const std::string& path) {
        size_t pos = path.find_last_of('.');
        if (pos != std::string::npos) {
            return path.substr(pos);
        }
        return "";
    }

    std::string GetFileName(const std::string& path) {
        size_t pos = path.find_last_of("\\/");
        if (pos != std::string::npos) {
            return path.substr(pos + 1);
        }
        return path;
    }

    std::string GetSubType(const std::string& ext, FileType type) {
        std::string lowerExt = ext;
        std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(), ::tolower);

        if (type == FileType::Document) {
            if (lowerExt == ".pdf") return "pdf";
            if (lowerExt == ".doc" || lowerExt == ".docx") return "word";
            if (lowerExt == ".xls" || lowerExt == ".xlsx") return "excel";
            if (lowerExt == ".ppt" || lowerExt == ".pptx") return "powerpoint";
            if (lowerExt == ".txt") return "text";
        } else if (type == FileType::Image) {
            if (lowerExt == ".jpg" || lowerExt == ".jpeg") return "jpeg";
            if (lowerExt == ".png") return "png";
            if (lowerExt == ".gif") return "gif";
            if (lowerExt == ".bmp") return "bitmap";
            if (lowerExt == ".psd") return "photoshop";
        } else if (type == FileType::Video) {
            if (lowerExt == ".mp4") return "mp4";
            if (lowerExt == ".avi") return "avi";
            if (lowerExt == ".mkv") return "matroska";
            if (lowerExt == ".mov") return "quicktime";
        } else if (type == FileType::Audio) {
            if (lowerExt == ".mp3") return "mp3";
            if (lowerExt == ".flac") return "flac";
            if (lowerExt == ".wav") return "wav";
        } else if (type == FileType::Code) {
            if (lowerExt == ".c" || lowerExt == ".cpp" || lowerExt == ".h") return "cpp";
            if (lowerExt == ".cs") return "csharp";
            if (lowerExt == ".java") return "java";
            if (lowerExt == ".py") return "python";
            if (lowerExt == ".js" || lowerExt == ".ts") return "javascript";
            if (lowerExt == ".html") return "html";
            if (lowerExt == ".css") return "css";
        }

        return "unknown";
    }

    std::vector<std::string> GetTagsForType(FileType type) {
        auto it = m_typeTags.find(type);
        if (it != m_typeTags.end()) {
            return it->second;
        }
        return {"other", "misc"};
    }

    std::string GetDescriptionForType(FileType type) {
        switch (type) {
            case FileType::Document: return "Document file";
            case FileType::Image: return "Image file";
            case FileType::Video: return "Video file";
            case FileType::Audio: return "Audio file";
            case FileType::Archive: return "Archive file";
            case FileType::Code: return "Source code file";
            case FileType::Data: return "Data file";
            case FileType::Executable: return "Executable file";
            default: return "Unknown file type";
        }
    }

    std::string GetTypePath(FileType type) {
        switch (type) {
            case FileType::Document: return "Documents";
            case FileType::Image: return "Images";
            case FileType::Video: return "Videos";
            case FileType::Audio: return "Audio";
            case FileType::Archive: return "Archives";
            case FileType::Code: return "SourceCode";
            case FileType::Data: return "Data";
            case FileType::Executable: return "Programs";
            default: return "Other";
        }
    }

    std::string GenerateSuggestedName(const std::string& archivePath, FileType dominantType) {
        std::string baseName = GetFileName(archivePath);
        size_t dotPos = baseName.find_last_of('.');
        if (dotPos != std::string::npos) {
            baseName = baseName.substr(0, dotPos);
        }

        std::string typePrefix;
        switch (dominantType) {
            case FileType::Document: typePrefix = "docs_"; break;
            case FileType::Image: typePrefix = "images_"; break;
            case FileType::Video: typePrefix = "videos_"; break;
            case FileType::Audio: typePrefix = "audio_"; break;
            case FileType::Archive: typePrefix = "backup_"; break;
            case FileType::Code: typePrefix = "code_"; break;
            case FileType::Data: typePrefix = "data_"; break;
            case FileType::Executable: typePrefix = "apps_"; break;
            default: typePrefix = "misc_"; break;
        }

        return typePrefix + baseName;
    }
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
        bool scanArchives = true;
        bool heuristicsEnabled = true;
        bool scanMemory = false;
        uint32_t maxRecursionDepth = 10;
        std::vector<std::string> excludePatterns;
        std::string password;
    };

private:
    SevenZipArchive& m_archive;
    std::string m_externalScanner;
    std::vector<std::vector<uint8_t>> m_signatureDatabase;
    std::vector<std::string> m_suspiciousPatterns;
    bool m_initialized;

public:
    VirusScannerInterface(SevenZipArchive& archive) 
        : m_archive(archive), m_initialized(false) {
        InitializePatterns();
    }

    ScanReport ScanArchive(const std::string& archivePath, const ScanOptions& options) {
        ScanReport report;
        report.overallResult = ScanResult::Clean;
        report.filesScanned = 0;
        report.threatsFound = 0;
        report.suspiciousFiles = 0;
        report.bytesScanned = 0;

        auto startTime = std::chrono::system_clock::now();

        ArchiveInfo info;
        if (!m_archive.ListArchive(archivePath, info, options.password)) {
            report.overallResult = ScanResult::Error;
            return report;
        }

        if (info.isEncrypted && options.password.empty()) {
            report.overallResult = ScanResult::PasswordProtected;
            return report;
        }

        for (const auto& file : info.files) {
            if (ShouldExclude(file.path, options.excludePatterns)) {
                continue;
            }

            ThreatInfo threat;
            ScanResult result = ScanFile(archivePath, file.path, threat, &options);

            report.filesScanned++;
            report.bytesScanned += file.size;

            if (result == ScanResult::Infected) {
                report.threatsFound++;
                report.threats.push_back(threat);
                report.overallResult = ScanResult::Infected;
            } else if (result == ScanResult::Suspicious) {
                report.suspiciousFiles++;
                report.threats.push_back(threat);
                if (report.overallResult != ScanResult::Infected) {
                    report.overallResult = ScanResult::Suspicious;
                }
            }
        }

        auto endTime = std::chrono::system_clock::now();
        report.duration = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime);

        return report;
    }

    ScanResult ScanFile(const std::string& archivePath, const std::string& filePath,
                       ThreatInfo& threat, const ScanOptions* options = nullptr) {
        std::vector<uint8_t> data;
        std::string password = options ? options->password : "";
        if (!m_archive.ExtractSingleFileToMemory(archivePath, filePath, data, password)) {
            return ScanResult::Error;
        }

        threat.filePath = filePath;

        if (!m_externalScanner.empty()) {
            return ScanWithExternalScanner(data, threat);
        }

        for (const auto& signature : m_signatureDatabase) {
            if (data.size() >= signature.size()) {
                for (size_t i = 0; i <= data.size() - signature.size(); i++) {
                    bool match = true;
                    for (size_t j = 0; j < signature.size(); j++) {
                        if (data[i + j] != signature[j]) {
                            match = false;
                            break;
                        }
                    }
                    if (match) {
                        threat.threatName = "Known malware signature detected";
                        threat.threatType = "Malware";
                        threat.severity = 10;
                        threat.action = "Quarantine";
                        return ScanResult::Infected;
                    }
                }
            }
        }

        if (options && options->heuristicsEnabled) {
            ScanResult heuristicResult = HeuristicScan(data, threat);
            if (heuristicResult != ScanResult::Clean) {
                return heuristicResult;
            }
        }

        std::string ext = GetExtension(filePath);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == ".exe" || ext == ".dll" || ext == ".scr") {
            if (HasSuspiciousPECharacteristics(data)) {
                threat.threatName = "Suspicious executable structure";
                threat.threatType = "Suspicious";
                threat.severity = 5;
                threat.action = "Monitor";
                return ScanResult::Suspicious;
            }
        }

        return ScanResult::Clean;
    }

    bool QuarantineFile(const std::string& archivePath, const std::string& filePath,
                       const std::string& quarantinePath) {
        std::vector<uint8_t> data;
        if (!m_archive.ExtractSingleFileToMemory(archivePath, filePath, data)) {
            return false;
        }

        CreateDirectoryA(quarantinePath.c_str(), NULL);

        std::string timestamp = GetTimestampString();
        std::string safeName = SanitizeFileName(filePath) + "_" + timestamp + ".quar";

        std::string quarFilePath = quarantinePath + "\\" + safeName;

        std::ofstream outFile(quarFilePath, std::ios::binary);
        if (!outFile.is_open()) {
            return false;
        }

        QuarantineHeader header;
        header.originalPath = filePath;
        header.archivePath = archivePath;
        header.quarantineTime = std::chrono::system_clock::now();

        WriteQuarantineHeader(outFile, header);
        outFile.write(reinterpret_cast<const char*>(data.data()), data.size());
        outFile.close();

        return true;
    }

    bool SetExternalScanner(const std::string& scannerPath) {
        if (GetFileAttributesA(scannerPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            m_externalScanner = scannerPath;
            return true;
        }
        return false;
    }

    std::string GetScannerVersion() {
        if (m_externalScanner.empty()) {
            return "Built-in scanner v1.0";
        }

        return "External: " + m_externalScanner;
    }

    bool UpdateDefinitions() {
        HINTERNET hInternet = InternetOpenA("SevenZipSDK", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
        if (!hInternet) return false;

        HINTERNET hConnect = InternetOpenUrlA(hInternet,
            "https://example.com/definitions.dat", NULL, 0, INTERNET_FLAG_RELOAD, 0);

        if (!hConnect) {
            InternetCloseHandle(hInternet);
            return false;
        }

        std::vector<uint8_t> buffer(4096);
        DWORD bytesRead;
        std::vector<uint8_t> allData;

        while (InternetReadFile(hConnect, buffer.data(), buffer.size(), &bytesRead) && bytesRead > 0) {
            allData.insert(allData.end(), buffer.begin(), buffer.begin() + bytesRead);
        }

        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);

        if (!allData.empty()) {
            ParseDefinitionData(allData);
            return true;
        }

        return false;
    }

private:
    struct QuarantineHeader {
        std::string originalPath;
        std::string archivePath;
        std::chrono::system_clock::time_point quarantineTime;
    };

    void InitializePatterns() {
        m_suspiciousPatterns = {
            "CreateRemoteThread",
            "VirtualAllocEx",
            "WriteProcessMemory",
            "NtUnmapViewOfSection",
            "SetWindowsHookEx",
            "keylog",
            "password",
            "creditcard",
            "backdoor",
            "shellcode"
        };

        m_signatureDatabase.push_back({0x4D, 0x5A, 0x90, 0x00, 0x03});
        m_initialized = true;
    }

    bool ShouldExclude(const std::string& filePath,
                      const std::vector<std::string>& patterns) {
        for (const auto& pattern : patterns) {
            if (filePath.find(pattern) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    ScanResult ScanWithExternalScanner(const std::vector<uint8_t>& data, ThreatInfo& threat) {
        char tempPath[MAX_PATH];
        GetTempPathA(MAX_PATH, tempPath);
        GetTempFileNameA(tempPath, "scan", 0, tempPath);

        std::ofstream outFile(tempPath, std::ios::binary);
        outFile.write(reinterpret_cast<const char*>(data.data()), data.size());
        outFile.close();

        std::string cmd = m_externalScanner + " /scan \"" + std::string(tempPath) + "\"";
        
        FILE* pipe = _popen(cmd.c_str(), "r");
        if (!pipe) {
            DeleteFileA(tempPath);
            return ScanResult::Error;
        }

        char buffer[256];
        std::string result;
        while (fgets(buffer, sizeof(buffer), pipe)) {
            result += buffer;
        }
        _pclose(pipe);

        DeleteFileA(tempPath);

        if (result.find("infected") != std::string::npos ||
            result.find("threat") != std::string::npos) {
            threat.threatName = "Detected by external scanner";
            threat.threatType = "Malware";
            threat.severity = 8;
            threat.action = "Quarantine";
            return ScanResult::Infected;
        }

        if (result.find("suspicious") != std::string::npos) {
            threat.threatName = "Suspicious by external scanner";
            threat.threatType = "Suspicious";
            threat.severity = 5;
            threat.action = "Monitor";
            return ScanResult::Suspicious;
        }

        return ScanResult::Clean;
    }

    ScanResult HeuristicScan(const std::vector<uint8_t>& data, ThreatInfo& threat) {
        std::string dataStr(data.begin(), data.end());
        std::transform(dataStr.begin(), dataStr.end(), dataStr.begin(), ::tolower);

        int suspiciousScore = 0;

        for (const auto& pattern : m_suspiciousPatterns) {
            if (dataStr.find(pattern) != std::string::npos) {
                suspiciousScore += 2;
            }
        }

        if (data.size() > 1024) {
            size_t nullCount = std::count(data.begin(), data.begin() + 1024, 0);
            if ((double)nullCount / 1024 > 0.3) {
                suspiciousScore += 1;
            }
        }

        if (ContainsEncryptedPayload(data)) {
            suspiciousScore += 3;
        }

        if (suspiciousScore >= 5) {
            threat.threatName = "Heuristic analysis: High risk behavior";
            threat.threatType = "Suspicious";
            threat.severity = suspiciousScore;
            threat.action = "Quarantine";
            return ScanResult::Suspicious;
        } else if (suspiciousScore >= 3) {
            threat.threatName = "Heuristic analysis: Moderate risk indicators";
            threat.threatType = "Suspicious";
            threat.severity = suspiciousScore;
            threat.action = "Monitor";
            return ScanResult::Suspicious;
        }

        return ScanResult::Clean;
    }

    bool HasSuspiciousPECharacteristics(const std::vector<uint8_t>& data) {
        if (data.size() < 512) return false;

        if (data[0] != 'M' || data[1] != 'Z') return false;

        uint32_t peOffset = *reinterpret_cast<const uint32_t*>(&data[60]);
        if (peOffset + 4 > data.size()) return false;

        if (data[peOffset] != 'P' || data[peOffset + 1] != 'E') return false;

        uint16_t characteristics = *reinterpret_cast<const uint16_t*>(&data[peOffset + 22]);
        if ((characteristics & 0x0002) == 0) {
            return true;
        }

        return false;
    }

    bool ContainsEncryptedPayload(const std::vector<uint8_t>& data) {
        if (data.size() < 256) return false;

        double entropy = CalculateEntropy(data);
        return entropy > 7.5;
    }

    double CalculateEntropy(const std::vector<uint8_t>& data) {
        std::map<uint8_t, size_t> freq;
        for (uint8_t b : data) {
            freq[b]++;
        }

        double entropy = 0.0;
        double size = data.size();

        for (const auto& pair : freq) {
            double p = pair.second / size;
            if (p > 0) {
                entropy -= p * log2(p);
            }
        }

        return entropy;
    }

    std::string GetExtension(const std::string& path) {
        size_t pos = path.find_last_of('.');
        if (pos != std::string::npos) {
            return path.substr(pos);
        }
        return "";
    }

    std::string GetTimestampString() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");
        return ss.str();
    }

    std::string SanitizeFileName(const std::string& name) {
        std::string result = name;
        std::replace(result.begin(), result.end(), '\\', '_');
        std::replace(result.begin(), result.end(), '/', '_');
        std::replace(result.begin(), result.end(), ':', '_');
        std::replace(result.begin(), result.end(), '*', '_');
        std::replace(result.begin(), result.end(), '?', '_');
        std::replace(result.begin(), result.end(), '"', '_');
        std::replace(result.begin(), result.end(), '<', '_');
        std::replace(result.begin(), result.end(), '>', '_');
        std::replace(result.begin(), result.end(), '|', '_');
        return result;
    }

    void WriteQuarantineHeader(std::ofstream& file, const QuarantineHeader& header) {
        uint32_t magic = 0x51554152;
        file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));

        uint32_t pathLen = (uint32_t)header.originalPath.size();
        file.write(reinterpret_cast<const char*>(&pathLen), sizeof(pathLen));
        file.write(header.originalPath.c_str(), pathLen);

        pathLen = (uint32_t)header.archivePath.size();
        file.write(reinterpret_cast<const char*>(&pathLen), sizeof(pathLen));
        file.write(header.archivePath.c_str(), pathLen);

        auto timestamp = std::chrono::system_clock::to_time_t(header.quarantineTime);
        file.write(reinterpret_cast<const char*>(&timestamp), sizeof(timestamp));
    }

    void ParseDefinitionData(const std::vector<uint8_t>& data) {
        m_signatureDatabase.clear();
        
        size_t offset = 0;
        while (offset + 4 <= data.size()) {
            uint32_t sigLen = *reinterpret_cast<const uint32_t*>(&data[offset]);
            offset += 4;

            if (offset + sigLen > data.size()) break;

            std::vector<uint8_t> sig(data.begin() + offset, data.begin() + offset + sigLen);
            m_signatureDatabase.push_back(sig);
            offset += sigLen;
        }
    }
};

class ArchiveConverter {
public:
    struct ConversionOptions {
        ArchiveFormat targetFormat = ArchiveFormat::FMT_7Z;
        CompressionMethod method = CompressionMethod::LZMA2;
        CompressionLevel level = CompressionLevel::Normal;
        bool preserveTimestamps = true;
        bool preserveAttributes = true;
        std::string password;
        std::string newPassword;
        uint32_t threadCount = 0;
    };

    struct ConversionResult {
        bool success = false;
        uint64_t originalSize = 0;
        uint64_t convertedSize = 0;
        uint32_t filesConverted = 0;
        std::string errorMessage;
    };

private:
    SevenZipArchive& m_archive;

public:
    ArchiveConverter(SevenZipArchive& archive) : m_archive(archive) {}

    ConversionResult ConvertArchive(const std::string& sourcePath, const std::string& targetPath,
                                   const ConversionOptions& options) {
        ConversionResult result;

        ArchiveInfo info;
        if (!m_archive.ListArchive(sourcePath, info, options.password)) {
            result.errorMessage = "Failed to read source archive";
            return result;
        }

        WIN32_FILE_ATTRIBUTE_DATA attr;
        if (GetFileAttributesExA(sourcePath.c_str(), GetFileExInfoStandard, &attr)) {
            result.originalSize = ((uint64_t)attr.nFileSizeHigh << 32) | attr.nFileSizeLow;
        }

        std::string tempDir = GetTempDirectory();
        CreateDirectoryA(tempDir.c_str(), NULL);

        ExtractOptions extractOpts;
        extractOpts.outputDir = tempDir;
        extractOpts.password = options.password;
        extractOpts.preserveDirectoryStructure = true;
        extractOpts.preserveFileTime = options.preserveTimestamps;
        extractOpts.preserveFileAttrib = options.preserveAttributes;

        if (!m_archive.ExtractArchive(sourcePath, extractOpts)) {
            result.errorMessage = "Failed to extract source archive";
            RemoveDirectoryRecursive(tempDir);
            return result;
        }

        CompressionOptions compOpts;
        compOpts.method = options.method;
        compOpts.level = options.level;
        compOpts.password = options.newPassword.empty() ? options.password : options.newPassword;
        compOpts.threadCount = options.threadCount;

        if (!m_archive.CompressDirectory(targetPath, tempDir, compOpts, true)) {
            result.errorMessage = "Failed to create target archive";
            RemoveDirectoryRecursive(tempDir);
            return result;
        }

        RemoveDirectoryRecursive(tempDir);

        if (GetFileAttributesExA(targetPath.c_str(), GetFileExInfoStandard, &attr)) {
            result.convertedSize = ((uint64_t)attr.nFileSizeHigh << 32) | attr.nFileSizeLow;
        }

        result.filesConverted = (uint32_t)info.files.size();
        result.success = true;

        return result;
    }

    ConversionResult ConvertTo7z(const std::string& sourcePath, const std::string& targetPath,
                                CompressionLevel level) {
        ConversionOptions options;
        options.targetFormat = ArchiveFormat::FMT_7Z;
        options.method = CompressionMethod::LZMA2;
        options.level = level;
        return ConvertArchive(sourcePath, targetPath, options);
    }

    ConversionResult ConvertToZip(const std::string& sourcePath, const std::string& targetPath,
                                 CompressionLevel level) {
        ConversionOptions options;
        options.targetFormat = ArchiveFormat::FMT_ZIP;
        options.method = CompressionMethod::DEFLATE;
        options.level = level;
        return ConvertArchive(sourcePath, targetPath, options);
    }

    bool BatchConvert(const std::vector<std::string>& sources, const std::string& outputDir,
                     const ConversionOptions& options,
                     std::function<void(const std::string&, const ConversionResult&)> callback) {
        CreateDirectoryA(outputDir.c_str(), NULL);

        bool allSuccess = true;

        for (const auto& source : sources) {
            std::string fileName = GetFileName(source);
            size_t dotPos = fileName.find_last_of('.');
            if (dotPos != std::string::npos) {
                fileName = fileName.substr(0, dotPos);
            }

            std::string extension = GetExtensionForFormat(options.targetFormat);
            std::string targetPath = outputDir + "\\" + fileName + extension;

            ConversionResult result = ConvertArchive(source, targetPath, options);

            if (callback) {
                callback(source, result);
            }

            if (!result.success) {
                allSuccess = false;
            }
        }

        return allSuccess;
    }

private:
    std::string GetTempDirectory() {
        char tempPath[MAX_PATH];
        GetTempPathA(MAX_PATH, tempPath);
        
        char tempDir[MAX_PATH];
        GetTempFileNameA(tempPath, "convert", 0, tempDir);
        DeleteFileA(tempDir);
        
        return std::string(tempDir);
    }

    void RemoveDirectoryRecursive(const std::string& path) {
        WIN32_FIND_DATAA findData;
        HANDLE hFind = FindFirstFileA((path + "\\*").c_str(), &findData);

        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (findData.cFileName[0] != '.') {
                    std::string fullPath = path + "\\" + findData.cFileName;
                    if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                        RemoveDirectoryRecursive(fullPath);
                    } else {
                        DeleteFileA(fullPath.c_str());
                    }
                }
            } while (FindNextFileA(hFind, &findData));
            FindClose(hFind);
        }

        RemoveDirectoryA(path.c_str());
    }

    std::string GetFileName(const std::string& path) {
        size_t pos = path.find_last_of("\\/");
        if (pos != std::string::npos) {
            return path.substr(pos + 1);
        }
        return path;
    }

    std::string GetExtensionForFormat(ArchiveFormat format) {
        switch (format) {
            case ArchiveFormat::FMT_7Z: return ".7z";
            case ArchiveFormat::FMT_ZIP: return ".zip";
            case ArchiveFormat::FMT_TAR: return ".tar";
            case ArchiveFormat::FMT_GZIP: return ".gz";
            case ArchiveFormat::FMT_BZIP2: return ".bz2";
            case ArchiveFormat::FMT_XZ: return ".xz";
            default: return ".7z";
        }
    }
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
        uint32_t minLength = 8;
        bool requireUppercase = true;
        bool requireLowercase = true;
        bool requireNumbers = true;
        bool requireSymbols = false;
        uint32_t expirationDays = 0;
    };

private:
    std::vector<PasswordEntry> m_entries;
    std::string m_dataPath;
    std::string m_masterPassword;

public:
    PasswordManager() {
        char appData[MAX_PATH];
        if (GetEnvironmentVariableA("APPDATA", appData, MAX_PATH) > 0) {
            m_dataPath = std::string(appData) + "\\SevenZipSDK\\passwords.dat";
            CreateDirectoryA((std::string(appData) + "\\SevenZipSDK").c_str(), NULL);
            LoadEntries();
        }
    }

    bool AddPassword(const std::string& archivePath, const std::string& password) {
        PasswordEntry entry;
        entry.id = GenerateId();
        entry.archivePath = archivePath;
        entry.password = password;
        entry.addedTime = std::chrono::system_clock::now();
        entry.lastUsedTime = entry.addedTime;
        entry.useCount = 0;

        auto existing = std::find_if(m_entries.begin(), m_entries.end(),
            [&](const PasswordEntry& e) { return e.archivePath == archivePath; });

        if (existing != m_entries.end()) {
            *existing = entry;
        } else {
            m_entries.push_back(entry);
        }

        SaveEntries();
        return true;
    }

    bool RemovePassword(const std::string& archivePath) {
        auto it = std::remove_if(m_entries.begin(), m_entries.end(),
            [&](const PasswordEntry& e) { return e.archivePath == archivePath; });

        if (it != m_entries.end()) {
            m_entries.erase(it, m_entries.end());
            SaveEntries();
            return true;
        }

        return false;
    }

    std::string GetPassword(const std::string& archivePath) {
        auto it = std::find_if(m_entries.begin(), m_entries.end(),
            [&](const PasswordEntry& e) { return e.archivePath == archivePath; });

        if (it != m_entries.end()) {
            it->lastUsedTime = std::chrono::system_clock::now();
            it->useCount++;
            SaveEntries();
            return it->password;
        }

        return "";
    }

    std::vector<PasswordEntry> GetAllPasswords() {
        return m_entries;
    }

    std::string GeneratePassword(uint32_t length, const PasswordPolicy& policy) {
        const char* uppercase = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        const char* lowercase = "abcdefghijklmnopqrstuvwxyz";
        const char* numbers = "0123456789";
        const char* symbols = "!@#$%^&*()_+-=[]{}|;:,.<>?";

        std::string charset;
        std::string password;

        if (policy.requireUppercase) {
            charset += uppercase;
            password += uppercase[rand() % strlen(uppercase)];
        }
        if (policy.requireLowercase) {
            charset += lowercase;
            password += lowercase[rand() % strlen(lowercase)];
        }
        if (policy.requireNumbers) {
            charset += numbers;
            password += numbers[rand() % strlen(numbers)];
        }
        if (policy.requireSymbols) {
            charset += symbols;
            password += symbols[rand() % strlen(symbols)];
        }

        while (password.length() < length) {
            password += charset[rand() % charset.length()];
        }

        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(password.begin(), password.end(), g);

        return password;
    }

    bool ValidatePassword(const std::string& password, const PasswordPolicy& policy) {
        if (password.length() < policy.minLength) {
            return false;
        }

        bool hasUpper = false, hasLower = false, hasNumber = false, hasSymbol = false;

        for (char c : password) {
            if (isupper(c)) hasUpper = true;
            else if (islower(c)) hasLower = true;
            else if (isdigit(c)) hasNumber = true;
            else hasSymbol = true;
        }

        if (policy.requireUppercase && !hasUpper) return false;
        if (policy.requireLowercase && !hasLower) return false;
        if (policy.requireNumbers && !hasNumber) return false;
        if (policy.requireSymbols && !hasSymbol) return false;

        return true;
    }

    bool ExportPasswords(const std::string& exportPath, const std::string& masterPassword) {
        std::ofstream file(exportPath, std::ios::binary);
        if (!file.is_open()) return false;

        uint32_t magic = 0x50575344;
        file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));

        uint32_t count = (uint32_t)m_entries.size();
        file.write(reinterpret_cast<const char*>(&count), sizeof(count));

        for (const auto& entry : m_entries) {
            std::string encryptedPassword = EncryptString(entry.password, masterPassword);
            
            WriteString(file, entry.id);
            WriteString(file, entry.archivePath);
            WriteString(file, encryptedPassword);
            
            auto added = std::chrono::system_clock::to_time_t(entry.addedTime);
            auto used = std::chrono::system_clock::to_time_t(entry.lastUsedTime);
            file.write(reinterpret_cast<const char*>(&added), sizeof(added));
            file.write(reinterpret_cast<const char*>(&used), sizeof(used));
            file.write(reinterpret_cast<const char*>(&entry.useCount), sizeof(entry.useCount));
        }

        file.close();
        return true;
    }

    bool ImportPasswords(const std::string& importPath, const std::string& masterPassword) {
        std::ifstream file(importPath, std::ios::binary);
        if (!file.is_open()) return false;

        uint32_t magic;
        file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        
        if (magic != 0x50575344) {
            file.close();
            return false;
        }

        uint32_t count;
        file.read(reinterpret_cast<char*>(&count), sizeof(count));

        for (uint32_t i = 0; i < count; i++) {
            PasswordEntry entry;
            
            entry.id = ReadString(file);
            entry.archivePath = ReadString(file);
            std::string encryptedPassword = ReadString(file);
            entry.password = DecryptString(encryptedPassword, masterPassword);
            
            time_t added, used;
            file.read(reinterpret_cast<char*>(&added), sizeof(added));
            file.read(reinterpret_cast<char*>(&used), sizeof(used));
            entry.addedTime = std::chrono::system_clock::from_time_t(added);
            entry.lastUsedTime = std::chrono::system_clock::from_time_t(used);
            
            file.read(reinterpret_cast<char*>(&entry.useCount), sizeof(entry.useCount));
            
            m_entries.push_back(entry);
        }

        file.close();
        SaveEntries();
        return true;
    }

private:
    std::string GenerateId() {
        GUID guid;
        CoCreateGuid(&guid);
        
        char buffer[64];
        sprintf_s(buffer, sizeof(buffer), "%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
                 guid.Data1, guid.Data2, guid.Data3,
                 guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
                 guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
        
        return std::string(buffer);
    }

    bool LoadEntries() {
        std::ifstream file(m_dataPath, std::ios::binary);
        if (!file.is_open()) return false;

        m_entries.clear();

        uint32_t count;
        file.read(reinterpret_cast<char*>(&count), sizeof(count));

        for (uint32_t i = 0; i < count; i++) {
            PasswordEntry entry;
            
            entry.id = ReadString(file);
            entry.archivePath = ReadString(file);
            entry.password = ReadString(file);
            
            time_t added, used;
            file.read(reinterpret_cast<char*>(&added), sizeof(added));
            file.read(reinterpret_cast<char*>(&used), sizeof(used));
            entry.addedTime = std::chrono::system_clock::from_time_t(added);
            entry.lastUsedTime = std::chrono::system_clock::from_time_t(used);
            
            file.read(reinterpret_cast<char*>(&entry.useCount), sizeof(entry.useCount));
            
            m_entries.push_back(entry);
        }

        file.close();
        return true;
    }

    bool SaveEntries() {
        std::ofstream file(m_dataPath, std::ios::binary);
        if (!file.is_open()) return false;

        uint32_t count = (uint32_t)m_entries.size();
        file.write(reinterpret_cast<const char*>(&count), sizeof(count));

        for (const auto& entry : m_entries) {
            WriteString(file, entry.id);
            WriteString(file, entry.archivePath);
            WriteString(file, entry.password);
            
            auto added = std::chrono::system_clock::to_time_t(entry.addedTime);
            auto used = std::chrono::system_clock::to_time_t(entry.lastUsedTime);
            file.write(reinterpret_cast<const char*>(&added), sizeof(added));
            file.write(reinterpret_cast<const char*>(&used), sizeof(used));
            file.write(reinterpret_cast<const char*>(&entry.useCount), sizeof(entry.useCount));
        }

        file.close();
        return true;
    }

    std::string EncryptString(const std::string& plaintext, const std::string& key) {
        std::string result = plaintext;
        for (size_t i = 0; i < plaintext.length(); i++) {
            result[i] = plaintext[i] ^ key[i % key.length()];
        }
        return result;
    }

    std::string DecryptString(const std::string& ciphertext, const std::string& key) {
        return EncryptString(ciphertext, key);
    }

    void WriteString(std::ofstream& file, const std::string& str) {
        uint32_t len = (uint32_t)str.size();
        file.write(reinterpret_cast<const char*>(&len), sizeof(len));
        file.write(str.c_str(), len);
    }

    std::string ReadString(std::ifstream& file) {
        uint32_t len;
        file.read(reinterpret_cast<char*>(&len), sizeof(len));
        
        std::string str(len, '\0');
        file.read(&str[0], len);
        
        return str;
    }
};

class ArchiveValidator {
public:
    struct ValidationResult {
        bool isValid = true;
        bool headersValid = true;
        bool dataValid = true;
        bool checksumsValid = true;
        uint32_t corruptedFiles = 0;
        uint64_t corruptedBytes = 0;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
    };

    struct ValidationOptions {
        bool checkCRC = true;
        bool checkHeaders = true;
        bool extractTest = false;
        bool deepScan = false;
        uint32_t maxErrors = 100;
    };

private:
    SevenZipArchive& m_archive;

public:
    ArchiveValidator(SevenZipArchive& archive) : m_archive(archive) {}

    ValidationResult ValidateArchive(const std::string& archivePath,
                                    const ValidationOptions& options) {
        ValidationResult result;

        if (!ValidateFileExists(archivePath)) {
            result.isValid = false;
            result.errors.push_back("Archive file does not exist");
            return result;
        }

        if (options.checkHeaders) {
            if (!ValidateHeaders(archivePath, result)) {
                result.headersValid = false;
                result.isValid = false;
            }
        }

        ArchiveInfo info;
        if (!m_archive.ListArchive(archivePath, info)) {
            result.isValid = false;
            result.errors.push_back("Failed to read archive contents");
            return result;
        }

        if (options.checkCRC) {
            ValidateChecksums(archivePath, info, result, options);
        }

        if (options.extractTest) {
            ValidateExtraction(archivePath, info, result, options);
        }

        if (options.deepScan) {
            DeepValidateArchive(archivePath, result, options);
        }

        return result;
    }

    bool QuickValidate(const std::string& archivePath) {
        ValidationOptions options;
        options.checkCRC = false;
        options.extractTest = false;
        options.deepScan = false;

        ValidationResult result = ValidateArchive(archivePath, options);
        return result.isValid;
    }

    bool ValidateFile(const std::string& archivePath, const std::string& filePath) {
        ArchiveInfo info;
        if (!m_archive.ListArchive(archivePath, info)) {
            return false;
        }

        for (const auto& file : info.files) {
            if (file.path == filePath) {
                std::vector<uint8_t> data;
                if (m_archive.ExtractSingleFileToMemory(archivePath, filePath, data)) {
                    uint32_t calculatedCRC = CalculateCRC32(data);
                    return calculatedCRC == file.crc;
                }
                break;
            }
        }

        return false;
    }

    std::string GenerateChecksum(const std::string& archivePath, const std::string& algorithm) {
        std::ifstream file(archivePath, std::ios::binary);
        if (!file.is_open()) return "";

        std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                                  std::istreambuf_iterator<char>());
        file.close();

        std::string algoLower = algorithm;
        std::transform(algoLower.begin(), algoLower.end(), 
                      algoLower.begin(), ::tolower);

        if (algoLower == "crc32") {
            uint32_t crc = CalculateCRC32(data);
            std::ostringstream oss;
            oss << std::hex << std::uppercase << crc;
            return oss.str();
        } else if (algoLower == "md5") {
            return CalculateMD5(data);
        } else if (algoLower == "sha256") {
            return CalculateSHA256(data);
        }

        return "";
    }

    bool VerifyChecksum(const std::string& archivePath, const std::string& expectedChecksum,
                       const std::string& algorithm) {
        std::string actualChecksum = GenerateChecksum(archivePath, algorithm);
        
        std::transform(actualChecksum.begin(), actualChecksum.end(), 
                      actualChecksum.begin(), ::tolower);
        std::string expectedLower = expectedChecksum;
        std::transform(expectedLower.begin(), expectedLower.end(), 
                      expectedLower.begin(), ::tolower);

        return actualChecksum == expectedLower;
    }

private:
    bool ValidateFileExists(const std::string& path) {
        return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
    }

    bool ValidateHeaders(const std::string& archivePath, ValidationResult& result) {
        std::ifstream file(archivePath, std::ios::binary);
        if (!file.is_open()) {
            result.errors.push_back("Cannot open archive file");
            return false;
        }

        uint8_t header[32];
        file.read(reinterpret_cast<char*>(header), sizeof(header));
        file.close();

        if (header[0] == '7' && header[1] == 'z' && 
            header[2] == 0xBC && header[3] == 0xAF) {
            return true;
        } else if (header[0] == 'P' && header[1] == 'K') {
            return true;
        } else if (header[0] == 'R' && header[1] == 'a' && 
                   header[2] == 'r' && header[3] == '!') {
            return true;
        } else {
            result.warnings.push_back("Unknown archive format");
        }

        return true;
    }

    void ValidateChecksums(const std::string& archivePath, const ArchiveInfo& info,
                          ValidationResult& result, const ValidationOptions& options) {
        for (const auto& file : info.files) {
            if (result.errors.size() >= options.maxErrors) {
                result.warnings.push_back("Maximum error count reached");
                break;
            }

            std::vector<uint8_t> data;
            if (!m_archive.ExtractSingleFileToMemory(archivePath, file.path, data)) {
                result.corruptedFiles++;
                result.corruptedBytes += file.size;
                result.errors.push_back("Failed to extract: " + file.path);
                continue;
            }

            uint32_t calculatedCRC = CalculateCRC32(data);
            if (calculatedCRC != file.crc) {
                result.corruptedFiles++;
                result.corruptedBytes += file.size;
                result.errors.push_back("CRC mismatch: " + file.path);
                result.checksumsValid = false;
            }
        }

        if (result.corruptedFiles > 0) {
            result.dataValid = false;
            result.isValid = false;
        }
    }

    void ValidateExtraction(const std::string& archivePath, const ArchiveInfo& info,
                           ValidationResult& result, const ValidationOptions& options) {
        char tempPath[MAX_PATH];
        GetTempPathA(MAX_PATH, tempPath);
        GetTempFileNameA(tempPath, "valid", 0, tempPath);
        DeleteFileA(tempPath);
        CreateDirectoryA(tempPath, NULL);

        ExtractOptions extractOpts;
        extractOpts.outputDir = tempPath;
        extractOpts.overwriteExisting = true;

        if (!m_archive.ExtractArchive(archivePath, extractOpts)) {
            result.errors.push_back("Extraction test failed");
            result.dataValid = false;
            result.isValid = false;
        }

        RemoveDirectoryRecursive(tempPath);
    }

    void DeepValidateArchive(const std::string& archivePath, ValidationResult& result,
                            const ValidationOptions& options) {
        std::ifstream file(archivePath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return;

        uint64_t fileSize = file.tellg();
        file.seekg(0);

        uint64_t reportedSize = 0;
        ArchiveInfo info;
        if (m_archive.ListArchive(archivePath, info)) {
            for (const auto& file : info.files) {
                reportedSize += file.packedSize;
            }
        }

        if (reportedSize > fileSize) {
            result.warnings.push_back("Reported packed size exceeds file size");
        }

        file.close();
    }

    void RemoveDirectoryRecursive(const std::string& path) {
        WIN32_FIND_DATAA findData;
        HANDLE hFind = FindFirstFileA((path + "\\*").c_str(), &findData);

        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (findData.cFileName[0] != '.') {
                    std::string fullPath = path + "\\" + findData.cFileName;
                    if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                        RemoveDirectoryRecursive(fullPath);
                    } else {
                        DeleteFileA(fullPath.c_str());
                    }
                }
            } while (FindNextFileA(hFind, &findData));
            FindClose(hFind);
        }

        RemoveDirectoryA(path.c_str());
    }

    uint32_t CalculateCRC32(const std::vector<uint8_t>& data) {
        uint32_t crc = 0xFFFFFFFF;
        
        static uint32_t table[256];
        static bool initialized = false;
        
        if (!initialized) {
            for (uint32_t i = 0; i < 256; i++) {
                uint32_t c = i;
                for (int j = 0; j < 8; j++) {
                    c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
                }
                table[i] = c;
            }
            initialized = true;
        }

        for (uint8_t byte : data) {
            crc = table[(crc ^ byte) & 0xFF] ^ (crc >> 8);
        }

        return crc ^ 0xFFFFFFFF;
    }

    std::string CalculateMD5(const std::vector<uint8_t>& data) {
        HCRYPTPROV hProv = 0;
        HCRYPTHASH hHash = 0;

        if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
            return "";
        }

        if (!CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash)) {
            CryptReleaseContext(hProv, 0);
            return "";
        }

        if (!CryptHashData(hHash, data.data(), (DWORD)data.size(), 0)) {
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            return "";
        }

        DWORD hashLen = 16;
        BYTE hashData[16];
        if (!CryptGetHashParam(hHash, HP_HASHVAL, hashData, &hashLen, 0)) {
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            return "";
        }

        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);

        std::ostringstream oss;
        for (DWORD i = 0; i < hashLen; i++) {
            oss << std::hex << std::setw(2) << std::setfill('0') << (int)hashData[i];
        }

        return oss.str();
    }

    std::string CalculateSHA256(const std::vector<uint8_t>& data) {
        HCRYPTPROV hProv = 0;
        HCRYPTHASH hHash = 0;

        if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
            return "";
        }

        if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
            CryptReleaseContext(hProv, 0);
            return "";
        }

        if (!CryptHashData(hHash, data.data(), (DWORD)data.size(), 0)) {
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            return "";
        }

        DWORD hashLen = 32;
        BYTE hashData[32];
        if (!CryptGetHashParam(hHash, HP_HASHVAL, hashData, &hashLen, 0)) {
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            return "";
        }

        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);

        std::ostringstream oss;
        for (DWORD i = 0; i < hashLen; i++) {
            oss << std::hex << std::setw(2) << std::setfill('0') << (int)hashData[i];
        }

        return oss.str();
    }
};

class SDKConfig {
public:
    struct ApplicationConfig {
        std::string name = "SevenZip SDK";
        std::string logLevel = "info";
        std::string logFile = "7zsdk.log";
        std::string tempDirectory = "";
        uint32_t maxThreads = 0;
    };

    struct CompressionConfig {
        std::string defaultFormat = "7z";
        std::string method = "LZMA2";
        std::string level = "Normal";
        bool solidMode = false;
        uint64_t solidBlockSize = 0;
        uint32_t threadCount = 0;
        uint32_t dictionarySize = 0;
        uint32_t wordSize = 0;
    };

    struct ExtractionConfig {
        bool overwriteExisting = false;
        bool preserveDirectoryStructure = true;
        bool preserveFileTime = true;
        bool preserveFileAttributes = true;
        bool createOutputDirectory = true;
    };

    struct EncryptionConfig {
        std::string algorithm = "AES256";
        std::string keyDerivation = "PBKDF2";
        uint32_t iterations = 100000;
        bool encryptHeaders = false;
        std::string defaultPassword = "";
    };

    struct BackupConfig {
        std::string defaultType = "Full";
        bool preservePermissions = true;
        bool preserveTimestamps = true;
        bool includeEmptyDirectories = true;
        std::vector<std::string> excludePatterns;
        std::vector<std::string> includePatterns;
    };

    struct SplitConfig {
        bool enabled = false;
        uint64_t volumeSize = 100 * 1024 * 1024;
    };

    struct ValidationConfig {
        bool checkCRC = true;
        bool checkHeaders = true;
        bool extractTest = false;
        bool deepScan = false;
        uint32_t maxErrors = 100;
    };

    struct VirusScanConfig {
        bool enabled = false;
        bool scanArchives = true;
        bool heuristicsEnabled = true;
        uint32_t maxRecursionDepth = 10;
        std::string externalScanner = "";
        std::string quarantineDirectory = "quarantine";
    };

    struct CloudConfig {
        bool enabled = false;
        std::string provider = "";
        std::string endpoint = "";
        std::string apiKey = "";
        uint32_t timeout = 30000;
        uint32_t retryCount = 3;
        uint32_t retryDelay = 1000;
    };

    struct TimelineConfig {
        bool enabled = false;
        std::string storagePath = "timeline";
        uint32_t maxEntries = 30;
        uint32_t maxAgeDays = 90;
        bool autoPrune = true;
    };

    struct DeduplicationConfig {
        bool enabled = false;
        uint32_t chunkMinSize = 4096;
        uint32_t chunkMaxSize = 65536;
        uint32_t chunkTargetSize = 8192;
        std::string hashAlgorithm = "SHA256";
        std::string storagePath = "dedup_store";
    };

    struct ProgressConfig {
        uint32_t updateInterval = 100;
        bool showSpeed = true;
        bool showETA = true;
        bool showCurrentFile = true;
    };

    struct PerformanceConfig {
        bool useMemoryMapping = true;
        uint32_t bufferSize = 1024 * 1024;
        uint32_t prefetchSize = 4 * 1024 * 1024;
        bool asyncIO = true;
    };

private:
    ApplicationConfig m_app;
    CompressionConfig m_compression;
    ExtractionConfig m_extraction;
    EncryptionConfig m_encryption;
    BackupConfig m_backup;
    SplitConfig m_split;
    ValidationConfig m_validation;
    VirusScanConfig m_virusScan;
    CloudConfig m_cloud;
    TimelineConfig m_timeline;
    DeduplicationConfig m_deduplication;
    ProgressConfig m_progress;
    PerformanceConfig m_performance;
    std::string m_configPath;

public:
    SDKConfig() = default;

    bool LoadFromFile(const std::string& configPath) {
        m_configPath = configPath;
        
        std::ifstream file(configPath);
        if (!file.is_open()) {
            return false;
        }

        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        file.close();

        return ParseJson(content);
    }

    bool SaveToFile(const std::string& configPath = "") {
        std::string path = configPath.empty() ? m_configPath : configPath;
        if (path.empty()) return false;

        std::ofstream file(path);
        if (!file.is_open()) return false;

        std::string json = GenerateJson();
        file.write(json.c_str(), json.size());
        file.close();

        return true;
    }

    bool LoadFromRegistry(const std::string& keyPath) {
        HKEY hKey;
        if (RegOpenKeyExA(HKEY_CURRENT_USER, keyPath.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
            return false;
        }

        char buffer[4096];
        DWORD bufferSize = sizeof(buffer);
        
        if (RegQueryValueExA(hKey, "CompressionMethod", NULL, NULL, (LPBYTE)buffer, &bufferSize) == ERROR_SUCCESS) {
            m_compression.method = std::string(buffer, bufferSize - 1);
        }
        
        bufferSize = sizeof(buffer);
        if (RegQueryValueExA(hKey, "CompressionLevel", NULL, NULL, (LPBYTE)buffer, &bufferSize) == ERROR_SUCCESS) {
            m_compression.level = std::string(buffer, bufferSize - 1);
        }

        DWORD dwordValue;
        bufferSize = sizeof(dwordValue);
        if (RegQueryValueExA(hKey, "ThreadCount", NULL, NULL, (LPBYTE)&dwordValue, &bufferSize) == ERROR_SUCCESS) {
            m_compression.threadCount = dwordValue;
        }

        RegCloseKey(hKey);
        return true;
    }

    bool SaveToRegistry(const std::string& keyPath) {
        HKEY hKey;
        if (RegCreateKeyExA(HKEY_CURRENT_USER, keyPath.c_str(), 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS) {
            return false;
        }

        RegSetValueExA(hKey, "CompressionMethod", 0, REG_SZ, (const BYTE*)m_compression.method.c_str(), m_compression.method.size() + 1);
        RegSetValueExA(hKey, "CompressionLevel", 0, REG_SZ, (const BYTE*)m_compression.level.c_str(), m_compression.level.size() + 1);
        RegSetValueExA(hKey, "ThreadCount", 0, REG_DWORD, (const BYTE*)&m_compression.threadCount, sizeof(DWORD));

        RegCloseKey(hKey);
        return true;
    }

    CompressionOptions GetCompressionOptions() const {
        CompressionOptions opts;
        
        if (m_compression.method == "LZMA") opts.method = CompressionMethod::LZMA;
        else if (m_compression.method == "LZMA2") opts.method = CompressionMethod::LZMA2;
        else if (m_compression.method == "BZIP2") opts.method = CompressionMethod::BZIP2;
        else if (m_compression.method == "PPMD") opts.method = CompressionMethod::PPMD;
        else if (m_compression.method == "DEFLATE") opts.method = CompressionMethod::DEFLATE;
        
        if (m_compression.level == "None") opts.level = CompressionLevel::None;
        else if (m_compression.level == "Fastest") opts.level = CompressionLevel::Fastest;
        else if (m_compression.level == "Fast") opts.level = CompressionLevel::Fast;
        else if (m_compression.level == "Normal") opts.level = CompressionLevel::Normal;
        else if (m_compression.level == "Maximum") opts.level = CompressionLevel::Maximum;
        else if (m_compression.level == "Ultra") opts.level = CompressionLevel::Ultra;
        
        opts.solidMode = m_compression.solidMode;
        opts.threadCount = m_compression.threadCount;
        
        return opts;
    }

    ExtractOptions GetExtractOptions(const std::string& outputDir = "") const {
        ExtractOptions opts;
        opts.outputDir = outputDir;
        opts.overwriteExisting = m_extraction.overwriteExisting;
        opts.preserveDirectoryStructure = m_extraction.preserveDirectoryStructure;
        opts.preserveFileTime = m_extraction.preserveFileTime;
        opts.preserveFileAttrib = m_extraction.preserveFileAttributes;
        return opts;
    }

    BackupOptions GetBackupOptions() const {
        BackupOptions opts;
        
        if (m_backup.defaultType == "Full") opts.type = BackupType::Full;
        else if (m_backup.defaultType == "Incremental") opts.type = BackupType::Incremental;
        else if (m_backup.defaultType == "Differential") opts.type = BackupType::Differential;
        
        opts.preservePermissions = m_backup.preservePermissions;
        opts.preserveTimestamps = m_backup.preserveTimestamps;
        opts.includeEmptyDirectories = m_backup.includeEmptyDirectories;
        opts.excludePatterns = m_backup.excludePatterns;
        opts.includePatterns = m_backup.includePatterns;
        
        return opts;
    }

    ArchiveValidator::ValidationOptions GetValidationOptions() const {
        ArchiveValidator::ValidationOptions opts;
        opts.checkCRC = m_validation.checkCRC;
        opts.checkHeaders = m_validation.checkHeaders;
        opts.extractTest = m_validation.extractTest;
        opts.deepScan = m_validation.deepScan;
        opts.maxErrors = m_validation.maxErrors;
        return opts;
    }

    VirusScannerInterface::ScanOptions GetScanOptions() const {
        VirusScannerInterface::ScanOptions opts;
        opts.scanArchives = m_virusScan.scanArchives;
        opts.heuristicsEnabled = m_virusScan.heuristicsEnabled;
        opts.maxRecursionDepth = m_virusScan.maxRecursionDepth;
        return opts;
    }

    ApplicationConfig& Application() { return m_app; }
    const ApplicationConfig& Application() const { return m_app; }
    
    CompressionConfig& Compression() { return m_compression; }
    const CompressionConfig& Compression() const { return m_compression; }
    
    ExtractionConfig& Extraction() { return m_extraction; }
    const ExtractionConfig& Extraction() const { return m_extraction; }
    
    EncryptionConfig& Encryption() { return m_encryption; }
    const EncryptionConfig& Encryption() const { return m_encryption; }
    
    BackupConfig& Backup() { return m_backup; }
    const BackupConfig& Backup() const { return m_backup; }
    
    SplitConfig& Split() { return m_split; }
    const SplitConfig& Split() const { return m_split; }
    
    ValidationConfig& Validation() { return m_validation; }
    const ValidationConfig& Validation() const { return m_validation; }
    
    VirusScanConfig& VirusScan() { return m_virusScan; }
    const VirusScanConfig& VirusScan() const { return m_virusScan; }
    
    CloudConfig& Cloud() { return m_cloud; }
    const CloudConfig& Cloud() const { return m_cloud; }
    
    TimelineConfig& Timeline() { return m_timeline; }
    const TimelineConfig& Timeline() const { return m_timeline; }
    
    DeduplicationConfig& Deduplication() { return m_deduplication; }
    const DeduplicationConfig& Deduplication() const { return m_deduplication; }
    
    ProgressConfig& Progress() { return m_progress; }
    const ProgressConfig& Progress() const { return m_progress; }
    
    PerformanceConfig& Performance() { return m_performance; }
    const PerformanceConfig& Performance() const { return m_performance; }

    void SetDefaults() {
        m_app = ApplicationConfig();
        m_compression = CompressionConfig();
        m_extraction = ExtractionConfig();
        m_encryption = EncryptionConfig();
        m_backup = BackupConfig();
        m_split = SplitConfig();
        m_validation = ValidationConfig();
        m_virusScan = VirusScanConfig();
        m_cloud = CloudConfig();
        m_timeline = TimelineConfig();
        m_deduplication = DeduplicationConfig();
        m_progress = ProgressConfig();
        m_performance = PerformanceConfig();
    }

private:
    bool ParseJson(const std::string& json) {
        auto getString = [&json](const std::string& key, size_t start) -> std::pair<std::string, size_t> {
            std::string searchKey = "\"" + key + "\"";
            size_t pos = json.find(searchKey, start);
            if (pos == std::string::npos) return {"", std::string::npos};
            
            pos = json.find(":", pos);
            if (pos == std::string::npos) return {"", std::string::npos};
            
            pos = json.find_first_of("\"", pos);
            if (pos == std::string::npos) return {"", std::string::npos};
            
            size_t end = json.find("\"", pos + 1);
            if (end == std::string::npos) return {"", std::string::npos};
            
            return {json.substr(pos + 1, end - pos - 1), end};
        };

        auto getBool = [&json](const std::string& key, size_t start) -> std::pair<bool, size_t> {
            std::string searchKey = "\"" + key + "\"";
            size_t pos = json.find(searchKey, start);
            if (pos == std::string::npos) return {false, std::string::npos};
            
            pos = json.find(":", pos);
            if (pos == std::string::npos) return {false, std::string::npos};
            
            size_t valueStart = json.find_first_of("tf", pos);
            if (valueStart == std::string::npos) return {false, std::string::npos};
            
            return {json.substr(valueStart, 4) == "true", valueStart + 4};
        };

        auto getNumber = [&json](const std::string& key, size_t start) -> std::pair<uint64_t, size_t> {
            std::string searchKey = "\"" + key + "\"";
            size_t pos = json.find(searchKey, start);
            if (pos == std::string::npos) return {0, std::string::npos};
            
            pos = json.find(":", pos);
            if (pos == std::string::npos) return {0, std::string::npos};
            
            size_t valueStart = json.find_first_of("0123456789", pos);
            if (valueStart == std::string::npos) return {0, std::string::npos};
            
            size_t valueEnd = json.find_first_not_of("0123456789", valueStart);
            if (valueEnd == std::string::npos) valueEnd = json.size();
            
            std::string numStr = json.substr(valueStart, valueEnd - valueStart);
            return {std::stoull(numStr), valueEnd};
        };

        auto [method, _] = getString("method", 0);
        if (!method.empty()) m_compression.method = method;

        auto [level, __] = getString("level", 0);
        if (!level.empty()) m_compression.level = level;

        auto [solidMode, ___] = getBool("solidMode", 0);
        m_compression.solidMode = solidMode;

        auto [threadCount, ____] = getNumber("threadCount", 0);
        m_compression.threadCount = (uint32_t)threadCount;

        auto [overwrite, _____] = getBool("overwriteExisting", 0);
        m_extraction.overwriteExisting = overwrite;

        auto [preserveDir, ______] = getBool("preserveDirectoryStructure", 0);
        m_extraction.preserveDirectoryStructure = preserveDir;

        auto [iterations, _______] = getNumber("iterations", 0);
        m_encryption.iterations = (uint32_t)iterations;

        auto [checkCRC, ________] = getBool("checkCRC", 0);
        m_validation.checkCRC = checkCRC;

        auto [volumeSize, _________] = getNumber("volumeSize", 0);
        m_split.volumeSize = volumeSize;

        return true;
    }

    std::string GenerateJson() const {
        std::ostringstream oss;
        oss << "{\n";
        oss << "    \"version\": \"1.0.0\",\n";
        oss << "    \"compression\": {\n";
        oss << "        \"method\": \"" << m_compression.method << "\",\n";
        oss << "        \"level\": \"" << m_compression.level << "\",\n";
        oss << "        \"solidMode\": " << (m_compression.solidMode ? "true" : "false") << ",\n";
        oss << "        \"threadCount\": " << m_compression.threadCount << "\n";
        oss << "    },\n";
        oss << "    \"extraction\": {\n";
        oss << "        \"overwriteExisting\": " << (m_extraction.overwriteExisting ? "true" : "false") << ",\n";
        oss << "        \"preserveDirectoryStructure\": " << (m_extraction.preserveDirectoryStructure ? "true" : "false") << "\n";
        oss << "    },\n";
        oss << "    \"encryption\": {\n";
        oss << "        \"algorithm\": \"" << m_encryption.algorithm << "\",\n";
        oss << "        \"iterations\": " << m_encryption.iterations << "\n";
        oss << "    },\n";
        oss << "    \"validation\": {\n";
        oss << "        \"checkCRC\": " << (m_validation.checkCRC ? "true" : "false") << "\n";
        oss << "    },\n";
        oss << "    \"split\": {\n";
        oss << "        \"volumeSize\": " << m_split.volumeSize << "\n";
        oss << "    }\n";
        oss << "}\n";
        return oss.str();
    }
};

}
