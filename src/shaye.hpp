#ifndef SHAY_SHAYE_HPP
#define SHAY_SHAYE_HPP

#include "useentry.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace shay {
inline constexpr unsigned char shaye_magic[] = {'S','H','A','Y','E',0x00,0x01};
inline constexpr unsigned char shaye_alias_opcode = 0x01;
inline constexpr unsigned char shaye_end_opcode = 0x00;

inline std::filesystem::path user_entry_directory() {
    const char* home = std::getenv("HOME");
#ifdef _WIN32
    if (!home || !*home) home = std::getenv("USERPROFILE");
#endif
    if (!home || !*home) return {};
    return std::filesystem::path(home) / ".shay" / "entries";
}
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
    const auto user_entries = user_entry_directory();
    if (!user_entries.empty()) roots.push_back(user_entries);
    std::vector<std::filesystem::path> unique;
    for (const auto& path : roots)
        if (std::find(unique.begin(), unique.end(), path) == unique.end())
            unique.push_back(path);
    return unique;
}
inline unsigned char shaye_mask(std::size_t index) {
    return static_cast<unsigned char>(0xA7u + static_cast<unsigned char>(index * 29u));
}
inline void shaye_write_u16(std::ostream& out, std::uint16_t value) {
    out.put(static_cast<char>((value >> 8) & 0xff));
    out.put(static_cast<char>(value & 0xff));
}
inline bool shaye_read_u16(const std::string& bytes, std::size_t& at, std::uint16_t& value) {
    if (at + 2 > bytes.size()) return false;
    value = static_cast<std::uint16_t>(
        (static_cast<unsigned char>(bytes[at]) << 8) |
        static_cast<unsigned char>(bytes[at + 1]));
    at += 2;
    return true;
}
inline std::string shaye_transform(const std::string& value) {
    std::string result(value);
    for (std::size_t i = 0; i < result.size(); ++i)
        result[i] = static_cast<char>(static_cast<unsigned char>(result[i]) ^ shaye_mask(i));
    return result;
}
inline bool read_shaye_records(const std::filesystem::path& path,
                               std::vector<std::pair<std::string, std::string>>& records) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    const std::string bytes((std::istreambuf_iterator<char>(input)), {});
    if (bytes.size() < sizeof(shaye_magic) ||
        !std::equal(std::begin(shaye_magic), std::end(shaye_magic), bytes.begin(),
                    [](unsigned char a, char b) { return a == static_cast<unsigned char>(b); }))
        return false;
    std::size_t at = sizeof(shaye_magic);
    std::vector<std::pair<std::string, std::string>> parsed;
    bool ended = false;
    while (at < bytes.size()) {
        const auto opcode = static_cast<unsigned char>(bytes[at++]);
        if (opcode == shaye_end_opcode) { ended = at == bytes.size(); break; }
        if (opcode != shaye_alias_opcode) return false;
        std::uint16_t alias_size = 0, target_size = 0;
        if (!shaye_read_u16(bytes, at, alias_size) || alias_size == 0 || at + alias_size > bytes.size()) return false;
        std::string alias = shaye_transform(bytes.substr(at, alias_size));
        at += alias_size;
        if (!shaye_read_u16(bytes, at, target_size) || target_size == 0 || at + target_size > bytes.size()) return false;
        std::string target = shaye_transform(bytes.substr(at, target_size));
        at += target_size;
        if (!is_lowercase_useentry_name(alias) || !is_lowercase_useentry_name(target)) return false;
        parsed.emplace_back(std::move(alias), std::move(target));
    }
    if (!ended) return false;
    records = std::move(parsed);
    return true;
}
inline std::unordered_map<std::string, std::filesystem::path>
discover_shaye_entries(const std::filesystem::path& source) {
    std::unordered_map<std::string, std::filesystem::path> entries;
    for (const auto& directory : entryinit_directories(source)) {
        std::error_code ec;
        if (!std::filesystem::is_directory(directory, ec)) continue;
        for (std::filesystem::recursive_directory_iterator it(directory, ec), end; !ec && it != end; it.increment(ec)) {
            if (!it->is_regular_file(ec) || it->path().extension() != ".shaye") continue;
            const std::string name = it->path().stem().string();
            if (!is_lowercase_useentry_name(name)) continue;
            if (!entries.emplace(name, it->path()).second) continue;
            register_useentry_name(name);
            std::vector<std::pair<std::string, std::string>> records;
            if (!read_shaye_records(it->path(), records)) continue;
            for (const auto& record : records)
                register_entry_syntax(name, record.first, record.second);
        }
    }
    return entries;
}
inline bool read_shaye(const std::filesystem::path& path, std::string& source) {
    std::vector<std::pair<std::string, std::string>> records;
    if (!read_shaye_records(path, records)) return false;
    source.clear();
    return true;
}
inline bool write_shaye(const std::filesystem::path& path, const std::string& source) {
    std::istringstream lines(source);
    std::string line;
    std::vector<std::pair<std::string, std::string>> records;
    while (std::getline(lines, line)) {
        const auto first = line.find_first_not_of(" \t\r");
        if (first == std::string::npos || line.compare(first, 2, "#%") == 0 || line.compare(first, 2, "##") == 0)
            continue;
        std::istringstream words(line.substr(first));
        std::string directive, alias, target, extra;
        if (!(words >> directive >> alias >> target) || (words >> extra) ||
            directive != "$entrysyn" || !is_lowercase_useentry_name(alias) ||
            !is_lowercase_useentry_name(target))
            return false;
        records.emplace_back(std::move(alias), std::move(target));
    }
    if (records.empty()) return false;
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) return false;
    output.write(reinterpret_cast<const char*>(shaye_magic), sizeof(shaye_magic));
    for (const auto& record : records) {
        if (record.first.size() > 65535 || record.second.size() > 65535) return false;
        output.put(static_cast<char>(shaye_alias_opcode));
        shaye_write_u16(output, static_cast<std::uint16_t>(record.first.size()));
        const auto alias = shaye_transform(record.first);
        output.write(alias.data(), static_cast<std::streamsize>(alias.size()));
        shaye_write_u16(output, static_cast<std::uint16_t>(record.second.size()));
        const auto target = shaye_transform(record.second);
        output.write(target.data(), static_cast<std::streamsize>(target.size()));
    }
    output.put(static_cast<char>(shaye_end_opcode));
    return static_cast<bool>(output);
}
}
#endif
