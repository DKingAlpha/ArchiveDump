#pragma once

#include <stdint.h>
#include <string>
#include <vector>
#include <assert.h>

// ---- CREDIT ----
// WolvenKit: for CR2W structure



// --------------------
template<typename T>
class IteratableStructs {
public:
    IteratableStructs(T* p, uint32_t count) : pstart(p), pend(p + count) {}
    IteratableStructs() : pstart(nullptr), pend(nullptr) {}
    T* begin() const { return pstart; }
    T* end() const { return pend; };
private:
    T* pstart = nullptr;
    T* pend = nullptr;
};

// --------------------

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

struct CR2WTableIndex {
    uint32_t pos;
    uint32_t count;
    uint32_t crc32;
};

// ---- forward declaration ----
struct CR2WStrings;
struct CR2WName;
struct CR2WImport;
struct CR2WProperty;
struct CR2WExport;
struct CR2WBuffer;
struct CR2WEmbedded;

struct CR2W {
    CR2WHeader header;
    CR2WTableIndex  tables[10];

public:
    template<typename T>
    T* Get(intptr_t offset, void* rel = nullptr)
    {
        if (rel == nullptr)
            rel = this;
        return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(rel) + offset);
    }

    template<typename T>
    T* GetTableEntry(int table_index, int entry_index) { return Get<T>(tables[table_index].pos) + entry_index; }

    template<typename T>
    constexpr int get_index_of_table()
    {
        constexpr int index = 
            (std::is_same<T, CR2WStrings>::value)  ? 0 :
            (std::is_same<T, CR2WName>::value)     ? 1 :
            (std::is_same<T, CR2WImport>::value)   ? 2 :
            (std::is_same<T, CR2WProperty>::value) ? 3 :
            (std::is_same<T, CR2WExport>::value)   ? 4 :
            (std::is_same<T, CR2WBuffer>::value)   ? 5 :
            (std::is_same<T, CR2WEmbedded>::value) ? 6 :
            -1;
        static_assert(index != -1);
        return index;
    }

    template<typename T>
    IteratableStructs<T> entries()
    {
        int table_index = get_index_of_table<T>();
        uint32_t table_pos = tables[table_index].pos;
        uint32_t table_ent_count = tables[table_index].count;
        if (table_pos && table_ent_count > 0)
            return IteratableStructs<T> { Get<T>(table_pos), table_ent_count};
        else
            return IteratableStructs<T> {};
    }

    template<typename T>
    T* entry(uint32_t index=0)
    {
        int table_index = get_index_of_table<T>();
        uint32_t table_pos = tables[table_index].pos;
        uint32_t table_ent_count = tables[table_index].count;
        if (table_pos && table_ent_count > 0 && index < table_ent_count)
            return Get<T>(table_pos)+ index;
        else
        {
            assert(0);
            return nullptr;
        }
    }

};

// --------------------
struct CR2WStrings {
    // char strings[0];       // sequential strings splited by null byte
    const char* const GetString(int offset)
    {
        return reinterpret_cast<char*>(this) + offset;
    }
};

struct CR2WName {
    uint32_t value;
    uint32_t hash;
public:
    const char* const GetName(CR2W* ctx) { return ctx->entry<CR2WStrings>()->GetString(value); }
};

struct CR2WImport {
    uint32_t depotPath;
    uint16_t className;
    uint16_t flags;

    const char* const GetTypeName(CR2W* ctx)  { return ctx->entry<CR2WName>(className)->GetName(ctx); }
    const char* const GetDepotPath(CR2W* ctx) { return ctx->entry<CR2WStrings>()->GetString(depotPath); }
    int64_t GetIndex(CR2W* ctx)
    {
        int64_t self_index = this - ctx->entry<CR2WImport>();
        assert(self_index >= 0);
        return self_index;
    }
};

struct CR2WProperty {
    uint16_t className;
    uint16_t classFlags;
    uint16_t propertyName;
    uint16_t propertyFlags;
    uint64_t hash;

    const char* const GetTypeName(CR2W* ctx) { return ctx->entry<CR2WName>(className)->GetName(ctx); }
    const char* const GetPropertyName(CR2W* ctx) { return ctx->entry<CR2WStrings>()->GetString(propertyName); }
};

struct CR2WExport {
    uint16_t className;
    uint16_t objectFlags;   // 0x2000: cooked
    uint32_t parentID;      // parentID=1: first CR2WExport, parentID=0: no parent
    uint32_t dataSize;
    uint32_t dataOffset;
    uint32_t tpl;           // template
    uint32_t crc32;
    
    const char* const GetTypeName(CR2W* ctx) { return ctx->entry<CR2WName>(className)->GetName(ctx); }
    std::string GetName(CR2W* ctx) { return std::string(GetTypeName(ctx)) + "#" + std::to_string(GetIndex(ctx)); }

    int64_t GetIndex(CR2W* ctx)
    {
        int64_t self_index = this - ctx->entry<CR2WExport>();
        assert(self_index >= 0);
        return self_index;
    }

    CR2WExport* GetParent(CR2W* ctx)
    {
        if (parentID)
            return ctx->entry<CR2WExport>(parentID - 1);
        else
            return nullptr;
    }

    std::vector<CR2WExport*> GetChildren(CR2W* ctx)
    {
        std::vector<CR2WExport*> children;
        int64_t self_index = this - ctx->entry<CR2WExport>();
        assert(self_index >= 0);
        for (auto&& ent : ctx->entries<CR2WExport>())
        {
            if (ent.parentID  == self_index + 1)    // 1-based
                children.push_back(&ent);
        }
        return children;
    }
};

struct CR2WBuffer {
    uint32_t flags;
    uint32_t index;
    uint32_t offset;
    uint32_t diskSize;
    uint32_t memSize;
    uint32_t crc32;
};


// BORKEN
struct CR2WEmbedded {
    uint32_t importIndex;   // 1: first CR2WImport, 0: no import
    uint32_t path;
    uint64_t pathHash;
    uint32_t dataOffset;
    uint32_t dataSize;

    CR2WImport* GetImport(CR2W* ctx)
    {
        if (importIndex)
            return ctx->entry<CR2WImport>(importIndex - 1);
        else
            return nullptr;
    }
    const char* const GetPath(CR2W* ctx) { return ctx->entry<CR2WStrings>()->GetString(path); }
};


#pragma pack(pop, CR2W)