#include <stdint.h>

// ---- CREDIT ----
// WolvenKit: for CR2W structure

#pragma pack(push, CR2W, 1)

struct CR2WHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t flags;
    uint64_t timestamp;
    uint32_t buildVersion;
    uint32_t fileSize;
    uint32_t bufferSize;
    uint32_t crc32;
    uint32_t numChunks;
};

struct CR2WTable {
    uint32_t pos;
    uint32_t count;
    uint32_t crc32;
};

// --------------------
struct CR2WName {
    uint32_t value;
    uint32_t hash;
};

struct CR2WImport {
    uint32_t depotPath;
    uint16_t className;
    uint16_t flags;
};

// --------------------
template<typename T>
class EntryIteratable {
public:
    EntryIteratable(T* p, int count): pstart(p), pend(p+count+1) {}
    T* begin() const { return pstart; }
    T* end() const { return pend; };
private:
    T* pstart = nullptr;
    T* pend = nullptr;
};

struct CR2W {
    CR2WHeader header;
    CR2WTable  tables[10];

public:
    template<typename T>
    T* Get(intptr_t offset, void* rel=nullptr)
    {
        if (rel == nullptr)
            rel = this;
        return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(rel) + offset);
    }
    
    template<typename T>
    T* GetTableEntry(int table_index, int entry_index) { return Get<T>(tables[table_index].pos) + entry_index; }

    char* GetStringAtOffset(int offset) { return Get<char>(tables[0].pos) + offset; }

    template<typename T>
    EntryIteratable<T> entries(int index)
    {
        return EntryIteratable<T>(Get<T>(tables[index].pos), tables[index].count);
    }
};

#pragma pack(pop, CR2W)
