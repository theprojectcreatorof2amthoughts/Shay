#include "ast.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "runtime.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;
namespace {
constexpr char StringMarker = '\x01';
struct Diagnostic { fs::path file; std::size_t line; std::string message; };
struct Context {
    std::unordered_map<std::string, std::string> values;
    std::unordered_set<std::string> loaded;
    std::unordered_set<std::string> active;
    std::vector<Diagnostic> diagnostics;
    bool check_only = false;
};
void diagnose(Context &ctx, const fs::path &file, std::size_t line, const std::string &message) {
    ctx.diagnostics.push_back({file, line, message});
}
bool execute_file(const fs::path &input, Context &ctx) {
    std::error_code ec;
    const fs::path canonical = fs::weakly_canonical(input, ec);
    const std::string key = (ec ? input : canonical).string();
    if (ctx.active.count(key)) {
        diagnose(ctx, input, 1, "circular import detected");
        return false;
    }
    if (ctx.loaded.count(key)) return true;

    const std::string extension = input.extension().string();
    if (extension != ".shay" && extension != ".shaz" && extension != ".shaii") {
        diagnose(ctx, input, 1, "unsupported file extension (expected .shay, .shaz, or .shaii)");
        return false;
    }
    std::ifstream stream(input, std::ios::binary);
    if (!stream) {
        diagnose(ctx, input, 1, "cannot open source file");
        return false;
    }
    const std::string source((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    shay::Lexer lexer(source);
    const auto tokens = lexer.scan();
    shay::Parser parser(tokens);
    const shay::Program program = parser.parse();
    for (const auto &e : lexer.errors()) diagnose(ctx, input, e.line, e.message);
    for (const auto &e : program.errors) diagnose(ctx, input, e.line, e.message);

    ctx.active.insert(key);
    for (const auto &statement : program.statements) {
        switch (statement.kind) {
        case shay::StatementKind::ImportLibrary:
        case shay::StatementKind::ImportTask: {
            const bool library = statement.kind == shay::StatementKind::ImportLibrary;
            const std::string required = library ? ".shaii" : ".shaz";
            if (extension == ".shaii" && library) {
                diagnose(ctx, input, statement.line, ".shaii files cannot import other .shaii libraries");
                break;
            }
            const fs::path targetName(statement.value);
            if (targetName.extension() != required) {
                diagnose(ctx, input, statement.line,
                         std::string("$") + (library ? "shaiiport" : "shazport") +
                         " requires a " + required + " file");
                break;
            }
            execute_file(input.parent_path() / targetName, ctx);
            break;
        }
        case shay::StatementKind::Declare:
            if (extension == ".shaz") {
                diagnose(ctx, input, statement.line, "Zeccurs files are variable-free; 'brew' is not allowed");
                break;
            }
            if (!statement.value.empty() && statement.value.front() == StringMarker)
                ctx.values[statement.name] = statement.value.substr(1);
            else
                ctx.values[statement.name] = statement.value;
            break;
        case shay::StatementKind::OutputText:
            if (!ctx.check_only) shay_runtime_write((statement.value + "\n").c_str());
            break;
        case shay::StatementKind::OutputValue: {
            const auto found = ctx.values.find(statement.name);
            if (found == ctx.values.end())
                diagnose(ctx, input, statement.line, "unknown variable '" + statement.name + "'");
            else if (!ctx.check_only)
                shay_runtime_write((found->second + "\n").c_str());
            break;
        }
        case shay::StatementKind::Invalid:
            break;
        }
    }
    ctx.active.erase(key);
    ctx.loaded.insert(key);
    return true;
}
void print_diagnostics(const Context &ctx) {
    for (const auto &d : ctx.diagnostics)
        std::cerr << d.file.string() << ':' << d.line << ": error: " << d.message << '\n';
}
}

int main(int argc, char **argv) {
    if (argc == 2 && std::string(argv[1]) == "--version") {
        std::cout << "gshai 0.2.0 (Shay lexer/parser runtime)\n";
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
    Context context;
    context.check_only = check;
    execute_file(source, context);
    if (!context.diagnostics.empty()) {
        print_diagnostics(context);
        return 1;
    }
    if (check) std::cout << "gshai: " << source.string() << ": no errors\n";
    return 0;
}
