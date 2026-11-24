#ifndef SHAY_IMPORTS_HPP
#define SHAY_IMPORTS_HPP

#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace shay {
namespace fs = std::filesystem;

// Resolve an import beside its importer first, then through a project's siil/
// standard-library directory. Zeccur task imports remain importer-relative.
inline fs::path resolve_import(const fs::path &importer, const std::string &filename,
                               bool library) {
    const fs::path requested(filename);
    const fs::path local = importer.parent_path() / requested;
    std::error_code ec;
    if (fs::is_regular_file(local, ec)) return local;

    if (library) {
        if (const char *configured = std::getenv("SHAY_STDLIB"); configured && *configured) {
            ec.clear();
            const fs::path candidate = fs::path(configured) / requested;
            if (fs::is_regular_file(candidate, ec)) return candidate;
        }
        std::vector<fs::path> roots;
        const fs::path cwd = fs::current_path(ec);
        if (!ec) roots.push_back(cwd);
        fs::path parent = importer.parent_path();
        if (parent.empty()) parent = cwd;
        parent = fs::absolute(parent, ec);
        if (ec) parent = importer.parent_path();
        while (!parent.empty()) {
            roots.push_back(parent);
            const fs::path next = parent.parent_path();
            if (next == parent) break;
            parent = next;
        }
        for (const auto &root : roots) {
            ec.clear();
            const fs::path candidate = root / "srchfile" / "siil" / requested;
            if (fs::is_regular_file(candidate, ec)) return candidate;
        }
    }
    return local;
}
} // namespace shay
#endif
