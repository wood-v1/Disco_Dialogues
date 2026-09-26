// Exercise the actual hook installers against relocated copies of installed PEs.
// No game code is executed and the installed files are never modified.
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <memory>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); std::abort(); } } while (0)

struct Image {
    BYTE* data;
    IMAGE_NT_HEADERS32* nt;
    explicit Image(const std::filesystem::path& path) {
        std::ifstream input(path, std::ios::binary);
        CHECK(input.good());
        std::vector<char> file{std::istreambuf_iterator<char>(input), {}};
        const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(file.data());
        const auto headers = reinterpret_cast<const IMAGE_NT_HEADERS32*>(file.data() + dos->e_lfanew);
        CHECK(headers->Signature == IMAGE_NT_SIGNATURE);
        CHECK(headers->FileHeader.Machine == IMAGE_FILE_MACHINE_I386);
        data = static_cast<BYTE*>(::VirtualAlloc(nullptr, headers->OptionalHeader.SizeOfImage,
            MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        CHECK(data);
        std::memcpy(data, file.data(), headers->OptionalHeader.SizeOfHeaders);
        nt = reinterpret_cast<IMAGE_NT_HEADERS32*>(data + dos->e_lfanew);
        auto section = IMAGE_FIRST_SECTION(headers);
        for (unsigned i = 0; i < headers->FileHeader.NumberOfSections; ++i, ++section)
            std::memcpy(data + section->VirtualAddress, file.data() + section->PointerToRawData,
                section->SizeOfRawData);
        const DWORD delta = reinterpret_cast<DWORD>(data) - headers->OptionalHeader.ImageBase;
        const auto directory = headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
        CHECK(directory.Size);
        for (DWORD offset = 0; offset < directory.Size;) {
            const auto block = reinterpret_cast<const IMAGE_BASE_RELOCATION*>(data + directory.VirtualAddress + offset);
            CHECK(block->SizeOfBlock >= sizeof(*block));
            const auto entries = reinterpret_cast<const WORD*>(block + 1);
            for (unsigned i = 0; i < (block->SizeOfBlock - sizeof(*block)) / sizeof(WORD); ++i) {
                const auto type = entries[i] >> 12;
                CHECK(type == IMAGE_REL_BASED_ABSOLUTE || type == IMAGE_REL_BASED_HIGHLOW);
                if (type == IMAGE_REL_BASED_HIGHLOW)
                    *reinterpret_cast<DWORD*>(data + block->VirtualAddress + (entries[i] & 0xfff)) += delta;
            }
            offset += block->SizeOfBlock;
        }
    }
    ~Image() { ::VirtualFree(data, 0, MEM_RELEASE); }
};

std::unique_ptr<Image> game, engine, ui, sound;
HMODULE TestGetModuleHandleW(LPCWSTR name) {
    Image* result = !name ? game.get() :
        !std::wcscmp(name, L"Engine.dll") ? engine.get() :
        !std::wcscmp(name, L"UI.dll") ? ui.get() :
        !std::wcscmp(name, L"Sound.dll") ? sound.get() : nullptr;
    return result ? reinterpret_cast<HMODULE>(result->data) : nullptr;
}
#define GetModuleHandleW TestGetModuleHandleW
#include HD_HOOK_SOURCE
#undef GetModuleHandleW

int wmain(int argc, wchar_t** argv) {
    CHECK(argc == 2);
    const std::filesystem::path root(argv[1]);
    game = std::make_unique<Image>(root / L"Game.exe");
    engine = std::make_unique<Image>(root / L"Engine.dll");
    ui = std::make_unique<Image>(root / L"UI.dll");
    sound = std::make_unique<Image>(root / L"Sound.dll");
    using namespace oynon::runtime;
    const bool gog = game->nt->FileHeader.TimeDateStamp == 0x569d011a;
#if HD_HOOK_KIND == 1
    const auto install = InstallCamera;
    Image& guarded = *game;
    const DWORD byteRva = gog ? 0x153766 : 0x153706;
    void** slot = reinterpret_cast<void**>(game->data + (gog ? 0x37ca44 : 0x37ba44));
#elif HD_HOOK_KIND == 2
    const auto install = InstallUIExecute;
    Image& guarded = *ui;
    const DWORD byteRva = 0x327a5;
    void** slot = reinterpret_cast<void**>(ui->data + 0xa4e00);
#else
    const auto install = InstallScriptAudio;
    Image& guarded = *game;
    const DWORD byteRva = gog ? 0x8331f : 0x832bf;
    void** slot = reinterpret_cast<void**>(game->data + (gog ? 0x36bf60 : 0x36af60));
#endif
    void* original = *slot;
    const auto stamp = guarded.nt->FileHeader.TimeDateStamp;
    guarded.nt->FileHeader.TimeDateStamp = 0;
    CHECK(!install()); CHECK(*slot == original);
    guarded.nt->FileHeader.TimeDateStamp = stamp;
    guarded.data[byteRva] ^= 1;
    CHECK(!install()); CHECK(*slot == original);
    guarded.data[byteRva] ^= 1;
    *slot = nullptr;
    CHECK(!install()); CHECK(*slot == nullptr);
    *slot = original;
    CHECK(install()); CHECK(*slot != original);
    CHECK(!install());
    std::printf("PASS %s: real PE hook installation; unknown build, altered code and occupied slot rejected\n",
        gog ? "GOG" : "Steam");
}
