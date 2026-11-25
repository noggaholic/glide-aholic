#include "glide/pe.h"
#include "glide/crypto.h"
#include <Windows.h>
#include <cstring>
#include <utility>

namespace glide {
namespace {
uint16_t U16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}
uint32_t U32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}
bool Range(size_t offset, size_t count, size_t size) {
    return offset <= size && count <= size - offset;
}
void Put32(uint8_t* p, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
        p[i] = static_cast<uint8_t>(value >> (i * 8));
}
bool ReadModule(const void* module, size_t offset, void* out, size_t count) {
    const uintptr_t base = reinterpret_cast<uintptr_t>(module);
    if (offset > UINTPTR_MAX - base || count > UINTPTR_MAX - base - offset)
        return false;
    uintptr_t cursor = base + offset;
    const uintptr_t end = cursor + count;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION region = {};
        if (!VirtualQuery(reinterpret_cast<const void*>(cursor), &region, sizeof(region)) ||
            region.AllocationBase != module || region.Type != MEM_IMAGE || region.State != MEM_COMMIT ||
            (region.Protect & (PAGE_NOACCESS | PAGE_GUARD)) || !region.RegionSize)
            return false;
        const uintptr_t start = reinterpret_cast<uintptr_t>(region.BaseAddress);
        if (region.RegionSize > UINTPTR_MAX - start)
            return false;
        const uintptr_t next = start + region.RegionSize;
        if (next <= cursor)
            return false;
        cursor = next < end ? next : end;
    }
    SIZE_T read = 0;
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(base + offset), out, count, &read) &&
           read == count;
}
} // namespace
bool PeImage::FromModule(const void* module, PeImage& out) {
    if (!module)
        return false;
    uint8_t dos[64], nt[88];
    if (!ReadModule(module, 0, dos, sizeof(dos)) || U16(dos) != IMAGE_DOS_SIGNATURE)
        return false;
    const size_t pe = U32(dos + 60);
    if (pe < 64 || pe > 1024 * 1024 || !ReadModule(module, pe, nt, sizeof(nt)) || U32(nt) != IMAGE_NT_SIGNATURE)
        return false;
    const size_t sections = U16(nt + 6), optionalSize = U16(nt + 20);
    const size_t headerSize = U32(nt + 24 + 60), imageSize = U32(nt + 24 + 56);
    const size_t table = pe + 24 + optionalSize;
    if (!sections || sections > 96 || optionalSize < 96 || headerSize > 1024 * 1024 ||
        !Range(table, sections * 40, headerSize) || imageSize < headerSize || imageSize > 1024 * 1024 * 1024)
        return false;
    PeImage image;
    if (!image.file_.Resize(headerSize) || !ReadModule(module, 0, image.file_.Data(), headerSize))
        return false;
    for (size_t i = 0; i < sections; ++i) {
        uint8_t* section = image.file_.Data() + table + i * 40;
        const uint32_t virtualSize = U32(section + 8), rva = U32(section + 12), rawSize = U32(section + 16);
        const size_t mappedSize = virtualSize > rawSize ? virtualSize : rawSize;
        if (!Range(rva, mappedSize, imageSize))
            return false;
        const bool executable = (U32(section + 36) & IMAGE_SCN_MEM_EXECUTE) != 0;
        const size_t count = executable ? (virtualSize && virtualSize < rawSize ? virtualSize : rawSize) : 0;
        const size_t offset = image.file_.Size();
        if (count && (rva < headerSize || count > imageSize - offset || !image.file_.Resize(offset + count) ||
                      !ReadModule(module, rva, image.file_.Data() + offset, count)))
            return false;
        section = image.file_.Data() + table + i * 40;
        Put32(section + 16, static_cast<uint32_t>(count));
        Put32(section + 20, count ? static_cast<uint32_t>(offset) : 0);
    }
    if (!image.Parse())
        return false;
    wchar_t path[32768];
    const DWORD pathSize = GetModuleFileNameW(static_cast<HMODULE>(const_cast<void*>(module)), path, 32768);
    PeImage disk;
    if (!pathSize || pathSize >= 32768 || !FromFile(path, disk) || image.machine_ != disk.machine_ ||
        image.code_.size() != disk.code_.size())
        return false;
    // The on-disk SHA identifies the exact executable. Verify that its section
    // layout also describes this loaded module, whose code may be relocated.
    for (size_t i = 0; i < image.code_.size(); ++i) {
        const auto& loaded = image.code_[i];
        const auto& original = disk.code_[i];
        if (loaded.rva != original.rva || loaded.bytes.size != original.bytes.size || loaded.name != original.name ||
            loaded.characteristics != original.characteristics)
            return false;
    }
    image.fingerprint_ = disk.fingerprint_;
    image.sourceSize_ = disk.sourceSize_;
    out = std::move(image);
    return true;
}
// Cppcheck 2.18 does not follow this local Windows-handle destructor.
// cppcheck-suppress-begin resourceLeak
bool PeImage::FromFile(const wchar_t* path, PeImage& out) {
    if (!path)
        return false;
    HANDLE file =
        CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;
    struct Close {
        HANDLE value;
        ~Close() {
            CloseHandle(value);
        }
    } close{file};
    LARGE_INTEGER length;
    if (!GetFileSizeEx(file, &length) || length.QuadPart < 64 || length.QuadPart > 1024LL * 1024 * 1024)
        return false;
    PeImage image;
    const size_t size = static_cast<size_t>(length.QuadPart);
    if (!image.file_.Reserve(size))
        return false;
    uint8_t chunk[65536];
    size_t remaining = size;
    while (remaining) {
        const DWORD want = static_cast<DWORD>(remaining < sizeof(chunk) ? remaining : sizeof(chunk));
        DWORD got = 0;
        if (!ReadFile(file, chunk, want, &got, nullptr) || got != want || !image.file_.Append(ByteView(chunk, got)))
            return false;
        remaining -= got;
    }
    if (!image.Parse() || !Sha256(image.file_.View(), image.fingerprint_.data()))
        return false;
    image.sourceSize_ = size;
    out = std::move(image);
    return true;
}
// cppcheck-suppress-end resourceLeak

