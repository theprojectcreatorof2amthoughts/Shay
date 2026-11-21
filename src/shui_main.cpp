#include "shui.hpp"
#include "shui_language.hpp"
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
}
int main(int argc,char**argv){
 if(argc<2||std::string(argv[1])=="--help"||std::string(argv[1])=="-h"){
  std::cout<<"Usage:\n  shui validate app.shui\n  shui build app.shui --target web -o app.html\n  shui run app.shui --target desktop\n";return argc<2?2:0;
 }
 const std::string command=argv[1];if(command!="validate"&&command!="build"&&command!="run"){std::cerr<<"shui: expected 'validate', 'build', or 'run'\n";return 2;}
 if(argc<3){std::cerr<<"shui: missing .shui input file\n";return 2;}
 fs::path input=argv[2];if(input.extension()!=".shui"&&input.extension()!=".shuil"){std::cerr<<"shui: input must have the .shui or .shuil extension\n";return 2;}
 std::ifstream stream(input,std::ios::binary);if(!stream){std::cerr<<"shui: cannot open '"<<input.string()<<"'\n";return 1;}
 std::ostringstream buffer;buffer<<stream.rdbuf();const std::string source=buffer.str();
 const bool directive_syntax=input.extension()==".shuil"||source.find("$useentry UI(")!=std::string::npos;
 if(directive_syntax){
  const auto checked=shay::ui::language::validate_source(source,input.extension().string());
  for(const auto&e:checked.diagnostics)std::cerr<<input.string()<<":"<<e.line<<":1: error: "<<e.message<<"\n";
  if(!checked.ok())return 1;
  if(command!="validate"){
   std::cerr<<"shui: directive-based .shui/.shuil syntax validation is implemented; translation/build and runtime execution are the next stage\n";
   return 2;
  }
  std::cout<<"shui: "<<input.string()<<" is valid (directive-based UI syntax)\n";
  return 0;
 }
 if(input.extension()==".shuil"){
  std::cerr<<"shui: .shuil files support validation only\\n";
  return 2;
 }
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
