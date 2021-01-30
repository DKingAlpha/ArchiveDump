#include <filesystem>
#include <assert.h>
#include <string.h>
#include <string>
#include <map>
#include <vector>
#include <fstream>

#include "RADR.hpp"
#include "CR2W.hpp"

#include <Windows.h>

using namespace std;

int main(int argc, const char** argv)
{
    OodleHelper::Initialize();

    if (argc <= 1) {
        printf("Usage:\n"
                "    %S InputFile [OutputDir]\n\n"
                "    * default value of OutputDir is filename\n", filesystem::path(argv[0]).filename().c_str());
        return 1;
    }
    filesystem::path filepath = argv[1];
    filesystem::path dump_path = filepath.stem();
    if (argc >= 3) { dump_path = argv[2]; }

    //for (const auto& fp : filesystem::directory_iterator("G:\\Games\\Cyberpunk 2077\\archive\\pc\\content"))
    {
        //const filesystem::path& filepath = fp.path();
        
        const wstring map_name = L"RDAR_" + wstring(filepath.filename().c_str());

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
        
        printf("%S\n", filepath.c_str());

        RedArchive archive(file_content);
        map<uint64_t, RedArchiveFile> filemap;
        for (uint32_t i = 0; i < archive.fileTable->fileEntryCount; i++)
        {
            auto f = archive.GetFile(i);
            if (!f.compressed) continue;
            filemap[f.entry.id] = f;
        }

        filesystem::create_directories(dump_path);

        for (auto&& f : filemap)
        {
            if (!f.second.compressed) continue;
            {
                auto savepath = dump_path / filesystem::path(to_string(f.first));
                ofstream of(savepath.string());
                of.write(reinterpret_cast<const char*>(f.second.data.data()), f.second.data.size());
            }

            // -----------  WIP  --------------

            // unpack CR2W files
            auto cr2w = f.second.Get<CR2W>();
            assert(cr2w->header.magic == 'W2RC'); // CR2W
            // printf("--------- %llu  ---------\n", f.first);
            // for (auto&& ent : cr2w->entries<CR2WName>(1))
            {
                // printf("[Name] %d, %x\n", ent.value, ent.hash);
            }
        }

        UnmapViewOfFile(file_content);
        CloseHandle(hMapFile);
        CloseHandle(file_handle);

    }
    return 0;
}
