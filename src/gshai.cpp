#include "runtime.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

struct Diagnostic {
    fs::path file;
    std::size_t line;
    std::string message;
};

struct Context {
    std::unordered_map<std::string, std::string> values;
    std::unordered_set<std::string> loaded;
    std::unordered_set<std::string> active;
    std::vector<Diagnostic> diagnostics;
    bool check_only = false;
};

static std::string trim(const std::string &s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

static std::string unquote(const std::string &s) {
    std::string out;
    for (std::size_t i = 1; i + 1 < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size() - 1) {
            const char n = s[++i];
            switch (n) {
                case 'n': out.push_back('\n'); break;
                case 't': out.push_back('\t'); break;
                case 'r': out.push_back('\r'); break;
                case '\\': out.push_back('\\'); break;
                case '"': out.push_back('"'); break;
                default: out.push_back(n); break;
            }
        } else out.push_back(s[i]);
    }
    return out;
}

static void diagnose(Context &ctx, const fs::path &file, std::size_t line,
                    const std::string &message) {
    ctx.diagnostics.push_back({file, line, message});
}

static bool execute_file(const fs::path &input, Context &ctx, bool imported = false) {
    std::error_code ec;
    const fs::path file = fs::weakly_canonical(input, ec);
    const std::string key = (ec ? input : file).string();
    if (ctx.active.count(key)) {
        diagnose(ctx, input, 1, "circular import detected");
        return false;
    }
    if (ctx.loaded.count(key)) return true;

    std::ifstream stream(input);
    if (!stream) {
        diagnose(ctx, input, 1, "cannot open source file");
        return false;
    }

    std::string ext = input.extension().string();
    if (ext != ".shay" && ext != ".shaz" && ext != ".shaii") {
        diagnose(ctx, input, 1, "unsupported file extension (expected .shay, .shaz, or .shaii)");
        return false;
    }

    ctx.active.insert(key);
    std::string raw;
    std::size_t line_no = 0;
    const std::regex import_re(R"(^\$(shaiiport|shazport)\s+"([^"]+)"\s*$)");
    const std::regex brew_re(R"(^brew\s+([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.+)$)");
    const std::regex text_re(R"(^instxt-"((?:\\.|[^"])*)"\s*$)");
    const std::regex value_re(R"(^insval-([A-Za-z_][A-Za-z0-9_]*)\s*$)");
    const std::regex number_re(R"(^-?[0-9]+(?:\.[0-9]+)?$)");

    while (std::getline(stream, raw)) {
        ++line_no;
        std::string line = trim(raw);
        if (line.empty() || line.rfind("##", 0) == 0) continue;

        std::smatch match;
        if (std::regex_match(line, match, import_re)) {
            const bool library_import = match[1] == "shaiiport";
            const std::string target_ext = library_import ? ".shaii" : ".shaz";
            if (ext == ".shaii" && library_import) {
                diagnose(ctx, input, line_no, ".shaii files cannot import other .shaii libraries");
                continue;
            }
            if (!library_import && ext == ".shaii") {
                // Libraries may import Zeccurs tasks.
            }
            fs::path target = input.parent_path() / match[2].str();
            if (target.extension() != target_ext) {
                diagnose(ctx, input, line_no, std::string("$") +
                         (library_import ? "shaiiport" : "shazport") +
                         " requires a " + target_ext + " file");
                continue;
            }
            execute_file(target, ctx, true);
            continue;
        }

        if (line.rfind("$shaiiport", 0) == 0 || line.rfind("$shazport", 0) == 0) {
            diagnose(ctx, input, line_no, "invalid import syntax; expected $shaiiport \"file.shaii\" or $shazport \"file.shaz\"");
            continue;
        }

        if (std::regex_match(line, match, brew_re)) {
            if (ext == ".shaz") {
                diagnose(ctx, input, line_no, "Zeccurs files are variable-free; 'brew' is not allowed");
                continue;
            }
            const std::string name = match[1];
            const std::string value = trim(match[2]);
            if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
                ctx.values[name] = unquote(value);
            } else if (value == "true" || value == "false" || std::regex_match(value, number_re)) {
                ctx.values[name] = value;
            } else {
                diagnose(ctx, input, line_no, "unsupported initializer; use a string, number, or boolean literal");
            }
            continue;
        }

        if (std::regex_match(line, match, text_re)) {
            if (!ctx.check_only) {
                const std::string output = unquote("\"" + match[1].str() + "\"") + "\n";
                shay_runtime_write(output.c_str());
            }
            continue;
        }

        if (std::regex_match(line, match, value_re)) {
            const auto found = ctx.values.find(match[1]);
            if (found == ctx.values.end()) {
                diagnose(ctx, input, line_no, "unknown variable '" + match[1].str() + "'");
            } else if (!ctx.check_only) {
                const std::string output = found->second + "\n";
                shay_runtime_write(output.c_str());
            }
            continue;
        }

        diagnose(ctx, input, line_no, "unrecognized Shay statement");
    }

    ctx.active.erase(key);
    ctx.loaded.insert(key);
    (void)imported;
    return true;
}

static void print_diagnostics(const Context &ctx) {
    for (const auto &d : ctx.diagnostics) {
        std::cerr << d.file.string() << ':' << d.line << ": error: " << d.message << '\n';
    }
}

int main(int argc, char **argv) {
    if (argc == 2 && std::string(argv[1]) == "--version") {
        std::cout << "gshai 0.1.0 (Shay hybrid compiler/runtime)\n";
        return 0;
    }
    bool check = false;
    fs::path source;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--check") check = true;
        else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: gshai [--check] program.shay|program.shaz|library.shaii\n"
                         "       gshai --version\n";
            return 0;
        } else if (!arg.empty() && arg[0] == '-') {
            std::cerr << "gshai: unknown option: " << arg << '\n';
            return 2;
        } else if (source.empty()) source = arg;
        else {
            std::cerr << "gshai: only one entry file is accepted\n";
            return 2;
        }
    }
    if (source.empty()) {
        std::cerr << "gshai: no input file (try --help)\n";
        return 2;
    }

    Context ctx;
    ctx.check_only = check;
    execute_file(source, ctx);
    if (!ctx.diagnostics.empty()) {
        print_diagnostics(ctx);
        return 1;
    }
    if (check) std::cout << "gshai: " << source.string() << ": no errors\n";
    return 0;
}
