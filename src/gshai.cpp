#include "ast.hpp"
#include "lexer.hpp"
#include "parser.hpp"

#include <chrono>
#include <cstdlib>
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
constexpr char StringMarker = '\x01';
struct Diagnostic { fs::path file; std::size_t line; std::string message; };
struct BuildContext {
    std::unordered_set<std::string> loaded;
    std::unordered_set<std::string> active;
    std::unordered_set<std::string> variables;
    std::vector<Diagnostic> diagnostics;
    std::vector<shay::Statement> statements;
};
void diagnose(BuildContext &ctx, const fs::path &file, std::size_t line, const std::string &message) {
    ctx.diagnostics.push_back({file, line, message});
}
void load_file(const fs::path &input, BuildContext &ctx) {
    std::error_code ec;
    const fs::path canonical = fs::weakly_canonical(input, ec);
    const std::string key = (ec ? input : canonical).string();
    if (ctx.active.count(key)) {
        diagnose(ctx, input, 1, "circular import detected");
        return;
    }
    if (ctx.loaded.count(key)) return;
    const std::string extension = input.extension().string();
    if (extension != ".shay" && extension != ".shaz" && extension != ".shaii") {
        diagnose(ctx, input, 1, "unsupported file extension (expected .shay, .shaz, or .shaii)");
        return;
    }
    std::ifstream stream(input, std::ios::binary);
    if (!stream) {
        diagnose(ctx, input, 1, "cannot open source file");
        return;
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
        if (statement.kind == shay::StatementKind::ImportLibrary ||
            statement.kind == shay::StatementKind::ImportTask) {
            const bool library = statement.kind == shay::StatementKind::ImportLibrary;
            const std::string required = library ? ".shaii" : ".shaz";
            if (extension == ".shaii" && library) {
                diagnose(ctx, input, statement.line, ".shaii files cannot import other .shaii libraries");
                continue;
            }
            const fs::path target(statement.value);
            if (target.extension() != required) {
                diagnose(ctx, input, statement.line,
                         std::string("$") + (library ? "shaiiport" : "shazport") +
                         " requires a " + required + " file");
                continue;
            }
            load_file(input.parent_path() / target, ctx);
            continue;
        }
        if (statement.kind == shay::StatementKind::Declare) {
            if (extension == ".shaz") {
                diagnose(ctx, input, statement.line, "Zeccurs files are variable-free; 'brew' is not allowed");
                continue;
            }
            ctx.variables.insert(statement.name);
        } else if (statement.kind == shay::StatementKind::OutputValue &&
                   !ctx.variables.count(statement.name)) {
            diagnose(ctx, input, statement.line, "unknown variable '" + statement.name + "'");
        }
        ctx.statements.push_back(statement);
    }
    ctx.active.erase(key);
    ctx.loaded.insert(key);
}
std::string cpp_string(const std::string &value) {
    std::ostringstream out;
    out << '"';
    for (unsigned char c : value) {
        switch (c) {
        case '\\': out << "\\\\"; break;
        case '"': out << "\\\""; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (c < 32 || c >= 127) {
                const char octal[] = "01234567";
                out << "\\";
                out << octal[(c >> 6) & 7] << octal[(c >> 3) & 7] << octal[c & 7];
            } else out << static_cast<char>(c);
        }
    }
    out << '"';
    return out.str();
}
std::string shell_quote(const std::string &value) {
    std::string out = "'";
    for (char c : value) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    return out + "'";
}
bool write_translation(const BuildContext &ctx, const fs::path &path, bool add_main) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << "#include <cstdio>\n#include <string>\n#include <unordered_map>\n"
           "extern \"C\" void shay_runtime_write(const char* text) { if (text) { std::fputs(text, stdout); std::fflush(stdout); } }\n"
           "extern \"C\" int shay_program_entry() {\n"
           "  std::unordered_map<std::string, std::string> values;\n";
    for (const auto &statement : ctx.statements) {
        switch (statement.kind) {
        case shay::StatementKind::Declare: {
            std::string value = statement.value;
            if (!value.empty() && value.front() == StringMarker) value.erase(value.begin());
            out << "  values[" << cpp_string(statement.name) << "] = " << cpp_string(value) << ";\n";
            break;
        }
        case shay::StatementKind::OutputText:
            out << "  shay_runtime_write(" << cpp_string(statement.value + "\n") << ");\n";
            break;
        case shay::StatementKind::OutputValue:
            out << "  shay_runtime_write((values[" << cpp_string(statement.name)
                << "] + \"\n\").c_str());\n";
            break;
        default: break;
        }
    }
    out << "  return 0;\n}\n";
    if (add_main) out << "int main() { return shay_program_entry(); }\n";
    return static_cast<bool>(out);
}
void print_diagnostics(const BuildContext &ctx) {
    for (const auto &d : ctx.diagnostics)
        std::cerr << d.file.string() << ':' << d.line << ": error: " << d.message << '\n';
}
}
int main(int argc,char **argv) {
    if(argc==2 && std::string(argv[1])=="--version") { std::cout<<"gshai 0.3.0 (Shay compiler)\n"; return 0; }
    if(argc==2 && (std::string(argv[1])=="--help" || std::string(argv[1])=="-h")) {
        std::cout<<"Usage:\n  gshai -ob [output.sho] program.shay   Compile a native object\n"
                    "  gshai -o  [output]     program.shay   Compile a runnable executable\n"
                    "  gshai --version\n\n"
                    "-ob emits .sho object files; -o links a standalone executable.\n";
        return 0;
    }
    if(argc<3 || argc>4) { std::cerr<<"gshai: expected -ob or -o and a .shay input (try --help)\n"; return 2; }
    const std::string mode=argv[1];
    if(mode!="-ob" && mode!="-o") { std::cerr<<"gshai: unknown output mode: "<<mode<<'\n'; return 2; }
    const bool object_mode=mode=="-ob";
    fs::path output,input;
    if(argc==3) input=argv[2]; else { output=argv[2]; input=argv[3]; }
    if(input.extension()!=".shay") { std::cerr<<"gshai: input must be a .shay file\n"; return 2; }
    if(output.empty()) {
        output=input.parent_path()/input.stem();
        if(object_mode) output += ".sho";
    } else if(object_mode && output.extension()!=".sho") {
        std::cerr<<"gshai: -ob output must use the .sho extension\n"; return 2;
    }
    BuildContext context; load_file(input,context);
    if(!context.diagnostics.empty()) { print_diagnostics(context); return 1; }
    const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path temp=fs::temp_directory_path()/("shay-build-"+std::to_string(stamp)+".cpp");
    if(!write_translation(context,temp,!object_mode)) { std::cerr<<"gshai: cannot write temporary translation unit\n"; return 1; }
    const char *configured=std::getenv("CXX");
    const std::string cxx=configured && *configured?configured:"c++";
    const std::string command=shell_quote(cxx)+" -std=c++20 -O2 "+
        (object_mode?"-c ":"")+shell_quote(temp.string())+" -o "+shell_quote(output.string());
    const int status=std::system(command.c_str());
    std::error_code ignored; fs::remove(temp,ignored);
    if(status!=0) { fs::remove(output,ignored); std::cerr<<"gshai: native compiler/linker failed\n"; return 1; }
    std::cout<<"gshai: wrote "<<(object_mode?"object file ":"executable ")<<output.string()<<'\n';
    return 0;
}
