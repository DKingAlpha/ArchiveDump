#pragma once

#include <stdint.h>
#include <lz4.h>
#include <vector>
#include <assert.h>

#include <Windows.h>

namespace zlib {
    #include <zlib.h>
}

// ---- Reversed from official DumpArchive.exe ----

#pragma pack(push, RDAR, 1)
struct RedArchiveHeader {
    uint32_t magic;
    uint32_t version;
    uint64_t indexPosition;
    uint32_t indexSize;
    uint64_t debugPosition;
    uint32_t debugSize;
    uint64_t totalFileSize;
};

struct RedArchiveDebug {
    uint32_t buildMachine;
    char computerName[64];
    char buildName[64];
};

struct RedArchiveIndex {
    uint32_t fileTableOffset;
    uint32_t fileTableSize;
};

struct RedArchiveEntry {
    // 0x38
    uint64_t id;
    uint64_t timestamp;
    uint32_t numInlineBufferSegments;
    uint32_t segmentsStart;
    uint32_t segmentsEnd;
    uint32_t resourceDependenciesStart;
    uint32_t resourceDependenciesEnd;
    uint8_t  hash[20];  // SHA-1
};

struct RedArchiveSegment {
    // 0x10
    uint64_t position;
    uint32_t sizeOnDisk;
    uint32_t sizeInMemory;
};

struct RedArchiveCompressed
{
    uint32_t magic;
    uint32_t uncomp_size;
    unsigned char data[0];
};

struct RedArchiveDependency {
    // 0x8
    uint64_t dependency;
};


struct RedArchiveFileTable {
    uint64_t crc64;
    uint32_t fileEntryCount;
    uint32_t fileSegmentCount;
    uint32_t resourceDependencyCount;
};

#pragma pack(pop, RDAR)

struct RedArchiveFile {
    std::vector<unsigned char> data;
    std::vector<uint64_t> dependencies;
    RedArchiveEntry entry;
    bool compressed;

    template<typename T>
    T* Get()
    {
        return reinterpret_cast<T*>(data.data());
    }
};



// -------------------- helpers -----------------------


typedef int64_t (__fastcall *Fn_OodleLZ_Decompress)(unsigned char* inputBuffer, int64_t inputBufferSize, unsigned char* outputBuffer, int64_t outputBufferSize,
    int64_t bFuzzSafe, int64_t bCheckCRC, int64_t verboseLevel, unsigned char* dictBackup, int64_t dictBackupSize, void* callback, int64_t a11, int64_t a12, int64_t a13, int64_t threadMode);

typedef int64_t (__fastcall *Fn_OodleLZ_Compress)(int64_t algorithm, unsigned char* inputBuffer, int64_t inputBufferSize, unsigned char* outputBuffer, int64_t outputBufferSize, 
    int64_t* a6, int64_t a7, int64_t a8, int64_t a9, int64_t compressLevel);

typedef int64_t(__fastcall *Fn_OodleCore_Plugins_SetPrintf)(void*);

/*
static void OodleLog(uint64_t x, const char * filepath, uint64_t linenumber, const char* fmt, ...)
{
    va_list argptr;
    printf("[LOG] %s:%lld ", filepath, linenumber);
    va_start(argptr, fmt);
    vprintf(fmt, argptr);
    va_end(argptr);
}
*/

class OodleHelper {
public:
    static void Initialize()
    {
        oo2 = LoadLibrary(L"oo2ext_7_win64.dll");
        assert(oo2 != 0);
        if (!oo2) return;
        OodleLZ_Decompress = reinterpret_cast<Fn_OodleLZ_Decompress>(GetProcAddress(oo2, "OodleLZ_Decompress"));
        OodleLZ_Compress = reinterpret_cast<Fn_OodleLZ_Compress>(GetProcAddress(oo2, "OodleLZ_Compress"));
        Fn_OodleCore_Plugins_SetPrintf OodleCore_Plugins_SetPrintf = reinterpret_cast<Fn_OodleCore_Plugins_SetPrintf>(GetProcAddress(oo2, "OodleCore_Plugins_SetPrintf"));
        // OodleCore_Plugins_SetPrintf(OodleLog);
        assert(OodleLZ_Decompress && OodleLZ_Compress );
    }

    static void Finalize()
    {
        if (oo2)
            FreeLibrary(oo2);
    }

    static int64_t Compress(int algo /* kraken=8 */, unsigned char* inputBuffer, int64_t inputBufferSize, unsigned char* outputBuffer, int64_t outputBufferSize, int64_t compressLevel=9)
    {
        return OodleLZ_Compress ? OodleLZ_Compress(algo, inputBuffer, inputBufferSize, outputBuffer, outputBufferSize, 0, 0, 0, 0, compressLevel) : 0;
    }

