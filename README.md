# ArchiveDump
An unofficial implementation of Cyberpunk 2077 archive dumper, by reversing official ArchiveDump.exe

* Official ArchiveDump.exe: print archive layout, without actual dumping
* This project: Uncompressed archive and dump all files.

**WIP**: parsing CR2W files
**What's done**: dump files from *.archive

## What's the difference between this and WolvenKit
* [C++20 hpp](ArchiveDump/RADR.hpp) for pure native plugins
* this project does not parse CR2W files extracted from *.archive, neither repacking back.

## How to compile and test?
1. install `vcpkg` and integrate with Visual Studio
2. `vcpkg.exe install --triplet=x64-windows-static lz4 zlib`
3. update `main` function
4. copy `oo2ext_7_win64.dll` from game to program working directory
5. build&run in Visual Studio

## Credit
WolvenKit for CR2W file structure
