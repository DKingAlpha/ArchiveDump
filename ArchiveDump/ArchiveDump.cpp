#include "RADR.hpp"
#include "CR2W.hpp"

#include <stdio.h>
#include <filesystem>
#include <assert.h>
#include <string.h>
#include <string>
#include <map>
#include <vector>
#include <fstream>


#include <Windows.h>

using namespace std;

struct DumpFlags {
    bool name;
    bool impt;
    bool prop;
    bool expt;
    bool buffer;
    bool embeded;
};

int extract_radr_archive(filesystem::path filepath, filesystem::path dump_path)
{
        const wstring map_name = L"RDAR_" + wstring(filepath.stem().c_str());

        const auto filesize = filesystem::file_size(filepath);
        const auto low_filesize = uint32_t(filesize & UINT32_MAX);
        const auto high_filesize = uint32_t((filesize >> 32) & UINT32_MAX);

        auto file_handle = CreateFile(filepath.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
        if (file_handle == INVALID_HANDLE_VALUE)
        {
            printf("Could not open file: %d\n", GetLastError());
            return 1;
        }
        auto hMapFile = CreateFileMapping(file_handle, NULL, PAGE_READONLY, high_filesize, low_filesize, map_name.c_str());
        if (hMapFile == NULL)
        {
            printf("Could not create file mapping object: %d\n", GetLastError());
            CloseHandle(file_handle);
            return 1;
        }
        auto file_content = MapViewOfFile(hMapFile, FILE_MAP_READ, 0, 0, filesize);
        if (file_content == NULL)
        {
            printf("Could not map view of file: %d\n", GetLastError());
            CloseHandle(hMapFile);
            CloseHandle(file_handle);
            return 1;
        }
        
        DumpFlags flag{
            .buffer = true,
        };
        
        
        filesystem::create_directories(dump_path);
        
        RedArchive archive(file_content);
        printf("========== RADR Archive: %S ==========\n", filepath.stem().c_str());
        for (uint32_t i = 0; i < archive.fileTable->fileEntryCount; i++)
        {
            RedArchiveFile f = archive.GetFile(i);

            {
                auto savepath = dump_path / filesystem::path(to_string(f.entry.id));
                ofstream of(savepath.string());
                of.write(reinterpret_cast<const char*>(f.data.data()), f.data.size());
            }
            
            printf("--------- Extract %s : %llu ---------\n", (f.compressed ? "compressed  " : "uncompressed"), f.entry.id);

            if (!f.compressed)
                continue;

            uint32_t magic = *reinterpret_cast<uint32_t*>(f.data.data());
            if (magic == 'W2RC') // CR2W
            {
                // unpack CR2W files
                auto cr2w = f.Get<CR2W>();
                #define ENT_OFFSET  ( reinterpret_cast<uintptr_t>(&ent) - reinterpret_cast<uintptr_t>(cr2w))

                if (flag.name)
                {
                    for (auto&& ent : cr2w->entries<CR2WName>())
                    {
                        printf("[Name] %p %s, %x\n", ENT_OFFSET, ent.GetName(cr2w), ent.hash);
                    }
                }
                if (flag.impt)
                {
                    for (auto&& ent : cr2w->entries<CR2WImport>())
                    {
                        if (!ent.flags)
                            continue;
                        printf("[Import] %p className: %s, depotPath: %s, flags: %x\n",
                            ENT_OFFSET,
                            ent.GetTypeName(cr2w),
                            ent.GetDepotPath(cr2w),
                            ent.flags);
                    }
                }

                if (flag.prop)
                {
                    for (auto&& ent : cr2w->entries<CR2WProperty>())
                    {
                        printf("[Property] %p className: %s, propertyName: %s\n", ENT_OFFSET, ent.GetTypeName(cr2w), ent.GetPropertyName(cr2w));
                    }
                }

                if (flag.expt)
                {
                    for (auto&& ent : cr2w->entries<CR2WExport>())
                    {
                        printf("[Export]: %p %s\n", ENT_OFFSET, ent.GetName(cr2w).c_str());
                        auto parent = ent.GetParent(cr2w);
                        if (parent)
                            printf("    [Parent]: %s\n", parent->GetName(cr2w).c_str());
                        for (auto&& child : ent.GetChildren(cr2w))
                        {
                            printf("    [Child]: %s\n", child->GetName(cr2w).c_str());
                        }
                    }
                }

                if (flag.buffer)
                {
                    for (auto&& ent : cr2w->entries<CR2WBuffer>())
                    {
                        printf("[Buffer]: %p  index: %d, crc: %x, size: %d/%d\n", ENT_OFFSET, ent.index, ent.crc32, ent.diskSize, ent.memSize);
                        auto buf_savepath = dump_path / filesystem::path(to_string(f.entry.id) +  "_buf_" + to_string(ent.index));
                        filesystem::create_directories(buf_savepath.parent_path());
                        ofstream bof(buf_savepath.string());
                        bof.write(cr2w->Get<const char>(ent.offset), ent.diskSize);
                    }
                }

                // BROKEN
                if (flag.embeded)
                {
                    for (auto&& ent : cr2w->entries<CR2WEmbedded>())
                    {
                        const char* x = "";
                        auto imp = ent.GetImport(cr2w);
                        if (imp)
                            x = imp->GetDepotPath(cr2w);
                        printf("[Embedded]: %p  size: %d, path:%s, importDepotPath:%s\n", ENT_OFFSET, ent.dataSize, ent.GetPath(cr2w), x);
                    }
                }
            }
            else {
                assert(0);
            }

        }

        UnmapViewOfFile(file_content);
        CloseHandle(hMapFile);
        CloseHandle(file_handle);
        return 0;
}

int main(int argc, const char** argv)
{
    OodleHelper::Initialize();

    if (argc <= 1) {
        printf("Usage:\n"
                "    %S InputFileOrDir [OutputDir]\n\n"
                "    * default value of OutputDir is filename\n", filesystem::path(argv[0]).filename().c_str());
        return 1;
    }

    filesystem::path filepath = argv[1];
    if (!filesystem::exists(filepath))
    {
        printf("File dor dir not exists\n");
        return 1;
    }

    if (filesystem::is_regular_file(filepath))
    {
        filesystem::path default_dump_path = filepath.stem();
        filesystem::path dump_path;
        if (argc >= 3)
            dump_path = argv[2];
        else
            dump_path = default_dump_path;
        return extract_radr_archive(filepath, dump_path);
    }

    for (const auto& fp : filesystem::directory_iterator(filepath))
    {
        filesystem::path default_dump_path = "dump";
        filesystem::path dump_path;
        if (argc >= 3)
            dump_path = argv[2];
        else
            dump_path = default_dump_path;
        const filesystem::path& fpath = fp.path();
        filesystem::path dpath = dump_path / filepath.stem();
        if (fpath.extension() != ".archive")
            continue;
        extract_radr_archive(fpath, dpath);
    }
    return 0;
}