    static int64_t Decompress(unsigned char* inputBuffer, int64_t inputBufferSize, unsigned char* outputBuffer, int64_t outputBufferSize)
    {
        return OodleLZ_Decompress ? OodleLZ_Decompress(inputBuffer, inputBufferSize, outputBuffer, outputBufferSize,
            0, 0, 0 /* loglevel */, 0, 0, 0, 0, 0, 0, 0) : 0;
    }

private:
    inline static HMODULE oo2;
    inline static Fn_OodleLZ_Decompress OodleLZ_Decompress;
    inline static Fn_OodleLZ_Compress OodleLZ_Compress;
};


struct RedArchive {

public:
    RedArchive(void* buf)
    {
        header = Get<RedArchiveHeader>(0, buf);
        assert(header->magic == 'RADR');
        debug = Get<RedArchiveDebug>(sizeof(RedArchiveHeader));
        index = Get<RedArchiveIndex>(header->indexPosition);
        fileTable = Get<RedArchiveFileTable>(index->fileTableOffset, index);
        entry = Get<RedArchiveEntry>(sizeof(RedArchiveFileTable), fileTable);
        segment = Get<RedArchiveSegment>(sizeof(RedArchiveEntry) * fileTable->fileEntryCount, entry);
        dependency = Get<RedArchiveDependency>(sizeof(RedArchiveSegment) * fileTable->fileSegmentCount, segment);
    }

    template<typename T>
    T* Get(intptr_t offset, void* rel=nullptr)
    {
        if (rel == nullptr)
            rel = header;
        return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(rel) + offset);
    }

    RedArchiveFile GetFile(uint32_t file_index)
    {
        std::vector<unsigned char> f;
        std::vector<uint64_t> dep;
        assert(file_index>=0 && file_index < fileTable->fileEntryCount);
        auto& fentry = entry[file_index];

        bool is_compressed = false;
        for (uint32_t seg_index = fentry.segmentsStart; seg_index < fentry.segmentsEnd; seg_index++)
        {
            auto& fseg = segment[seg_index];
            // uncompressed
            if (fseg.sizeInMemory == fseg.sizeOnDisk)
            {
                f.insert(f.end(), Get<unsigned char>(fseg.position), Get<unsigned char>(fseg.position + fseg.sizeOnDisk));
                continue;
            }

            // compressed
            auto arc = Get<RedArchiveCompressed>(fseg.position);
            unsigned char* decompressed = new unsigned char[fseg.sizeInMemory];
            memset(decompressed, 0, fseg.sizeInMemory);
            assert(arc->uncomp_size == fseg.sizeInMemory);
            int64_t decomp_len = 0;
            switch (arc->magic)
            {
            case 'KRAK':
                is_compressed = true;
                decomp_len = OodleHelper::Decompress(arc->data, fseg.sizeOnDisk - sizeof(RedArchiveCompressed), decompressed, fseg.sizeInMemory);
                break;
            case 'XLZ4':
                is_compressed = true;
                decomp_len = LZ4_decompress_safe(reinterpret_cast<const char*>(arc->data), reinterpret_cast<char*>(decompressed),
                    fseg.sizeOnDisk - sizeof(RedArchiveCompressed), fseg.sizeInMemory);
                break;
            case 'ZLIB':
            {
                is_compressed = true;
                zlib::uLongf out_size = fseg.sizeInMemory;
                int zlib_uncomp_ret = zlib::uncompress(decompressed, &out_size, arc->data, fseg.sizeOnDisk - sizeof(RedArchiveCompressed));
                if (zlib_uncomp_ret == Z_OK)
                    decomp_len = out_size;
                else
                    decomp_len = 0;
                break;
            }
            default:
                is_compressed = true;
                // unknown compression
                break;
            }
            assert(decomp_len == arc->uncomp_size);
            if (decomp_len)
            {
                f.insert(f.end(), decompressed, decompressed + decomp_len);
            }
            else
            {
                // error or warn ?
            }
            delete[] decompressed;
        }

        for (uint32_t dep_index = fentry.resourceDependenciesStart; dep_index < fentry.resourceDependenciesEnd; dep_index++)
        {
            auto& fdep = dependency[dep_index];
            dep.emplace_back(fdep.dependency);
        }
        return {f, dep, fentry, is_compressed};
    }

public:
    RedArchiveHeader* header = nullptr;
    RedArchiveDebug* debug = nullptr;
    RedArchiveIndex* index = nullptr;
    RedArchiveFileTable* fileTable = nullptr;
    RedArchiveEntry* entry = nullptr;
    RedArchiveSegment* segment = nullptr;
    RedArchiveDependency* dependency = nullptr;
};
