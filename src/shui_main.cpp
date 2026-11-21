#include "shui.hpp"
#include "shui_language.hpp"
#ifdef _WIN32
#include "shui_win32.hpp"
#endif
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
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
 if(input.extension()==".shuil"){
  const auto checked=shay::ui::language::validate_source(source,".shuil");
  for(const auto&e:checked.diagnostics)std::cerr<<input.string()<<":"<<e.line<<":1: error: "<<e.message<<"\n";
  if(!checked.ok())return 1;
  if(command!="validate"){std::cerr<<"shui: pass the .shui application file to build; .shuil is its platform adapter\n";return 2;}
  std::cout<<"shui: "<<input.string()<<" is valid (platform adapter)\n";return 0;
 }
 const bool directive_syntax=source.find("$useentry UI(")!=std::string::npos;
 if(directive_syntax){
  const auto checked=shay::ui::language::validate_source(source,".shui");
  for(const auto&e:checked.diagnostics)std::cerr<<input.string()<<":"<<e.line<<":1: error: "<<e.message<<"\n";
  if(!checked.ok())return 1;
  std::smatch adapter_match;
  if(!std::regex_search(source,adapter_match,std::regex(R"(\*#uiContrl\s+"([^"]+\.shuil)")"))){
   std::cerr<<"shui: .shui app must select a .shuil adapter with *#uiContrl\n";return 1;
  }
  fs::path adapter_path=input.parent_path()/adapter_match[1].str();
  std::ifstream adapter_stream(adapter_path,std::ios::binary);
  if(!adapter_stream){std::cerr<<"shui: cannot open platform adapter '"<<adapter_path.string()<<"'\n";return 1;}
  std::ostringstream adapter_buffer;adapter_buffer<<adapter_stream.rdbuf();
  const std::string adapter_source=adapter_buffer.str();
  shay::ui::language::AdapterConfig adapter;
  shay::ui::language::Result adapter_result;
  if(!shay::ui::language::inspect_adapter(adapter_source,adapter,adapter_result)){
   for(const auto&e:adapter_result.diagnostics)std::cerr<<adapter_path.string()<<":"<<e.line<<":1: error: "<<e.message<<"\n";
   return 1;
  }
  if(command=="validate"){
   std::cout<<"shui: "<<input.string()<<" and "<<adapter_path.string()<<" are valid\n";return 0;
  }
  if(command=="run"){
   std::cerr<<"shui: run is not available for directive-based apps yet; build the generated project first\n";return 2;
  }
  std::string target=adapter.platform;
  fs::path output_path;
  for(int i=3;i<argc;++i){
   const std::string arg=argv[i];
   if(arg=="--target"&&i+1<argc)target=argv[++i];
   else if(arg=="-o"&&i+1<argc)output_path=argv[++i];
   else{std::cerr<<"shui: unknown or incomplete option '"<<arg<<"'\n";return 2;}
  }
  if(target!=adapter.platform){std::cerr<<"shui: selected target '"<<target<<"' does not match adapter platform '"<<adapter.platform<<"'\n";return 2;}
  std::string generated,extension;
  shay::ui::language::Result generation;
  if(!shay::ui::language::generate_output(source,adapter,input.stem().string(),generated,extension,generation)){
   for(const auto&e:generation.diagnostics)std::cerr<<input.string()<<":"<<e.line<<":1: error: "<<e.message<<"\n";
   return 1;
  }
  if(output_path.empty())output_path=input.parent_path()/(input.stem().string()+extension);
  std::ofstream generated_file(output_path,std::ios::binary);
  if(!generated_file){std::cerr<<"shui: cannot write '"<<output_path.string()<<"'\n";return 1;}
  generated_file<<generated;
  if(!generated_file){std::cerr<<"shui: failed while writing '"<<output_path.string()<<"'\n";return 1;}
  if(adapter.platform=="windows"&&adapter.method=="csharpTrans"){
   fs::path project_path=output_path;project_path.replace_extension(".csproj");
   std::ofstream project_file(project_path,std::ios::binary);
   if(!project_file){std::cerr<<"shui: cannot write Windows project '"<<project_path.string()<<"'\n";return 1;}
   project_file<<"<Project Sdk=\"Microsoft.NET.Sdk\">\n"
               <<"  <PropertyGroup>\n"
               <<"    <OutputType>WinExe</OutputType>\n"
               <<"    <TargetFramework>net8.0-windows</TargetFramework>\n"
               <<"    <UseWindowsForms>true</UseWindowsForms>\n"
               <<"    <EnableWindowsTargeting>true</EnableWindowsTargeting>\n"
               <<"    <ImplicitUsings>enable</ImplicitUsings>\n"
               <<"    <Nullable>enable</Nullable>\n"
               <<"  </PropertyGroup>\n"
               <<"</Project>\n";
   if(!project_file){std::cerr<<"shui: failed while writing Windows project\n";return 1;}
   std::cout<<"shui: generated Windows Forms project "<<project_path.string()<<"\n";
  }
  std::cout<<"shui: generated "<<adapter.platform<<" UI source "<<output_path.string()<<"\n";return 0;
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
