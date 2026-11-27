#include "shaye.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

namespace {
struct Diagnostic {
    fs::path file;
    std::size_t line = 1;
    std::size_t column = 1;
    std::string message;
    std::string source;
};

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r");
    return value.substr(first, last - first + 1);
}

bool validate_source(const fs::path& file, const std::string& source,
                     std::vector<Diagnostic>& diagnostics,
                     std::unordered_set<std::string>& aliases,
                     std::unordered_set<std::string>& definitions) {
    std::istringstream input(source);
    std::string line;
    std::size_t line_number = 0;
    bool in_definition = false;
    std::string definition_name;
    std::size_t definition_line = 0;
    std::size_t pattern_count = 0;
    bool found_declaration = false;
    bool valid = true;

    auto report = [&](std::size_t number, const std::string& raw,
                      const std::string& message, const std::string& token = {}) {
        Diagnostic diagnostic;
        diagnostic.file = file;
        diagnostic.line = number;
        diagnostic.message = message;
        diagnostic.source = raw;
        const auto at = token.empty() ? raw.find_first_not_of(" \t\r") : raw.find(token);
        diagnostic.column = at == std::string::npos ? 1 : at + 1;
        diagnostics.push_back(std::move(diagnostic));
        valid = false;
    };

    while (std::getline(input, line)) {
        ++line_number;
        const std::string current = trim(line);
        if (current.empty() || current.rfind("#%", 0) == 0 || current.rfind("##", 0) == 0) continue;

        if (in_definition) {
            if (current == "finish") {
                if (pattern_count == 0)
                    report(line_number, line, "syntax definition '" + definition_name + "' has no pattern lines", "finish");
                in_definition = false;
                definition_name.clear();
                pattern_count = 0;
            } else {
                if (current.rfind("$entrysyndef", 0) == 0 || current.rfind("$entrysyn", 0) == 0)
                    report(line_number, line, "entry declarations cannot be nested inside a $entrysyndef block", current);
                if (current.size() > 65535)
                    report(line_number, line, "syntax pattern exceeds the 65535-byte package limit", current);
                ++pattern_count;
                if (pattern_count > 65535)
                    report(line_number, line, "syntax definition exceeds the 65535-pattern package limit", current);
            }
            continue;
        }

        std::istringstream words(current);
        std::string directive, first, second, extra;
        if (!(words >> directive)) continue;
        if (directive == "$entrysyn") {
            if (!(words >> first)) {
                report(line_number, line, "expected an alias name after '$entrysyn'", "$entrysyn");
                continue;
            }
            if (!(words >> second)) {
                report(line_number, line, "expected an existing Shay syntax target after alias '" + first + "'", first);
                continue;
            }
            if (words >> extra) {
                report(line_number, line, "unexpected extra token '" + extra + "' after '$entrysyn'", extra);
                continue;
            }
            if (!shay::is_lowercase_useentry_name(first))
                report(line_number, line, "alias name '" + first + "' must contain lowercase English letters only", first);
            if (!shay::is_lowercase_useentry_name(second))
                report(line_number, line, "syntax target '" + second + "' must contain lowercase English letters only", second);
            if (!aliases.insert(first).second)
                report(line_number, line, "duplicate entry syntax alias '" + first + "'", first);
            found_declaration = true;
        } else if (directive == "$entrysyndef") {
            if (!(words >> first)) {
                report(line_number, line, "expected a syntax definition name after '$entrysyndef'", "$entrysyndef");
                continue;
            }
            if (words >> extra) {
                report(line_number, line, "unexpected extra token '" + extra + "' after '$entrysyndef'", extra);
                continue;
            }
            if (!shay::is_lowercase_useentry_name(first))
                report(line_number, line, "syntax definition name '" + first + "' must contain lowercase English letters only", first);
            if (!definitions.insert(first).second)
                report(line_number, line, "duplicate syntax definition '" + first + "'", first);
            in_definition = true;
            definition_name = first;
            definition_line = line_number;
            pattern_count = 0;
            found_declaration = true;
        } else {
            report(line_number, line, "unexpected content in an entry source; expected '$entrysyn' or '$entrysyndef'", directive);
        }
    }

    if (in_definition)
        report(definition_line, "$entrysyndef " + definition_name,
               "syntax definition '" + definition_name + "' is missing its closing 'finish'", "$entrysyndef");
    if (!found_declaration)
        report(1, source.substr(0, source.find('\n')),
               "entry source contains no $entrysyn or $entrysyndef declarations");
    return valid;
}

