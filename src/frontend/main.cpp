#include "shay_runtime.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct ShayError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r");
    return value.substr(first, last - first + 1);
}

std::string unquote(const std::string& value, const fs::path& file, std::size_t line) {
    if (value.size() < 2 || value.front() != '"' || value.back() != '"')
        throw ShayError(file.string() + ":" + std::to_string(line) + ": expected a quoted string");
    std::string out;
    for (std::size_t i = 1; i + 1 < value.size(); ++i) {
        if (value[i] == '\\' && i + 2 < value.size()) {
            const char next = value[++i];
            switch (next) {
                case 'n': out.push_back('\n'); break;
                case 't': out.push_back('\t'); break;
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                default: out.push_back(next); break;
            }
        } else {
            out.push_back(value[i]);
        }
    }
    return out;
}

class Interpreter {
public:
    void run(const fs::path& entry) {
        execute_file(fs::absolute(entry).lexically_normal());
    }

private:
    std::unordered_set<std::string> loaded_;
    std::vector<std::string> import_stack_;
    std::unordered_map<std::string, std::string> variables_;

    static std::string extension_kind(const fs::path& path) {
        const auto ext = path.extension().string();
        if (ext == ".shay") return "shay";
        if (ext == ".shaz") return "shaz";
        if (ext == ".shaii") return "shaii";
        throw ShayError("unsupported Shay file extension '" + ext + "'");
    }

    void import_file(const fs::path& importer, const std::string& directive,
                     const std::string& raw_path, std::size_t line) {
        const auto importer_kind = extension_kind(importer);
        const auto target = (importer.parent_path() / unquote(raw_path, importer, line)).lexically_normal();
        const auto target_kind = extension_kind(target);

        if (directive == "$shaiiport" && target_kind != "shaii")
            throw ShayError(importer.string() + ":" + std::to_string(line) + ": $shaiiport requires a .shaii file");
        if (directive == "$shazport" && target_kind != "shaz")
            throw ShayError(importer.string() + ":" + std::to_string(line) + ": $shazport requires a .shaz file");
        if (importer_kind == "shaii" && directive == "$shaiiport")
            throw ShayError(importer.string() + ":" + std::to_string(line) + ": .shaii files cannot import other .shaii files");

        execute_file(target);
    }

    void execute_file(const fs::path& file) {
        const auto key = file.string();
        if (loaded_.contains(key)) return;
        if (!fs::exists(file))
            throw ShayError("cannot open imported file '" + key + "'");
        if (std::find(import_stack_.begin(), import_stack_.end(), key) != import_stack_.end())
            throw ShayError("circular import detected at '" + key + "'");

        const auto kind = extension_kind(file);
        std::ifstream input(file);
        if (!input) throw ShayError("cannot read '" + key + "'");

        import_stack_.push_back(key);
        std::string source;
        std::size_t line_number = 0;
        while (std::getline(input, source)) {
            ++line_number;
            const auto line = trim(source);
            if (line.empty() || line.rfind("#", 0) == 0) continue;

            static const std::regex import_re(R"(^\$(shaiiport|shazport)\s+(".*")\s*$)");
            std::smatch match;
            if (std::regex_match(line, match, import_re)) {
                import_file(file, "$" + match[1].str(), match[2].str(), line_number);
                continue;
            }

            if (kind == "shaz" && line.rfind("brew", 0) == 0)
                throw ShayError(key + ":" + std::to_string(line_number) + ": Zeccurs (.shaz) files cannot declare variables");

            if (line.rfind("instxt-", 0) == 0) {
                shay_print_text(unquote(trim(line.substr(7)), file, line_number).c_str());
                continue;
            }

            if (line.rfind("insval-", 0) == 0) {
                const auto name = trim(line.substr(7));
                const auto found = variables_.find(name);
                if (found == variables_.end())
                    throw ShayError(key + ":" + std::to_string(line_number) + ": unknown value '" + name + "'");
                if (kind == "shaz")
                    throw ShayError(key + ":" + std::to_string(line_number) + ": Zeccurs cannot read Shay variables");
                shay_print_value(found->second.c_str());
                continue;
            }

            static const std::regex brew_re(R"(^brew\s+([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.+)$)");
            if (std::regex_match(line, match, brew_re)) {
                if (kind == "shaz")
                    throw ShayError(key + ":" + std::to_string(line_number) + ": Zeccurs (.shaz) files cannot declare variables");
                const auto name = match[1].str();
                const auto value = trim(match[2].str());
                if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
                    variables_[name] = unquote(value, file, line_number);
                else
                    variables_[name] = value;
                continue;
            }

            throw ShayError(key + ":" + std::to_string(line_number) + ": unrecognized Shay statement: " + line);
        }
        import_stack_.pop_back();
        loaded_.insert(key);
    }
};

void usage() {
    std::cout << "Shay 0.1.0 - hybrid C/C++ language toolchain\n"
                 "Usage: shay <program.shay>\n"
                 "       shay <task.shaz>\n"
                 "       shay --help\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        usage();
        return 0;
    }
    if (argc != 2) {
        usage();
        return 2;
    }
    try {
        Interpreter interpreter;
        interpreter.run(argv[1]);
        return 0;
    } catch (const ShayError& error) {
        std::cerr << "shay: error: " << error.what() << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "shay: fatal: " << error.what() << '\n';
        return 1;
    }
}
