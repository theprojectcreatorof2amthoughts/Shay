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
#include <utility>
#include <vector>

namespace shay {
inline constexpr unsigned char shaye_magic[] = {'S','H','A','Y','E',0x00,0x01}; // Legacy .shaye package magic.
inline constexpr unsigned char shpak_magic[] = {'S','H','P','A','K',0x00,0x01};
inline constexpr unsigned char shaye_alias_opcode = 0x01;
inline constexpr unsigned char shaye_definition_opcode = 0x02;
inline constexpr unsigned char shaye_end_opcode = 0x00;
inline constexpr unsigned char shaye_source_opcode = 0x03;

struct ShayeRecords {
    std::string source;
    std::vector<std::pair<std::string, std::string>> aliases;
    std::vector<std::pair<std::string, std::vector<std::string>>> definitions;
};

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
        if (std::find(unique.begin(), unique.end(), path) == unique.end()) unique.push_back(path);
    return unique;
}
inline unsigned char shaye_mask(std::size_t index) {
    return static_cast<unsigned char>(0xA7u + static_cast<unsigned char>(index * 29u));
}
inline void shaye_write_u16(std::ostream& out, std::uint16_t value) {
    out.put(static_cast<char>((value >> 8) & 0xff));
    out.put(static_cast<char>(value & 0xff));
}
inline void shaye_write_u32(std::ostream& out, std::uint32_t value) {
    out.put(static_cast<char>((value >> 24) & 0xff));
    out.put(static_cast<char>((value >> 16) & 0xff));
    out.put(static_cast<char>((value >> 8) & 0xff));
    out.put(static_cast<char>(value & 0xff));
}
inline bool shaye_read_u32(const std::string& bytes, std::size_t& at, std::uint32_t& value) {
    if (at + 4 > bytes.size()) return false;
    value = (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[at])) << 24) |
            (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[at + 1])) << 16) |
            (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[at + 2])) << 8) |
            static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[at + 3]));
    at += 4;
    return true;
}
inline bool shaye_read_u16(const std::string& bytes, std::size_t& at, std::uint16_t& value) {
    if (at + 2 > bytes.size()) return false;
    value = static_cast<std::uint16_t>((static_cast<unsigned char>(bytes[at]) << 8) |
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
inline bool shaye_read_string(const std::string& bytes, std::size_t& at, std::string& value) {
    std::uint16_t size = 0;
    if (!shaye_read_u16(bytes, at, size) || size == 0 || at + size > bytes.size()) return false;
    value = shaye_transform(bytes.substr(at, size));
    at += size;
    return true;
}
inline void shaye_write_string(std::ostream& out, const std::string& value) {
    shaye_write_u16(out, static_cast<std::uint16_t>(value.size()));
    const auto encoded = shaye_transform(value);
    out.write(encoded.data(), static_cast<std::streamsize>(encoded.size()));
}
inline bool read_shaye_records(const std::filesystem::path& path, ShayeRecords& records) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    const std::string bytes((std::istreambuf_iterator<char>(input)), {});
    const bool is_shpak = bytes.size() >= sizeof(shpak_magic) &&
        std::equal(std::begin(shpak_magic), std::end(shpak_magic), bytes.begin(),
                   [](unsigned char a, char b) { return a == static_cast<unsigned char>(b); });
    const bool is_legacy_shaye = bytes.size() >= sizeof(shaye_magic) &&
        std::equal(std::begin(shaye_magic), std::end(shaye_magic), bytes.begin(),
                   [](unsigned char a, char b) { return a == static_cast<unsigned char>(b); });
    if (!is_shpak && !is_legacy_shaye) return false;
    std::size_t at = is_shpak ? sizeof(shpak_magic) : sizeof(shaye_magic);
    ShayeRecords parsed;
    bool ended = false;
    while (at < bytes.size()) {
        const auto opcode = static_cast<unsigned char>(bytes[at++]);
        if (opcode == shaye_end_opcode) { ended = at == bytes.size(); break; }
        if (opcode == shaye_alias_opcode) {
            std::string alias, target;
            if (!shaye_read_string(bytes, at, alias) || !shaye_read_string(bytes, at, target) ||
                !is_lowercase_useentry_name(alias) || !is_lowercase_useentry_name(target)) return false;
            parsed.aliases.emplace_back(std::move(alias), std::move(target));
        } else if (opcode == shaye_definition_opcode) {
            std::string name;
            std::uint16_t count = 0;
            if (!shaye_read_string(bytes, at, name) || !is_lowercase_useentry_name(name) ||
                !shaye_read_u16(bytes, at, count) || count == 0) return false;
            std::vector<std::string> patterns;
            patterns.reserve(count);
            for (std::uint16_t i = 0; i < count; ++i) {
                std::string pattern;
                if (!shaye_read_string(bytes, at, pattern)) return false;
                patterns.push_back(std::move(pattern));
            }
            parsed.definitions.emplace_back(std::move(name), std::move(patterns));
        } else if (opcode == shaye_source_opcode) {
            std::uint32_t size = 0;
            if (!shaye_read_u32(bytes, at, size) || at + size > bytes.size()) return false;
            parsed.source = shaye_transform(bytes.substr(at, size));
            at += size;
        } else return false;
    }
    if (!ended) return false;
    records = std::move(parsed);
    return true;
}
inline bool read_shaye_records(const std::filesystem::path& path,
                               std::vector<std::pair<std::string, std::string>>& aliases) {
    ShayeRecords records;
    if (!read_shaye_records(path, records)) return false;
    aliases = std::move(records.aliases);
    return true;
}
inline std::unordered_map<std::string, std::filesystem::path>
discover_shaye_entries(const std::filesystem::path& source) {
    std::unordered_map<std::string, std::filesystem::path> entries;
    for (const auto& directory : entryinit_directories(source)) {
        std::error_code ec;
        if (!std::filesystem::is_directory(directory, ec)) continue;
        for (std::filesystem::recursive_directory_iterator it(directory, ec), end; !ec && it != end; it.increment(ec)) {
            if (!it->is_regular_file(ec)) continue;
            const auto extension = it->path().extension();
            if (extension != ".shpak" && extension != ".shaye") continue;
            const std::string name = it->path().stem().string();
            if (!is_lowercase_useentry_name(name)) continue;
            auto existing = entries.find(name);
            if (existing != entries.end()) {
                if (existing->second.extension() == ".shpak" || extension != ".shpak") continue;
                existing->second = it->path();
            } else entries.emplace(name, it->path());
            register_useentry_name(name);
            ShayeRecords records;
            if (!read_shaye_records(it->path(), records)) continue;
            for (const auto& alias : records.aliases) register_entry_syntax(name, alias.first, alias.second);
            for (const auto& definition : records.definitions)
                register_entry_syntax_pattern(name, definition.first, definition.second);
        }
    }
    return entries;
}
inline bool read_shaye(const std::filesystem::path& path, std::string& source) {
    ShayeRecords records;
    if (!read_shaye_records(path, records)) return false;
    source = std::move(records.source);
    return true;
}
inline std::string shaye_trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \\t\\r");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \\t\\r");
    return value.substr(first, last - first + 1);
}
inline bool shaye_parse_definition_signature(const std::string& signature, std::string& name) {
    // Function-style entry syntax declarations use: $name()
    if (signature.size() >= 4 && signature.front() == '$' &&
        signature.compare(signature.size() - 2, 2, "()") == 0) {
        name = signature.substr(1, signature.size() - 3);
    } else {
        // Keep accepting the original bare-name form for compatibility.
        name = signature;
    }
    return is_lowercase_useentry_name(name);
}
inline std::string shaye_executable_source(const std::string& source) {
    std::istringstream lines(source);
    std::ostringstream executable;
    std::string line;
    bool in_definition = false;
    while (std::getline(lines, line)) {
        const std::string current = shaye_trim(line);
        if (in_definition) {
            if (current == "finish") in_definition = false;
            executable << '\\n';
            continue;
        }
        // Check the longer directive first: "$entrysyndef" also starts
        // with "$entrysyn".
        if (current.rfind("$entrysyndef", 0) == 0) {
            in_definition = true;
            executable << '\\n';
            continue;
        }
        if (current.rfind("$entrysyn", 0) == 0) {
            executable << '\\n';
            continue;
        }
        executable << line << '\\n';
    }
    return executable.str();
}
inline bool write_shaye(const std::filesystem::path& path, const std::string& source) {
    std::istringstream lines(source);
    std::string line;
    ShayeRecords records;
    bool in_definition = false;
    std::string definition_name;
    std::vector<std::string> definition_patterns;
    while (std::getline(lines, line)) {
        const std::string current = shaye_trim(line);
        if (current.empty() || current.rfind("#%", 0) == 0 || current.rfind("##", 0) == 0) continue;
        if (in_definition) {
            if (current == "finish") {
                if (definition_patterns.empty()) return false;
                records.definitions.emplace_back(definition_name, definition_patterns);
                in_definition = false;
                definition_name.clear();
                definition_patterns.clear();
            } else {
                if (current.size() > 65535) return false;
                definition_patterns.push_back(current);
                if (definition_patterns.size() > 65535) return false;
            }
            continue;
        }
        std::istringstream words(current);
        std::string directive, first, second, extra;
        if (!(words >> directive)) continue;
        if (directive == "$entrysyn") {
            if (!(words >> first >> second) || (words >> extra) ||
                !is_lowercase_useentry_name(first) || !is_lowercase_useentry_name(second)) return false;
            records.aliases.emplace_back(std::move(first), std::move(second));
        } else if (directive == "$entrysyndef") {
            if (!(words >> first) || (words >> extra) ||
                !shaye_parse_definition_signature(first, definition_name)) return false;
            in_definition = true;
        } else {
            // A .shpak is a complete Shay entry source. Ordinary Shay statements
            // are retained in the payload; only entry metadata is extracted here.
            continue;
        }
    }
    if (in_definition) return false;
    records.source = shaye_executable_source(source);
    if (records.source.size() > UINT32_MAX) return false;
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) return false;
    output.write(reinterpret_cast<const char*>(shpak_magic), sizeof(shpak_magic));
    for (const auto& alias : records.aliases) {
        output.put(static_cast<char>(shaye_alias_opcode));
        shaye_write_string(output, alias.first);
        shaye_write_string(output, alias.second);
    }
    for (const auto& definition : records.definitions) {
        output.put(static_cast<char>(shaye_definition_opcode));
        shaye_write_string(output, definition.first);
        shaye_write_u16(output, static_cast<std::uint16_t>(definition.second.size()));
        for (const auto& pattern : definition.second) shaye_write_string(output, pattern);
    }
    output.put(static_cast<char>(shaye_source_opcode));
    shaye_write_u32(output, static_cast<std::uint32_t>(records.source.size()));
    const auto encoded_source = shaye_transform(records.source);
    output.write(encoded_source.data(), static_cast<std::streamsize>(encoded_source.size()));
    output.put(static_cast<char>(shaye_end_opcode));
    return static_cast<bool>(output);
}
}
#endif
 &&
        signature.compare(signature.size() - 2, 2, "()") == 0) {
        name = signature.substr(1, signature.size() - 3);
    } else {
        // Keep accepting the original bare-name form for compatibility.
        name = signature;
    }
    return is_lowercase_useentry_name(name);
}
inline std::string shaye_trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r");
    return value.substr(first, last - first + 1);
}
inline std::string shaye_executable_source(const std::string& source) {
    std::istringstream lines(source);
    std::ostringstream executable;
    std::string line;
    bool in_definition = false;
    while (std::getline(lines, line)) {
        const std::string current = shaye_trim(line);
        if (in_definition) {
            if (current == "finish") in_definition = false;
            executable << '\n';
            continue;
        }
        if (current.rfind("$entrysyn", 0) == 0) {
            executable << '\n';
            continue;
        }
        if (current.rfind("$entrysyndef", 0) == 0) {
            in_definition = true;
            executable << '\n';
            continue;
        }
        executable << line << '\n';
    }
    return executable.str();
}
inline bool write_shaye(const std::filesystem::path& path, const std::string& source) {
    std::istringstream lines(source);
    std::string line;
    ShayeRecords records;
    bool in_definition = false;
    std::string definition_name;
    std::vector<std::string> definition_patterns;
    while (std::getline(lines, line)) {
        const std::string current = shaye_trim(line);
        if (current.empty() || current.rfind("#%", 0) == 0 || current.rfind("##", 0) == 0) continue;
        if (in_definition) {
            if (current == "finish") {
                if (definition_patterns.empty()) return false;
                records.definitions.emplace_back(definition_name, definition_patterns);
                in_definition = false;
                definition_name.clear();
                definition_patterns.clear();
            } else {
                if (current.size() > 65535) return false;
                definition_patterns.push_back(current);
                if (definition_patterns.size() > 65535) return false;
            }
            continue;
        }
        std::istringstream words(current);
        std::string directive, first, second, extra;
        if (!(words >> directive)) continue;
        if (directive == "$entrysyn") {
            if (!(words >> first >> second) || (words >> extra) ||
                !is_lowercase_useentry_name(first) || !is_lowercase_useentry_name(second)) return false;
            records.aliases.emplace_back(std::move(first), std::move(second));
        } else if (directive == "$entrysyndef") {
            if (!(words >> first) || (words >> extra) || !is_lowercase_useentry_name(first)) return false;
            in_definition = true;
            definition_name = std::move(first);
        } else {
            // A .shpak is a complete Shay entry source. Ordinary Shay statements
            // are retained in the payload; only entry metadata is extracted here.
            continue;
        }
    }
    if (in_definition) return false;
    records.source = shaye_executable_source(source);
    if (records.source.size() > UINT32_MAX) return false;
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) return false;
    output.write(reinterpret_cast<const char*>(shpak_magic), sizeof(shpak_magic));
    for (const auto& alias : records.aliases) {
        output.put(static_cast<char>(shaye_alias_opcode));
        shaye_write_string(output, alias.first);
        shaye_write_string(output, alias.second);
    }
    for (const auto& definition : records.definitions) {
        output.put(static_cast<char>(shaye_definition_opcode));
        shaye_write_string(output, definition.first);
        shaye_write_u16(output, static_cast<std::uint16_t>(definition.second.size()));
        for (const auto& pattern : definition.second) shaye_write_string(output, pattern);
    }
    output.put(static_cast<char>(shaye_source_opcode));
    shaye_write_u32(output, static_cast<std::uint32_t>(records.source.size()));
    const auto encoded_source = shaye_transform(records.source);
    output.write(encoded_source.data(), static_cast<std::streamsize>(encoded_source.size()));
    output.put(static_cast<char>(shaye_end_opcode));
    return static_cast<bool>(output);
}
}
#endif
