#include "assets.h"
#include "glide/installation_data.h"
#include "glide/files.h"
#include <algorithm>
#include <set>

namespace glide {
namespace april14075 {
namespace {
bool Text(Reader& reader, std::string& text) {
    uint16_t length = 0;
    ByteView value;
    if (!reader.U16(length) || !length || length > 255 || !reader.Bytes(length, value))
        return false;
    text.assign(reinterpret_cast<const char*>(value.data), value.size);
    std::wstring checked;
    return Utf8ToWide(text, checked);
}
} // namespace
const ArchiveResolution* Assets::Find(uint32_t reference) const {
    const auto at = std::lower_bound(resolutions_.begin(), resolutions_.end(), reference,
                                     [](const ArchiveResolution& row, uint32_t id) { return row.reference < id; });
    return at != resolutions_.end() && at->reference == reference ? &*at : nullptr;
}
bool Assets::Open(const std::wstring& path, std::string& error) {
    InstallationData data;
    Buffer bytes;
    if (!data.Open(path, 14075, error) || !data.Read("client/archive-resolutions.bin", bytes, error))
        return false;
    error = "Invalid local April archive resolution table";
    if (!bytes.Size() || bytes.Size() > 4 * 1024 * 1024)
        return false;
    std::unique_ptr<DatArchive> candidate(new DatArchive);
    if (!candidate->Open(path)) {
        error = "Cannot read April archive index";
        return false;
    }
    std::vector<ArchiveResolution> rows;
    std::set<uint32_t> targets;
    Reader reader(bytes.View());
    while (reader.Remaining()) {
        ArchiveResolution row;
        if (rows.size() >= 100000 || !reader.U32(row.reference) || !reader.U32(row.file) ||
            !reader.U16(row.referenceWidth) || !reader.U16(row.referenceHeight) || !reader.U16(row.fileWidth) ||
            !reader.U16(row.fileHeight) || !Text(reader, row.slot) || !Text(reader, row.resource) || !row.reference ||
            !row.file || row.fileWidth != row.referenceWidth * 2u || row.fileHeight != row.referenceHeight * 2u ||
            (!rows.empty() && rows.back().reference >= row.reference) || !targets.insert(row.file).second)
            return false;
        if (!candidate->Find(row.reference) || !candidate->Find(row.file)) {
            error =
                "Missing April archive resource: " + std::to_string(row.reference) + " -> " + std::to_string(row.file);
            return false;
        }
        rows.push_back(std::move(row));
    }
    std::lock_guard<std::mutex> guard(mutex_);
    resolutions_ = std::move(rows);
    archive_ = std::move(candidate);
    error.clear();
    return true;
}
bool Assets::Contains(uint32_t id) {
    std::lock_guard<std::mutex> guard(mutex_);
    return id && archive_ && archive_->Find(id) != 0;
}
int Assets::Availability(uint32_t reference, int stockResult, uint32_t* file) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!stockResult && file && reference && archive_ && archive_->Find(reference)) {
        const auto* row = Find(reference);
        *file = row ? row->file : reference;
        return 1;
    }
    return stockResult;
}
int Assets::ResolveMap(uint32_t reference, int stockResult, uint32_t* file, bool worldJoined) {
    std::lock_guard<std::mutex> guard(mutex_);
    const auto* row = Find(reference);
    if (!row && stockResult)
        row = Find(file ? *file : 0);
    if (!row || row->resource != "client-map" || !worldJoined || !file || !archive_ || !archive_->Find(row->file))
        return stockResult;
    *file = row->file;
    return 1;
}
uint32_t Assets::DecodeMap(uint32_t decoded, bool worldJoined) {
    std::lock_guard<std::mutex> guard(mutex_);
    const auto* row = Find(decoded);
    return row && row->resource == "client-map" && worldJoined && archive_ && archive_->Find(row->file) ? row->file
                                                                                                        : decoded;
}
} // namespace april14075
} // namespace glide
