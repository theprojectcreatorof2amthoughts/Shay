#include "shui.hpp"
#include "shuil.hpp"
#ifdef _WIN32
#include "shui_win32.hpp"
#endif
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace fs=std::filesystem;
namespace {
void print_errors(const fs::path&file,const std::vector<shay::LexError>&errors){
 for(const auto&e:errors)std::cerr<<file.string()<<":"<<e.line<<":"<<e.column<<": error: "<<e.message<<"\n";
}
void print_errors(const fs::path&file,const std::vector<shay::ui::ParseError>&errors){
 for(const auto&e:errors)std::cerr<<file.string()<<":"<<e.line<<":"<<e.column<<": error: "<<e.message<<"\n";
}
void print_errors(const fs::path&file,const std::vector<shay::ui::library::ParseError>&errors){
 for(const auto&e:errors)std::cerr<<file.string()<<":"<<e.line<<":"<<e.column<<": error: "<<e.message<<"\n";
}
}
int main(int argc,char**argv){
 if(argc<2||std::string(argv[1])=="--help"||std::string(argv[1])=="-h"){
  std::cout<<"Usage:\n  shui validate app.shui\n  shui validate-library platform.shuil\n  shui build app.shui --target web -o app.html\n  shui run app.shui --target desktop\n";return argc<2?2:0;
 }
 const std::string command=argv[1];
 if(command=="validate-library"){
  if(argc!=3){std::cerr<<"shui: validate-library requires exactly one .shuil file\n";return 2;}
  fs::path input=argv[2];
  if(input.extension()!=".shuil"){std::cerr<<"shui: library input must have the .shuil extension\n";return 2;}
  std::ifstream stream(input,std::ios::binary);if(!stream){std::cerr<<"shui: cannot open '"<<input.string()<<"'\n";return 1;}
  std::ostringstream buffer;buffer<<stream.rdbuf();
  auto parsed=shay::ui::library::parse(buffer.str());
  if(!parsed.ok()){print_errors(input,parsed.errors);return 1;}
  std::cout<<"shui: "<<input.string()<<" is valid ("<<parsed.config.platform<<", "<<parsed.config.translation_method
           <<", language "<<parsed.config.language<<", "<<parsed.config.methods.size()<<" method(s))\n";
  return 0;
 }
 if(command!="validate"&&command!="build"&&command!="run"){std::cerr<<"shui: expected 'validate', 'validate-library', 'build', or 'run'\n";return 2;}
 if(argc<3){std::cerr<<"shui: missing .shui input file\n";return 2;}
 fs::path input=argv[2];if(input.extension()!=".shui"){std::cerr<<"shui: input must have the .shui extension\n";return 2;}
 std::ifstream stream(input,std::ios::binary);if(!stream){std::cerr<<"shui: cannot open '"<<input.string()<<"'\n";return 1;}
 std::ostringstream buffer;buffer<<stream.rdbuf();const std::string source=buffer.str();
 shay::Lexer lexer(source);const auto tokens=lexer.scan();if(!lexer.errors().empty()){print_errors(input,lexer.errors());return 1;}
 auto parsed=shay::ui::parse(tokens);if(!parsed.ok()){print_errors(input,parsed.errors);return 1;}
 auto validation=shay::ui::validate(parsed.app);if(!validation.empty()){print_errors(input,validation);return 1;}
 if(command=="validate"){std::cout<<"shui: "<<input.string()<<" is valid ("<<parsed.app.nodes.size()<<" root element(s))\n";return 0;}
 std::string target=command=="run"?"desktop":"web";fs::path output;
 for(int i=3;i<argc;++i){const std::string arg=argv[i];if(arg=="--target"&&i+1<argc)target=argv[++i];else if(arg=="-o"&&i+1<argc)output=argv[++i];else{std::cerr<<"shui: unknown or incomplete option '"<<arg<<"'\n";return 2;}}
 if(command=="run"){
  if(!output.empty()){std::cerr<<"shui: '-o' is only valid with 'build'\n";return 2;}
  if(target!="desktop"){std::cerr<<"shui: 'run' currently supports only the desktop target\n";return 2;}
#ifdef _WIN32
  return shay::ui::run_windows(parsed.app);
#else
  std::cerr<<"shui: the desktop target requires Windows; use 'build --target web' on this platform\n";return 2;
#endif
 }
 if(target!="web"){std::cerr<<"shui: target '"<<target<<"' is not implemented for build (available: web)\n";return 2;}
 if(output.empty()){output=input.parent_path()/(input.stem().string()+".html");}
 std::ofstream html(output,std::ios::binary);if(!html){std::cerr<<"shui: cannot write '"<<output.string()<<"'\n";return 1;}
 html<<shay::ui::render_html(parsed.app);if(!html){std::cerr<<"shui: failed while writing '"<<output.string()<<"'\n";return 1;}
 std::cout<<"shui: wrote browser app "<<output.string()<<"\n";return 0;
}
