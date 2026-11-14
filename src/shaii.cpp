#include "ast.hpp"
#include "imports.hpp"
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
    std::unordered_set<std::string> loaded, active;
    std::vector<Diagnostic> diagnostics;
    bool check_only = false;
};
void diagnose(Context &ctx, const fs::path &file, std::size_t line, const std::string &message) {
    ctx.diagnostics.push_back({file, line, message});
}
void execute_file(const fs::path &input, Context &ctx) {
    std::error_code ec;
    const fs::path canonical = fs::weakly_canonical(input, ec);
    const std::string key = (ec ? input : canonical).string();
    if (ctx.active.count(key)) { diagnose(ctx,input,1,"circular import detected"); return; }
    if (ctx.loaded.count(key)) return;
    const std::string extension=input.extension().string();
    if(extension!=".shay" && extension!=".shaz" && extension!=".shaii") {
        diagnose(ctx,input,1,"unsupported file extension (expected .shay, .shaz, or .shaii)"); return;
    }
    std::ifstream stream(input,std::ios::binary);
    if(!stream) { diagnose(ctx,input,1,"cannot open source file"); return; }
    const std::string source((std::istreambuf_iterator<char>(stream)),{});
    shay::Lexer lexer(source); const auto tokens=lexer.scan();
    shay::Parser parser(tokens); const shay::Program program=parser.parse();
    for(const auto &e:lexer.errors()) diagnose(ctx,input,e.line,e.message);
    for(const auto &e:program.errors) diagnose(ctx,input,e.line,e.message);
    ctx.active.insert(key);
    for(const auto &statement:program.statements) {
        switch(statement.kind) {
        case shay::StatementKind::ImportLibrary:
        case shay::StatementKind::ImportTask: {
            const bool library=statement.kind==shay::StatementKind::ImportLibrary;
            const std::string required=library?".shaii":".shaz";
            if(extension==".shaii" && library) {
                diagnose(ctx,input,statement.line,".shaii files cannot import other .shaii libraries"); break;
            }
            const fs::path target(statement.value);
            if(target.extension()!=required) {
                diagnose(ctx,input,statement.line,std::string("$")+(library?"shaiiport":"shazport")+" requires a "+required+" file"); break;
            }
            execute_file(shay::resolve_import(input, statement.value, library),ctx); break;
        }
        case shay::StatementKind::Declare:
            if(extension==".shaz") { diagnose(ctx,input,statement.line,"Zeccurs files are variable-free; 'brew' is not allowed"); break; }
            if(!statement.value.empty() && statement.value.front()==StringMarker) ctx.values[statement.name]=statement.value.substr(1);
            else ctx.values[statement.name]=statement.value;
            break;
        case shay::StatementKind::OutputText:
            if(!ctx.check_only) shay_runtime_write((statement.value+"\n").c_str());
            break;
        case shay::StatementKind::OutputValue: {
            const auto found=ctx.values.find(statement.name);
            if(found==ctx.values.end()) diagnose(ctx,input,statement.line,"unknown variable '"+statement.name+"'");
            else if(!ctx.check_only) shay_runtime_write((found->second+"\n").c_str());
            break;
        }
        case shay::StatementKind::Invalid: break;
        }
    }
    ctx.active.erase(key); ctx.loaded.insert(key);
}
}
int main(int argc,char **argv) {
    if(argc==2 && std::string(argv[1])=="--version") { std::cout<<"shaii 0.2.0 (Shay interpreter)\n"; return 0; }
    bool check=false; fs::path source;
    for(int i=1;i<argc;++i) {
        const std::string arg=argv[i];
        if(arg=="--check") check=true;
        else if(arg=="--help" || arg=="-h") {
            std::cout<<"Usage: shaii [--check] program.shay|program.shaz|library.shaii\n       shaii --version\n";
            return 0;
        } else if(!arg.empty() && arg[0]=='-') { std::cerr<<"shaii: unknown option: "<<arg<<"\n"; return 2; }
        else if(source.empty()) source=arg;
        else { std::cerr<<"shaii: only one entry file is accepted\n"; return 2; }
    }
    if(source.empty()) { std::cerr<<"shaii: no input file (try --help)\n"; return 2; }
    Context context; context.check_only=check; execute_file(source,context);
    if(!context.diagnostics.empty()) {
        for(const auto &d:context.diagnostics) std::cerr<<d.file.string()<<':'<<d.line<<": error: "<<d.message<<'\n';
        return 1;
    }
    if(check) std::cout<<"shaii: "<<source.string()<<": no errors\n";
    return 0;
}
