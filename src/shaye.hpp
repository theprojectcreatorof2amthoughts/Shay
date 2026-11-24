#ifndef SHAY_SHAYE_HPP
#define SHAY_SHAYE_HPP

#include "useentry.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <unordered_map>
#include <vector>

namespace shay {
namespace fs = std::filesystem;
inline constexpr const char* shaye_magic = "SHAYE/1\n";

inline std::vector<fs::path> entryinit_directories(const fs::path& source) {
    std::vector<fs::path> roots;
    std::error_code ec;
    auto add_ancestors = [&](fs::path start) {
        if (start.empty()) start = fs::current_path(ec);
        start = fs::absolute(start, ec);
        if (ec) { ec.clear(); return; }
        if (fs::is_regular_file(start, ec)) start = start.parent_path();
        while (!start.empty()) {
            roots.push_back(start / "srchfile" / "siil" / "entryinit");
            const auto parent = start.parent_path();
            if (parent == start) break;
            start = parent;
        }
    };
    add_ancestors(fs::current_path(ec));
    ec.clear();
    add_ancestors(source);
    std::vector<fs::path> unique;
    for (const auto& path : roots)
        if (std::find(unique.begin(), unique.end(), path) == unique.end())
            unique.push_back(path);
    return unique;
}

inline std::unordered_map<std::string, fs::path>
discover_shaye_entries(const fs::path& source) {
    std::unordered_map<std::string, fs::path> entries;
    for (const auto& directory : entryinit_directories(source)) {
        std::error_code ec;
        if (!fs::is_directory(directory, ec)) continue;
        for (fs::directory_iterator it(directory, ec), end; !ec && it != end; it.increment(ec)) {
            if (!it->is_regular_file(ec) || it->path().extension() != ".shaye") continue;
            const std::string name = it->path().stem().string();
            if (!is_lowercase_useentry_name(name)) continue;
            entries.emplace(name, it->path());
            register_useentry_name(name);
        }
    }
    return entries;
}

inline bool read_shaye(const fs::path& path, std::string& source) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    std::string bytes((std::istreambuf_iterator<char>(input)), {});
    const std::string magic(shaye_magic);
    if (bytes.compare(0, magic.size(), magic) != 0) return false;
    source.assign(bytes.data() + magic.size(), bytes.size() - magic.size());
    return true;
}

inline bool write_shaye(const fs::path& path, const std::string& source) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) return false;
    output << shaye_magic;
    output.write(source.data(), static_cast<std::streamsize>(source.size()));
    return static_cast<bool>(output);
}
}
#endif
