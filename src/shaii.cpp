#include "ast.hpp"
#include "expression.hpp"
#include "imports.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "runtime.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
namespace fs=std::filesystem;
namespace {
constexpr char StringMarker='\x01';
struct Diagnostic{fs::path file;std::size_t line;std::string message;};
struct Context{
 std::unordered_map<std::string,std::string> values;
 std::unordered_map<std::string,double> numbers;
 std::unordered_map<std::string,shay::Statement> functions;
 std::unordered_set<std::string> loaded,active;
 std::vector<Diagnostic> diagnostics;
 bool check_only=false;
};
void diagnose(Context&ctx,const fs::path&file,std::size_t line,const std::string&message){ctx.diagnostics.push_back({file,line,message});}
bool evaluate(const std::string&source,const std::unordered_map<std::string,double>&values,double&result,std::string&error){
 shay::NumericExpression expression(source,values);return expression.evaluate(result,error);
}
void execute_file(const fs::path&input,Context&ctx){
 std::error_code ec;const fs::path canonical=fs::weakly_canonical(input,ec);const std::string key=(ec?input:canonical).string();
 if(ctx.active.count(key)){diagnose(ctx,input,1,"circular import detected");return;}
 if(ctx.loaded.count(key))return;
 const std::string extension=input.extension().string();
 if(extension!=".shay"&&extension!=".shaz"&&extension!=".shaii"){diagnose(ctx,input,1,"unsupported file extension (expected .shay, .shaz, or .shaii)");return;}
 std::ifstream stream(input,std::ios::binary);if(!stream){diagnose(ctx,input,1,"cannot open source file");return;}
 const std::string source((std::istreambuf_iterator<char>(stream)),{});
 shay::Lexer lexer(source);const auto tokens=lexer.scan();shay::Parser parser(tokens);const shay::Program program=parser.parse();
 for(const auto&e:lexer.errors())diagnose(ctx,input,e.line,e.message);
 for(const auto&e:program.errors)diagnose(ctx,input,e.line,e.message);
 ctx.active.insert(key);
 for(const auto&s:program.statements){
  switch(s.kind){
   case shay::StatementKind::ImportLibrary:case shay::StatementKind::ImportTask:{
    const bool library=s.kind==shay::StatementKind::ImportLibrary;const std::string required=library?".shaii":".shaz";
    if(extension==".shaii"&&library){diagnose(ctx,input,s.line,".shaii files cannot import other .shaii libraries");break;}
    if(fs::path(s.value).extension()!=required){diagnose(ctx,input,s.line,std::string("$")+(library?"shaiiport":"shazport")+" requires a "+required+" file");break;}
    execute_file(shay::resolve_import(input,s.value,library),ctx);break;
   }
   case shay::StatementKind::Declare:{
    if(extension==".shaz"){diagnose(ctx,input,s.line,"Zeccurs files are variable-free; 'brew' is not allowed");break;}
    if(!s.value.empty()&&s.value.front()==StringMarker){ctx.values[s.name]=s.value.substr(1);ctx.numbers.erase(s.name);}
    else{ctx.values[s.name]=s.value;try{std::size_t used=0;double value=std::stod(s.value,&used);if(used==s.value.size())ctx.numbers[s.name]=value;else ctx.numbers.erase(s.name);}catch(...){ctx.numbers.erase(s.name);}}
    break;
   }
   case shay::StatementKind::DeclareFunction:
    if(ctx.functions.count(s.name))diagnose(ctx,input,s.line,"function '"+s.name+"' is already declared");
    else ctx.functions.emplace(s.name,s);
    break;
   case shay::StatementKind::OutputText:
    if(!ctx.check_only)shay_runtime_write((s.value+"\n").c_str());
    break;
   case shay::StatementKind::OutputValue:{
    auto found=ctx.values.find(s.name);
    if(found==ctx.values.end())diagnose(ctx,input,s.line,"unknown variable '"+s.name+"'");
    else if(!ctx.check_only)shay_runtime_write((found->second+"\n").c_str());
    break;
   }
   case shay::StatementKind::OutputCall:{
    auto found=ctx.functions.find(s.name);
    if(found==ctx.functions.end()){diagnose(ctx,input,s.line,"unknown function '"+s.name+"'");break;}
    const auto&fn=found->second;
    if(!fn.has_return){diagnose(ctx,input,s.line,"function '"+s.name+"' does not return a value");break;}
    if(s.arguments.size()!=fn.parameters.size()){diagnose(ctx,input,s.line,"function '"+s.name+"' expects "+std::to_string(fn.parameters.size())+" argument(s), got "+std::to_string(s.arguments.size()));break;}
    auto scope=ctx.numbers;bool args_ok=true;
    for(std::size_t i=0;i<s.arguments.size();++i){double value=0;std::string error;if(!evaluate(s.arguments[i],ctx.numbers,value,error)){diagnose(ctx,input,s.line,"invalid argument "+std::to_string(i+1)+" for '"+s.name+"': "+error);args_ok=false;break;}scope[fn.parameters[i]]=value;}
    if(!args_ok)break;
    double result=0;std::string error;
    if(!evaluate(fn.value,scope,result,error)){diagnose(ctx,input,fn.line,"invalid return expression in '"+s.name+"': "+error);break;}
    if(!ctx.check_only){std::ostringstream output;output<<std::setprecision(15)<<result<<"\n";shay_runtime_write(output.str().c_str());}
    break;
   }
   case shay::StatementKind::Invalid:break;
  }
 }
 ctx.active.erase(key);ctx.loaded.insert(key);
}
}
int main(int argc,char**argv){
 if(argc==2&&std::string(argv[1])=="--version"){std::cout<<"shaii 0.3.0 (Shay interpreter)\n";return 0;}
 bool check=false;fs::path source;
 for(int i=1;i<argc;++i){const std::string arg=argv[i];
  if(arg=="--check")check=true;
  else if(arg=="--help"||arg=="-h"){std::cout<<"Usage: shaii [--check] program.shay|program.shaz|library.shaii\n       shaii --version\n";return 0;}
  else if(!arg.empty()&&arg[0]=='-'){std::cerr<<"shaii: unknown option: "<<arg<<"\n";return 2;}
  else if(source.empty())source=arg;else{std::cerr<<"shaii: only one entry file is accepted\n";return 2;}
 }
 if(source.empty()){std::cerr<<"shaii: no input file (try --help)\n";return 2;}
 Context context;context.check_only=check;execute_file(source,context);
 if(!context.diagnostics.empty()){for(const auto&d:context.diagnostics)std::cerr<<d.file.string()<<':'<<d.line<<": error: "<<d.message<<'\n';return 1;}
 if(check)std::cout<<"shaii: "<<source.string()<<": no errors\n";
 return 0;
}
