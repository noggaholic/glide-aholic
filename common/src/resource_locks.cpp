#include "glide/resource_locks.h"
#include "glide/runtime_manifest.h"
#include <cwchar>
#include <atomic>
#include <algorithm>

namespace glide {
struct ReadOnlyResources::ParallelTree {
    static const unsigned MaximumWorkers = 16;
    struct Worker {
        ParallelTree* tree = nullptr;
        unsigned index = 0;
    };
    const std::vector<std::wstring>* paths = nullptr;
    std::atomic<size_t> next{0};
    std::atomic<bool> failed{false};
    ReadOnlyResources resources[MaximumWorkers];
    Worker workers[MaximumWorkers];
};
DWORD WINAPI ReadOnlyResources::LockChildTrees(void* parameter) {
    auto& worker = *static_cast<ParallelTree::Worker*>(parameter);
    auto& tree = *worker.tree;
    while (!tree.failed) {
        const size_t index = tree.next.fetch_add(1);
        if (index >= tree.paths->size())
            break;
        if (!tree.resources[worker.index].AddTree((*tree.paths)[index], 1)) {
            tree.failed = true;
            break;
        }
    }
    return 0;
}
bool ReadOnlyResources::HoldChildTrees(const std::vector<std::wstring>& paths) {
    ParallelTree tree;
    tree.paths = &paths;
    const unsigned count = static_cast<unsigned>((std::min)(paths.size(), size_t(ParallelTree::MaximumWorkers)));
    HANDLE threads[ParallelTree::MaximumWorkers] = {};
    unsigned started = 0;
    for (unsigned i = 0; i < count; ++i) {
        tree.workers[i].tree = &tree;
        tree.workers[i].index = i;
    }
    for (unsigned i = 0; i + 1 < count; ++i) {
        threads[i] = CreateThread(nullptr, 0, LockChildTrees, &tree.workers[i], 0, nullptr);
        if (!threads[i]) {
            tree.failed = true;
            break;
        }
        ++started;
    }
    // The caller participates; every helper must finish before its state or handles can move.
    LockChildTrees(&tree.workers[count - 1]);
    for (unsigned i = 0; i < started; ++i) {
        WaitForSingleObject(threads[i], INFINITE);
        CloseHandle(threads[i]);
    }
    if (tree.failed)
        return false; // Each group closes its partial locks on destruction.
    for (unsigned i = 0; i < count; ++i) {
        auto& handles = tree.resources[i].handles_;
        handles_.insert(handles_.end(), handles.begin(), handles.end());
        handles.clear();
    }
    return true;
}
ReadOnlyResources::~ReadOnlyResources() {
    Release();
}
void ReadOnlyResources::Release() {
    for (auto at = handles_.rbegin(); at != handles_.rend(); ++at)
        CloseHandle(*at);
    handles_.clear();
}
bool ReadOnlyResources::Hold(const std::wstring& path, bool directory) {
    HANDLE handle =
        CreateFileW(path.c_str(), directory ? FILE_READ_ATTRIBUTES : GENERIC_READ,
                    directory ? FILE_SHARE_READ | FILE_SHARE_WRITE : FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                    FILE_FLAG_OPEN_REPARSE_POINT | (directory ? FILE_FLAG_BACKUP_SEMANTICS : 0), nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        return false;
    BY_HANDLE_FILE_INFORMATION info = {};
    if (!GetFileInformationByHandle(handle, &info) ||
        ((info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) != directory ||
        (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
        CloseHandle(handle);
        return false;
    }
    handles_.push_back(handle);
    return true;
}
bool ReadOnlyResources::AddTree(const std::wstring& path, unsigned depth) {
    if (depth > 64)
        return false;
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
        return false;
    const bool directory = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    if (!Hold(path, directory))
        return false;
    if (!directory)
        return true;
    WIN32_FIND_DATAW item = {};
    HANDLE find = FindFirstFileW((path + L"\\*").c_str(), &item);
    if (find == INVALID_HANDLE_VALUE)
        return GetLastError() == ERROR_FILE_NOT_FOUND;
    bool ok = true;
    std::vector<std::wstring> children;
    do {
        if (!std::wcscmp(item.cFileName, L".") || !std::wcscmp(item.cFileName, L".."))
            continue;
        if (item.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
            ok = false;
            break;
        }
        children.push_back(path + L"\\" + item.cFileName);
    } while (FindNextFileW(find, &item));
    if (ok && GetLastError() != ERROR_NO_MORE_FILES)
        ok = false;
    FindClose(find);
    if (!ok)
        return false;
    // Native chunk archives contain tens of thousands of files in hundreds of directories.
    // Acquire every original lock, using independent worker groups only at the tree root.
    if (!depth && children.size() >= 64)
        return HoldChildTrees(children);
    for (const auto& child : children)
        if (!AddTree(child, depth + 1))
            return false;
    return true;
}
bool ReadOnlyResources::Acquire(const std::vector<std::wstring>& paths, std::string& error) {
    error = "Cannot protect original resources; close any program writing to them";
    if (!handles_.empty() || paths.empty())
        return false;
    for (const auto& path : paths) {
        if (path.size() < 3 || path[1] != L':' || (path[2] != L'\\' && path[2] != L'/') ||
            path.find(L'\0') != std::wstring::npos) {
            Release();
            return false;
        }
        wchar_t full[32768];
        const DWORD size = GetFullPathNameW(path.c_str(), 32768, full, nullptr);
        if (!size || size >= 32768) {
            Release();
            return false;
        }
        std::wstring normalized(full);
        while (normalized.size() > 3 && normalized.back() == L'\\')
            normalized.pop_back();
        if (!RuntimeRelativePath(normalized.substr(3)) || !Hold(normalized.substr(0, 3), true)) {
            Release();
            return false;
        }
        for (size_t i = 3; i < normalized.size(); ++i) {
            if (normalized[i] == L'\\' && !Hold(normalized.substr(0, i), true)) {
                Release();
                return false;
            }
        }
        if (!AddTree(normalized, 0)) {
            Release();
            return false;
        }
    }
    error.clear();
    return true;
}
} // namespace glide
