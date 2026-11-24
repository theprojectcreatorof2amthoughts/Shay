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
inline constexpr const char* shaye_magic = "SHAYE/1\n";

inline std::vector<std::filesystem::path> entryinit_directories(const std::filesystem::path& source) {
    std::vector<std::filesystem::path> roots;
    std::error_code ec;
    auto add_ancestors = [&](std::filesystem::path start) {
        if (start.empty()) start = std::filesystem::current_path(ec);
        start = std::filesystem::absolute(start, ec);
        if (ec) { ec.clear(); return; }
        if (std::filesystem::is_regular_file(start, ec)) start = start.parent_path();
        while (!start.empty()) {
            roots.push_back(start / "srchfile" / "siil" / "entryinit");
            const auto parent = start.parent_path();
            if (parent == start) break;
            start = parent;
        }
    };
    add_ancestors(std::filesystem::current_path(ec));
    ec.clear();
    add_ancestors(source);
    std::vector<std::filesystem::path> unique;
    for (const auto& path : roots)
        if (std::find(unique.begin(), unique.end(), path) == unique.end())
            unique.push_back(path);
    return unique;
}

inline std::unordered_map<std::string, std::filesystem::path>
discover_shaye_entries(const std::filesystem::path& source) {
    std::unordered_map<std::string, std::filesystem::path> entries;
    for (const auto& directory : entryinit_directories(source)) {
        std::error_code ec;
        if (!std::filesystem::is_directory(directory, ec)) continue;
        for (std::filesystem::directory_iterator it(directory, ec), end; !ec && it != end; it.increment(ec)) {
            if (!it->is_regular_file(ec) || it->path().extension() != ".shaye") continue;
            const std::string name = it->path().stem().string();
            if (!is_lowercase_useentry_name(name)) continue;
            entries.emplace(name, it->path());
            register_useentry_name(name);
        }
    }
    return entries;
}

inline bool read_shaye(const std::filesystem::path& path, std::string& source) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    std::string bytes((std::istreambuf_iterator<char>(input)), {});
    const std::string magic(shaye_magic);
    if (bytes.compare(0, magic.size(), magic) != 0) return false;
    source.assign(bytes.data() + magic.size(), bytes.size() - magic.size());
    return true;
}

inline bool write_shaye(const std::filesystem::path& path, const std::string& source) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) return false;
    output << shaye_magic;
    output.write(source.data(), static_cast<std::streamsize>(source.size()));
    return static_cast<bool>(output);
}
}
#endif