bool PeImage::FromBytes(ByteView file, PeImage& out) {
    PeImage image;
    if (!image.file_.Append(file) || !image.Parse() || !Sha256(file, image.fingerprint_.data()))
        return false;
    image.sourceSize_ = file.size;
    out = std::move(image);
    return true;
}
bool PeImage::Parse() {
    const uint8_t* data = file_.Data();
    const size_t size = file_.Size();
    if (size < 64 || U16(data) != IMAGE_DOS_SIGNATURE)
        return false;
    const size_t pe = U32(data + 60);
    if (pe < 64 || !Range(pe, 24, size) || U32(data + pe) != IMAGE_NT_SIGNATURE)
        return false;
    const uint16_t machine = U16(data + pe + 4);
    if (machine != IMAGE_FILE_MACHINE_I386 && machine != IMAGE_FILE_MACHINE_AMD64)
        return false;
    machine_ = machine == IMAGE_FILE_MACHINE_I386 ? Bitness::X86 : Bitness::X64;
    const size_t sectionCount = U16(data + pe + 6);
    const size_t optionalSize = U16(data + pe + 20);
    const size_t optional = pe + 24;
    const size_t minimum = machine_ == Bitness::X86 ? 96 : 112;
    if (sectionCount == 0 || sectionCount > 96 || optionalSize < minimum || !Range(optional, optionalSize, size))
        return false;
    if (U16(data + optional) != (machine_ == Bitness::X86 ? 0x10b : 0x20b))
        return false;
    const uint32_t imageSize = U32(data + optional + 56);
    const uint32_t headerSize = U32(data + optional + 60);
    const size_t table = optional + optionalSize;
    if (!Range(table, sectionCount * 40, size) || headerSize > size || headerSize < table + sectionCount * 40 ||
        imageSize < headerSize)
        return false;
    for (size_t i = 0; i < sectionCount; ++i) {
        const uint8_t* section = data + table + i * 40;
        const uint32_t virtualSize = U32(section + 8), rva = U32(section + 12);
        const size_t rawSize = U32(section + 16), raw = U32(section + 20);
        const uint32_t flags = U32(section + 36);
        const size_t mappedSize = virtualSize > rawSize ? virtualSize : rawSize;
        if (!Range(rva, mappedSize, imageSize) || (rawSize && (raw < headerSize || !Range(raw, rawSize, size))))
            return false;
        if (!(flags & IMAGE_SCN_MEM_EXECUTE) || !rawSize)
            continue;
        const size_t codeSize = virtualSize && virtualSize < rawSize ? virtualSize : rawSize;
        CodeSection code{ByteView(data + raw, codeSize), rva};
        std::memcpy(code.name.data(), section, 8);
        code.characteristics = flags;
        code_.push_back(code);
    }
    return !code_.empty();
}
} // namespace glide
