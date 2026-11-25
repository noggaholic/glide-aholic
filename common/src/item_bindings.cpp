#include "glide/item_bindings.h"
#include "glide/files.h"
#include "glide/crypto.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <map>
#include <mutex>
#include <vector>
namespace glide {
namespace {
struct Table {
    std::vector<ItemBinding> rows;
    std::array<uint8_t, 32> hash{};
};
std::mutex mutex;
std::map<unsigned, Table> tables;
} // namespace
bool LoadItemBindings(const std::wstring& file, unsigned build, std::string& error) {
    Buffer bytes;
    Table table;
    error = "Missing or invalid local item binding table";
    if (!ReadFileBytes(file, 4 * 1024 * 1024, bytes) || !Sha256(bytes.View(), table.hash.data()))
        return false;
    Reader reader(bytes.View());
    ByteView magic;
    uint32_t selected = 0, count = 0;
    if (!reader.Bytes(8, magic) || std::memcmp(magic.data, "GLDIB001", 8) || !reader.U32(selected) || !build ||
        selected != build || !reader.U32(count) || !count || count > 100000)
        return false;
    for (uint32_t i = 0; i < count; ++i) {
        ItemBinding row;
        if (!reader.U32(row.definition) || !row.definition || !reader.U8(row.onUse) || !reader.U8(row.onAcquisition) ||
            (!table.rows.empty() && table.rows.back().definition >= row.definition))
            return false;
        table.rows.push_back(row);
    }
    if (reader.Remaining())
        return false;
    std::lock_guard<std::mutex> guard(mutex);
    const auto previous = tables.find(build);
    if (previous != tables.end() && previous->second.hash != table.hash) {
        error = "Different item binding data already loaded";
        return false;
    }
    if (previous == tables.end())
        tables.emplace(build, std::move(table));
    error.clear();
    return true;
}
const ItemBinding* FindItemBinding(unsigned build, uint32_t definition) {
    std::lock_guard<std::mutex> guard(mutex);
    const auto table = tables.find(build);
    if (table == tables.end())
        return nullptr;
    const auto& rows = table->second.rows;
    const auto row = std::lower_bound(rows.begin(), rows.end(), definition,
                                      [](const ItemBinding& value, uint32_t id) { return value.definition < id; });
    return row != rows.end() && row->definition == definition ? &*row : nullptr;
}
} // namespace glide
