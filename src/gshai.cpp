#include "ast.hpp"
#include "imports.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
namespace fs=std::filesystem;
namespace {
constexpr char StringMarker='\x01';
struct Diagnostic{fs::path file;std::size_t line;std::string message;};
struct BuildContext{
 std::unordered_set<std::string>loaded,active,variables;
 std::unordered_map<std::string,shay::Statement>functions;
 std::vector<Diagnostic>diagnostics;
 std::vector<shay::Statement>statements;
};
void diagnose(BuildContext&ctx,const fs::path&file,std::size_t line,const std::string&message){ctx.diagnostics.push_back({file,line,message});}
std::unordered_set<std::string> expression_names(const std::string&source){
 std::unordered_set<std::string> names;const std::regex identifier(R"([A-Za-z_][A-Za-z0-9_]*)");
 for(std::sregex_iterator it(source.begin(),source.end(),identifier),end;it!=end;++it)names.insert(it->str());
 return names;
}
void validate_expression_names(BuildContext&ctx,const fs::path&file,std::size_t line,const std::string&source,const std::unordered_set<std::string>&locals,const std::string&label){
 for(const auto&name:expression_names(source))if(!ctx.variables.count(name)&&!locals.count(name)&&name!="true"&&name!="false")
  diagnose(ctx,file,line,label+" references unknown variable '"+name+"'");
}
void validate_body(BuildContext&ctx,const fs::path&file,const std::vector<shay::Statement>&body,const std::unordered_set<std::string>&locals,const std::string&bodyLabel){
 for(const auto&s:body){
  if(s.kind==shay::StatementKind::If){
   validate_expression_names(ctx,file,s.line,s.condition,locals,"ifit condition");
   validate_body(ctx,file,s.body,locals,bodyLabel);
  }else if(s.kind==shay::StatementKind::OutputValue){
   if(!ctx.variables.count(s.name)&&!locals.count(s.name))
    diagnose(ctx,file,s.line,"unknown variable '"+s.name+"' inside "+bodyLabel);
  }else if(s.kind==shay::StatementKind::OutputCall){
   auto fn=ctx.functions.find(s.name);
   if(fn==ctx.functions.end())diagnose(ctx,file,s.line,"unknown function '"+s.name+"' inside "+bodyLabel);
   else if(s.arguments.size()!=fn->second.parameters.size())
    diagnose(ctx,file,s.line,"function '"+s.name+"' expects "+std::to_string(fn->second.parameters.size())+" argument(s), got "+std::to_string(s.arguments.size()));
   for(const auto&arg:s.arguments)validate_expression_names(ctx,file,s.line,arg,locals,"function argument");
  }else if(s.kind!=shay::StatementKind::OutputText){
   diagnose(ctx,file,s.line,"unsupported statement inside function body");
  }
 }
}
void load_file(const fs::path&input,BuildContext&ctx){
 std::error_code ec;const fs::path canonical=fs::weakly_canonical(input,ec);const std::string key=(ec?input:canonical).string();
 if(ctx.active.count(key)){diagnose(ctx,input,1,"circular import detected");return;}if(ctx.loaded.count(key))return;
 const std::string extension=input.extension().string();
 if(extension!=".shay"&&extension!=".shaz"&&extension!=".shaii"){diagnose(ctx,input,1,"unsupported file extension (expected .shay, .shaz, or .shaii)");return;}
 std::ifstream stream(input,std::ios::binary);if(!stream){diagnose(ctx,input,1,"cannot open source file");return;}
 const std::string source((std::istreambuf_iterator<char>(stream)),std::istreambuf_iterator<char>());
 shay::Lexer lexer(source);const auto tokens=lexer.scan();shay::Parser parser(tokens);const shay::Program program=parser.parse();
 for(const auto&e:lexer.errors())diagnose(ctx,input,e.line,e.message);
 for(const auto&e:program.errors)diagnose(ctx,input,e.line,e.message);
 ctx.active.insert(key);
 for(const auto&s:program.statements){
  if(s.kind==shay::StatementKind::ImportLibrary||s.kind==shay::StatementKind::ImportTask){
   const bool library=s.kind==shay::StatementKind::ImportLibrary;const std::string required=library?".shaii":".shaz";
   if(extension==".shaii"&&library){diagnose(ctx,input,s.line,".shaii files cannot import other .shaii libraries");continue;}
   if(fs::path(s.value).extension()!=required){diagnose(ctx,input,s.line,std::string("$")+(library?"shaiiport":"shazport")+" requires a "+required+" file");continue;}
   load_file(shay::resolve_import(input,s.value,library&&!s.external_import),ctx);continue;
  }
  if(s.kind==shay::StatementKind::Declare){
   if(extension==".shaz"){diagnose(ctx,input,s.line,"Zeccurs files are variable-free; 'brew' is not allowed");continue;}
   if(!ctx.variables.insert(s.name).second){diagnose(ctx,input,s.line,"variable '"+s.name+"' is already declared");continue;}
  }else if(s.kind==shay::StatementKind::DeclareFunction){
   if(ctx.functions.count(s.name)){diagnose(ctx,input,s.line,"function '"+s.name+"' is already declared");continue;}
   std::unordered_set<std::string> parameters(s.parameters.begin(),s.parameters.end());
   if(s.has_return)validate_expression_names(ctx,input,s.line,s.value,parameters,"function '"+s.name+"' return expression");
   validate_body(ctx,input,s.body,parameters,"function body");
   ctx.functions.emplace(s.name,s);
  }else if(s.kind==shay::StatementKind::If){
   validate_expression_names(ctx,input,s.line,s.condition,{},"ifit condition");
   validate_body(ctx,input,s.body,{},"ifit block");
  }else if(s.kind==shay::StatementKind::OutputValue&&!ctx.variables.count(s.name)){
   diagnose(ctx,input,s.line,"unknown variable '"+s.name+"'");
  }else if(s.kind==shay::StatementKind::OutputCall){
   auto fn=ctx.functions.find(s.name);
   if(fn==ctx.functions.end())diagnose(ctx,input,s.line,"unknown function '"+s.name+"'");
   else if(s.arguments.size()!=fn->second.parameters.size())
    diagnose(ctx,input,s.line,"function '"+s.name+"' expects "+std::to_string(fn->second.parameters.size())+" argument(s), got "+std::to_string(s.arguments.size()));
   for(const auto&arg:s.arguments)validate_expression_names(ctx,input,s.line,arg,{},"function argument");
  }
  ctx.statements.push_back(s);
 }
 ctx.active.erase(key);ctx.loaded.insert(key);
}
std::string cpp_string(const std::string&value){
 std::ostringstream out;out<<'"';for(unsigned char c:value){switch(c){case '\\':out<<"\\\\";break;case '"':out<<"\\\"";break;case '\n':out<<"\\n";break;case '\r':out<<"\\r";break;case '\t':out<<"\\t";break;default:if(c<32||c>=127){const char octal[]="01234567";out<<"\\"<<octal[(c>>6)&7]<<octal[(c>>3)&7]<<octal[c&7];}else out<<static_cast<char>(c);}}out<<'"';return out.str();
}
std::string cpp_condition(const std::string&condition){
 std::regex identifier(R"([A-Za-z_][A-Za-z0-9_]*)");std::string out;std::sregex_iterator it(condition.begin(),condition.end(),identifier),end;std::size_t last=0;
 for(;it!=end;++it){const auto&m=*it;out+=condition.substr(last,static_cast<std::size_t>(m.position())-last);const std::string name=m.str();out+="std::stod(values["+cpp_string(name)+"])";last=static_cast<std::size_t>(m.position()+m.length());}
 out+=condition.substr(last);return out;
}
std::string shell_quote(const std::string&value){
#ifdef _WIN32
 std::string out="\"";
 for(char c:value){if(c=='\"')out+="\\\"";else out+=c;}
 return out+"\"";
#else
 std::string out="'";for(char c:value){if(c=='\\'')out+="'\\\\''";else out+=c;}return out+"'";
#endif
}
void emit_body(std::ostream&out,const std::vector<shay::Statement>&body,const BuildContext&ctx,const std::string&indent){
 for(const auto&s:body){
  switch(s.kind){
   case shay::StatementKind::OutputText:
    out<<indent<<"shay_runtime_write("<<cpp_string(s.value+"\n")<<");\n";break;
   case shay::StatementKind::OutputValue:
    out<<indent<<"shay_runtime_write((values["<<cpp_string(s.name)<<" ] + "<<cpp_string("\n")<<").c_str());\n";break;
   case shay::StatementKind::If:
    out<<indent<<"if ("<<cpp_condition(s.condition)<<") {\n";
    emit_body(out,s.body,ctx,indent+" ");
    out<<indent<<"}\n";break;
   case shay::StatementKind::OutputCall:{
    auto fn=ctx.functions.find(s.name);
    if(fn==ctx.functions.end())break;
    out<<indent;
    if(fn->second.has_return)out<<"{ std::ostringstream shay_nested_call; shay_nested_call<<std::setprecision(15)<<";
    out<<"shay_fn_"<<s.name<<"(values";
    for(const auto&arg:s.arguments)out<<", "<<arg;
    out<<")";
    if(fn->second.has_return)out<<"; shay_nested_call<<"<<cpp_string("\n")<<"; shay_runtime_write(shay_nested_call.str().c_str()); }";
    out<<";\n";break;
   }
   default:break;
  }
 }
}
bool write_translation(const BuildContext&ctx,const fs::path&path,bool add_main){
 std::ofstream out(path,std::ios::binary);if(!out)return false;
 out<<"#include <cstdio>\n#include <iomanip>\n#include <sstream>\n#include <string>\n#include <unordered_map>\n"
       "extern \"C\" void shay_runtime_write(const char* text) { if(text){std::fputs(text,stdout);std::fflush(stdout);} }\n";
 for(const auto&s:ctx.statements)if(s.kind==shay::StatementKind::DeclareFunction){
  out<<"static "<<(s.has_return?"double":"void")<<" shay_fn_"<<s.name<<"(std::unordered_map<std::string,std::string>& global_values";
  for(const auto&p:s.parameters)out<<", double "<<p;
  out<<");\n";
 }
 for(const auto&s:ctx.statements)if(s.kind==shay::StatementKind::DeclareFunction){
  out<<"static "<<(s.has_return?"double":"void")<<" shay_fn_"<<s.name<<"(std::unordered_map<std::string,std::string>& global_values";
  for(const auto&p:s.parameters)out<<", double "<<p;
  out<<") {\n auto values = global_values;\n";
  for(const auto&p:s.parameters)out<<" values["<<cpp_string(p)<<"] = std::to_string("<<p<<");\n";
  if(s.has_return)out<<" return ("<<s.value<<");\n";
  else emit_body(out,s.body,ctx," ");
  out<<"}\n";
 }
 out<<"extern \"C\" int shay_program_entry() {\n std::unordered_map<std::string,std::string> values;\n";
 for(const auto&s:ctx.statements){
  switch(s.kind){
   case shay::StatementKind::Declare:{
    std::string value=s.value;if(!value.empty()&&value.front()==StringMarker)value.erase(value.begin());
    out<<" values["<<cpp_string(s.name)<<"] = "<<cpp_string(value)<<";\n";break;
   }
   case shay::StatementKind::If:
    out<<" if ("<<cpp_condition(s.condition)<<") {\n";
    emit_body(out,s.body,ctx," ");
    out<<" }\n";break;
   case shay::StatementKind::OutputText:out<<" shay_runtime_write("<<cpp_string(s.value+"\n")<<");\n";break;
   case shay::StatementKind::OutputValue:out<<" shay_runtime_write((values["<<cpp_string(s.name)<<"] + "<<cpp_string("\n")<<").c_str());\n";break;
   case shay::StatementKind::OutputCall:{
    auto fn=ctx.functions.find(s.name);if(fn==ctx.functions.end())break;
    out<<" ";
    if(fn->second.has_return)out<<"{ std::ostringstream shay_call_output; shay_call_output<<std::setprecision(15)<<";
    out<<"shay_fn_"<<s.name<<"(values";
    for(const auto&arg:s.arguments)out<<", "<<arg;
    out<<")";
    if(fn->second.has_return)out<<"; shay_call_output<<"<<cpp_string("\n")<<"; shay_runtime_write(shay_call_output.str().c_str()); }";
    out<<";\n";break;
   }
   default:break;
  }
 }
 out<<" return 0;\n}\n";if(add_main)out<<"int main(){return shay_program_entry();}\n";return static_cast<bool>(out);
}
void print_diagnostics(const BuildContext&ctx){for(const auto&d:ctx.diagnostics)std::cerr<<d.file.string()<<':'<<d.line<<": error: "<<d.message<<'\n';}
}
int main(int argc,char**argv){
 bool warn_all=false,warn_extra=false;std::string optimization="2";
 std::vector<std::string> positional;
 for(int i=1;i<argc;++i){
  const std::string arg=argv[i];
  if(arg=="--shaywall")warn_all=true;
  else if(arg=="--shaywextra")warn_extra=true;
  else if(arg=="--shayo0")optimization="0";
  else if(arg=="--shayo1")optimization="1";
  else if(arg=="--shayo2")optimization="2";
  else if(arg=="--shayo3")optimization="3";
  else positional.push_back(arg);
 }
 if(positional.size()==1&&positional[0]=="--version"){std::cout<<"gshai 0.4.0 (Shay compiler)\n";return 0;}
 if(positional.size()==1&&(positional[0]=="--help"||positional[0]=="-h")){
  std::cout<<"Usage:\n  gshai [Shay flags] -ob [output.sho] program.shay   Compile a native object\n  gshai [Shay flags] -o  [output]     program.shay   Compile a runnable executable\n  gshai --version\n\n"
           <<"-ob emits .sho object files; -o links a standalone executable.\n"
           <<"Shay-native compiler flags (may appear anywhere):\n"
           <<"  --shaywall    Enable all common native compiler warnings (-Wall)\n"
           <<"  --shaywextra  Enable additional native compiler warnings (-Wextra)\n"
           <<"  --shayo0      Disable optimization (-O0)\n"
           <<"  --shayo1      Enable level 1 optimization (-O1)\n"
           <<"  --shayo2      Enable level 2 optimization (-O2; default)\n"
           <<"  --shayo3      Enable level 3 optimization (-O3)\n";
  return 0;
 }
 if(positional.size()<2||positional.size()>3){std::cerr<<"gshai: expected -ob or -o and a .shay input (try --help)\n";return 2;}
 const std::string mode=positional[0];if(mode!="-ob"&&mode!="-o"){std::cerr<<"gshai: unknown output mode: "<<mode<<'\n';return 2;}
 const bool object_mode=mode=="-ob";fs::path output,input;
 if(positional.size()==2)input=positional[1];else{output=positional[1];input=positional[2];}
 if(input.extension()!=".shay"){std::cerr<<"gshai: input must be a .shay file\n";return 2;}
 if(output.empty()){output=input.parent_path()/input.stem();if(object_mode)output+=".sho";}
 else if(object_mode&&output.extension()!=".sho"){std::cerr<<"gshai: -ob output must use the .sho extension\n";return 2;}
 BuildContext context;load_file(input,context);if(!context.diagnostics.empty()){print_diagnostics(context);return 1;}
 const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();const fs::path temp=fs::temp_directory_path()/("shay-build-"+std::to_string(stamp)+".cpp");
 if(!write_translation(context,temp,!object_mode)){std::cerr<<"gshai: cannot write temporary translation unit\n";return 1;}
 const char*configured=std::getenv("CXX");const std::string cxx=configured&&*configured?configured:"c++";
 std::string command=shell_quote(cxx)+" -std=c++20 -O"+optimization+" ";
 if(warn_all)command+="-Wall ";
 if(warn_extra)command+="-Wextra ";
 command+=(object_mode?"-c ":"")+shell_quote(temp.string())+" -o "+shell_quote(output.string());
 const int status=std::system(command.c_str());std::error_code ignored;fs::remove(temp,ignored);
 if(status!=0){fs::remove(output,ignored);std::cerr<<"gshai: native compiler/linker failed\n";return 1;}
 std::cout<<"gshai: wrote "<<(object_mode?"object file ":"executable ")<<output.string()<<'\n';return 0;
}