void print_diagnostics(const std::vector<Diagnostic>& diagnostics) {
    for (const auto& diagnostic : diagnostics) {
        std::cerr << diagnostic.file.string() << ':' << diagnostic.line << ':' << diagnostic.column
                  << ": error: " << diagnostic.message << '\n';
        if (!diagnostic.source.empty()) {
            std::cerr << "  " << diagnostic.source << '\n' << "  ";
            for (std::size_t i = 1; i < diagnostic.column; ++i) std::cerr << ' ';
            std::cerr << "^\n";
        }
    }
}

void usage() {
    std::cout
        << "Usage: shentrypak source.shay [-o output.shpak]\n"
        << "       shentrypak -o output.shpak source.shay [more-source.shay ...]\n"
        << "       shentrypak --version\n\n"
        << "Compiles $entrysyn and $entrysyndef declarations into a binary .shpak entry package.\n";
}
}

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--version") {
        std::cout << "shentrypak 0.1.0 (Shay entry package compiler)\n";
        return 0;
    }
    if (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
        usage();
        return 0;
    }

    fs::path output;
    std::vector<fs::path> sources;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-o") {
            if (++i >= argc || !output.empty()) {
                std::cerr << "shentrypak: -o requires exactly one output path\n";
                return 2;
            }
            output = argv[i];
        } else if (!arg.empty() && arg[0] == '-') {
            std::cerr << "shentrypak: unknown option '" << arg << "'\n";
            return 2;
        } else {
            sources.emplace_back(arg);
        }
    }
    if (sources.empty() || output.empty()) {
        usage();
        std::cerr << "shentrypak: provide at least one source file and an output path\n";
        return 2;
    }
    if (output.extension() != ".shpak") {
        std::cerr << "shentrypak: output file must use the .shpak extension\n";
        return 2;
    }
    if (!shay::is_lowercase_useentry_name(output.stem().string())) {
        std::cerr << "shentrypak: output filename stem must contain lowercase English letters only\n";
        return 2;
    }

    std::string combined_source;
    std::vector<Diagnostic> diagnostics;
    std::unordered_set<std::string> aliases;
    std::unordered_set<std::string> definitions;
    for (const auto& source_path : sources) {
        if (source_path.extension() != ".shay") {
            std::cerr << "shentrypak: input '" << source_path.string() << "' must use the .shay extension\n";
            return 2;
        }
        std::ifstream input(source_path, std::ios::binary);
        if (!input) {
            std::cerr << "shentrypak: cannot open source file '" << source_path.string() << "'\n";
            return 1;
        }
        const std::string source((std::istreambuf_iterator<char>(input)), {});
        validate_source(source_path, source, diagnostics, aliases, definitions);
        combined_source += source;
        if (combined_source.empty() || combined_source.back() != '\n') combined_source.push_back('\n');
    }
    if (!diagnostics.empty()) {
        print_diagnostics(diagnostics);
        std::cerr << "shentrypak: package was not written because the entry source contains errors\n";
        return 1;
    }

    std::error_code ec;
    if (!output.parent_path().empty()) fs::create_directories(output.parent_path(), ec);
    if (ec) {
        std::cerr << "shentrypak: cannot create output directory '" << output.parent_path().string()
                  << "': " << ec.message() << '\n';
        return 1;
    }
    if (!shay::write_shaye(output, combined_source)) {
        std::cerr << "shentrypak: cannot write package '" << output.string() << "'\n";
        return 1;
    }
    std::cout << "shentrypak: created " << output.string() << " from "
              << sources.size() << " source file(s)\n";
    return 0;
}
