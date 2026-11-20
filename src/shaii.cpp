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
 std::unordered_map<std::string,std::vector<std::string>> classes;
 std::unordered_set<std::string> loaded,active;
 std::vector<Diagnostic> diagnostics;
 bool check_only=false;
 std::unordered_map<std::string,std::string> failures;
 std::unordered_set<std::string> handled_failures;
 std::vector<std::string> active_functions;
 bool exit_requested=false;
 std::string log_file_name="shay";
 std::string log_file_extension=".log";
 bool log_file_name_set=false;
 bool log_file_extension_set=false;
};
using NumberScope=std::unordered_map<std::string,double>;
struct ReturnFlow{bool returned=false;double value=0;};
void diagnose(Context&ctx,const fs::path&file,std::size_t line,const std::string&message){ctx.diagnostics.push_back({file,line,message});}
bool evaluate(const std::string&source,const NumberScope&values,double&result,std::string&error){
 shay::NumericExpression expression(source,values);return expression.evaluate(result,error);
}
std::string number_text(double value){std::ostringstream out;out<<std::setprecision(15)<<value;return out.str();}
fs::path configured_log_path(const Context&ctx){fs::path path(ctx.log_file_name);path.replace_extension(ctx.log_file_extension);return path;}
bool log_configuration_ready(const Context&ctx){return ctx.log_file_name_set||ctx.log_file_extension_set;}
bool append_log(const Context&ctx,const std::string&message){
 std::ofstream stream(configured_log_path(ctx),std::ios::binary|std::ios::app);
 if(!stream)return false;
 stream.write(message.data(),static_cast<std::streamsize>(message.size()));
 if(message.empty()||message.back()!='\n')stream.put('\n');
 return static_cast<bool>(stream);
}
bool evaluate_condition(const std::string&source,const NumberScope&scope,Context&ctx,bool&result,std::string&error){
 auto trim=[](const std::string&value){const auto first=value.find_first_not_of(" \t\r\n");if(first==std::string::npos)return std::string();const auto last=value.find_last_not_of(" \t\r\n");return value.substr(first,last-first+1);};
 const std::string condition=trim(source);
 if(condition.rfind("!#",0)==0){
  const std::string name=trim(condition.substr(2));
  if(name.empty()){error="expected a function name after '!#'";return false;}
  result=ctx.failures.count(name)!=0;
  if(result)ctx.handled_failures.insert(name);
  return true;
 }
 return shay::evaluate_condition(condition,scope,result,error);
}
bool execute_statements(const std::vector<shay::Statement>&statements,Context&ctx,const fs::path&file,NumberScope&scope,std::string&failure,ReturnFlow*return_flow=nullptr);
bool call_function(const shay::Statement&call,Context&ctx,const fs::path&file,NumberScope&callerScope){
 ctx.failures.erase(call.name);
 ctx.handled_failures.erase(call.name);
 auto fail=[&](const std::string&reason){ctx.handled_failures.erase(call.name);ctx.failures[call.name]=reason;return false;};
 auto found=ctx.functions.find(call.name);
 if(found==ctx.functions.end())return fail("unknown function '"+call.name+"'");
 const auto&fn=found->second;
 if(call.arguments.size()!=fn.parameters.size())
  return fail("function '"+call.name+"' expects "+std::to_string(fn.parameters.size())+" argument(s), got "+std::to_string(call.arguments.size()));
 NumberScope local=callerScope;
 for(std::size_t i=0;i<call.arguments.size();++i){
  double value=0;std::string error;
  if(!evaluate(call.arguments[i],callerScope,value,error))
   return fail("invalid argument "+std::to_string(i+1)+" for '"+call.name+"': "+error);
  local[fn.parameters[i]]=value;
 }
 if(fn.has_return&&fn.body.empty()){
  double result=0;std::string error;
  if(!evaluate(fn.value,local,result,error))
   return fail("invalid return expression in '"+call.name+"': "+error);
  if(!ctx.check_only)shay_runtime_write((number_text(result)+"\n").c_str());
 }else{
  std::string reason;ReturnFlow flow;
  ctx.active_functions.push_back(call.name);
  const bool body_ok=execute_statements(fn.body,ctx,file,local,reason,&flow);
  ctx.active_functions.pop_back();
  if(!body_ok)return fail(reason);
  if(ctx.failures.count(call.name))return false;
  if(fn.has_return){
   if(!flow.returned)return fail("function '"+call.name+"' completed without returning a value");
   if(!ctx.check_only)shay_runtime_write((number_text(flow.value)+"\n").c_str());
  }
 }
 return true;
}
bool execute_class(const std::string&class_name,Context&ctx,const fs::path&file,NumberScope&scope,std::string&failure){
 auto found=ctx.classes.find(class_name);
 if(found==ctx.classes.end()){failure="unknown class '"+class_name+"'";return false;}
 for(const auto&method_name:found->second){
  shay::Statement call;call.name=method_name;
  if(!call_function(call,ctx,file,scope)){failure=ctx.failures[method_name];return false;}
 }
 return true;
}
bool execute_statements(const std::vector<shay::Statement>&statements,Context&ctx,const fs::path&file,NumberScope&scope,std::string&failure,ReturnFlow*return_flow){
 bool success=true;
 for(const auto&s:statements){
  if(ctx.exit_requested||(return_flow&&return_flow->returned))break;
  switch(s.kind){
   case shay::StatementKind::MarkFailure:
    if(ctx.active_functions.empty()||ctx.active_functions.back()!=s.name){
     failure="!#"+s.name+" can only mark the currently executing function";
     success=false;
    }else{
     ctx.failures[s.name]="function explicitly marked as failed";
     ctx.handled_failures.erase(s.name);
    }
    break;
   case shay::StatementKind::ReturnValue:{
    if(!return_flow){failure="'forw' is only valid inside a function";success=false;break;}
    double value=0;std::string error;
    if(!evaluate(s.value,scope,value,error)){failure="invalid return expression: "+error;success=false;break;}
    return_flow->value=value;return_flow->returned=true;
    break;
   }
   case shay::StatementKind::OutputText:
    if(!ctx.check_only)shay_runtime_write((s.value+"\n").c_str());
    break;
   case shay::StatementKind::LogText:
    if(!log_configuration_ready(ctx)){failure="'logit/>' requires '$logindxfl' or '$logindxnm' to be configured before use";success=false;}
    else if(!ctx.check_only&&!append_log(ctx,s.value)){failure="cannot write log file '"+configured_log_path(ctx).string()+"'";success=false;}
    break;
   case shay::StatementKind::OutputTextExit:
    if(!ctx.check_only)shay_runtime_write((s.value+"\n").c_str());
    ctx.exit_requested=true;
    failure="program terminated by instxtexit";
    return false;
   case shay::StatementKind::OutputValue:{
    for(const auto&name:s.output_names){
     auto number=scope.find(name);
     if(number!=scope.end()){
      if(!ctx.check_only)shay_runtime_write((number_text(number->second)+"\n").c_str());
      continue;
     }
     auto value=ctx.values.find(name);
     if(value!=ctx.values.end()){
      if(!ctx.check_only)shay_runtime_write((value->second+"\n").c_str());
     }else{
      failure="unknown variable '"+name+"'";
      success=false;
      break;
     }
    }
    break;
   }
   case shay::StatementKind::If:{
    bool condition=false;std::string error;
    if(!evaluate_condition(s.condition,scope,ctx,condition,error)){failure="invalid ifit condition: "+error;success=false;break;}
    std::string nestedFailure;
    const bool nestedOk=condition?execute_statements(s.body,ctx,file,scope,nestedFailure,return_flow):execute_statements(s.else_body,ctx,file,scope,nestedFailure,return_flow);
    if(!nestedOk&&!ctx.exit_requested){failure=nestedFailure;success=false;}
    break;
   }
   case shay::StatementKind::OutputCall:
    if(!call_function(s,ctx,file,scope)){failure=ctx.failures[s.name];success=false;}
    break;
   case shay::StatementKind::DoFunction:
    {shay::Statement call;call.name=s.name;if(!call_function(call,ctx,file,scope)){failure=ctx.failures[s.name];success=false;}}
    break;
   case shay::StatementKind::ExecuteClass:
    if(!execute_class(s.name,ctx,file,scope,failure))success=false;
    break;
   default:
    failure="statement is not supported inside a function body";
    success=false;
    break;
  }
 }
 return success&&!ctx.exit_requested;
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
  if(ctx.exit_requested)break;
  switch(s.kind){
   case shay::StatementKind::ImportLibrary:case shay::StatementKind::ImportTask:{
    const bool library=s.kind==shay::StatementKind::ImportLibrary;const std::string required=library?".shaii":".shaz";
    if(extension==".shaii"&&library){diagnose(ctx,input,s.line,".shaii files cannot import other .shaii libraries");break;}
    if(fs::path(s.value).extension()!=required){diagnose(ctx,input,s.line,std::string("$")+(library?"shaiiport":"shazport")+" requires a "+required+" file");break;}
    execute_file(shay::resolve_import(input,s.value,library&&!s.external_import),ctx);break;
   }
   case shay::StatementKind::InputVariable:{
    if(extension==".shaz"){diagnose(ctx,input,s.line,"Zeccurs files are variable-free; 'inpvarforw' is not allowed");break;}
    if(!ctx.check_only){std::string entered;std::getline(std::cin,entered);ctx.values[s.name]=entered;ctx.numbers.erase(s.name);}
    else ctx.values.emplace(s.name,std::string());
    break;
   }
   case shay::StatementKind::Declare:{
    if(extension==".shaz"){diagnose(ctx,input,s.line,"Zeccurs files are variable-free; 'brew' is not allowed");break;}
    if(!s.value.empty()&&s.value.front()==StringMarker){ctx.values[s.name]=s.value.substr(1);ctx.numbers.erase(s.name);}
    else{ctx.values[s.name]=s.value;try{std::size_t used=0;double value=std::stod(s.value,&used);if(used==s.value.size())ctx.numbers[s.name]=value;else ctx.numbers.erase(s.name);}catch(...){ctx.numbers.erase(s.name);}}
    break;
   }
   case shay::StatementKind::LogFileExtension:{
    auto value=s.dynamic_value?ctx.values.find(s.name):ctx.values.end();
    if(s.dynamic_value&&value==ctx.values.end()){diagnose(ctx,input,s.line,"unknown log extension variable '"+s.name+"'");break;}
    ctx.log_file_extension=s.dynamic_value?value->second:s.value;
    if(ctx.log_file_extension.empty()||ctx.log_file_extension[0]!='.'||ctx.log_file_extension.find_first_not_of(".abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")!=std::string::npos){diagnose(ctx,input,s.line,"invalid log file extension");break;}
    ctx.log_file_extension_set=true;break;
   }
   case shay::StatementKind::LogFileName:{
    auto value=s.dynamic_value?ctx.values.find(s.name):ctx.values.end();
    if(s.dynamic_value&&value==ctx.values.end()){diagnose(ctx,input,s.line,"unknown log filename variable '"+s.name+"'");break;}
    ctx.log_file_name=s.dynamic_value?value->second:s.value;
    if(ctx.log_file_name.empty()){diagnose(ctx,input,s.line,"log filename cannot be empty");break;}
    ctx.log_file_name_set=true;break;
   }
   case shay::StatementKind::DeclareFunction:
    if(extension!=".shaii"){diagnose(ctx,input,s.line,"'$declr' is only allowed in .shaii files");break;}
    if(ctx.functions.count(s.name))diagnose(ctx,input,s.line,"function '"+s.name+"' is already declared");
    else ctx.functions.emplace(s.name,s);
    break;
   case shay::StatementKind::DeclareClass:{
    if(extension!=".shaii"){diagnose(ctx,input,s.line,"'$classfunc' is only allowed in .shaii files");break;}
    if(ctx.classes.count(s.name)){diagnose(ctx,input,s.line,"class '"+s.name+"' is already declared");break;}
    std::vector<std::string> methods;bool valid=true;
    for(const auto&method:s.body)
     if(ctx.functions.count(method.name)){diagnose(ctx,input,method.line,"method or function '"+method.name+"' is already declared");valid=false;}
    if(valid){
     for(const auto&method:s.body){ctx.functions.emplace(method.name,method);methods.push_back(method.name);}
     ctx.classes.emplace(s.name,std::move(methods));
    }
    break;
   }
   case shay::StatementKind::DoFunction:{
    NumberScope scope=ctx.numbers;std::string failure;
    shay::Statement call;call.name=s.name;
    if(!ctx.functions.count(s.name))diagnose(ctx,input,s.line,"unknown function '"+s.name+"'");
    else if(!ctx.functions[s.name].parameters.empty())diagnose(ctx,input,s.line,"dofunc only calls functions declared without parameters");
    else if(!call_function(call,ctx,input,scope))diagnose(ctx,input,s.line,"function '"+s.name+"' failed: "+ctx.failures[s.name]);
    break;
   }
   case shay::StatementKind::ExecuteClass:{
    NumberScope scope=ctx.numbers;std::string failure;
    if(!ctx.classes.count(s.name))diagnose(ctx,input,s.line,"unknown class '"+s.name+"'");
    else if(!execute_class(s.name,ctx,input,scope,failure))diagnose(ctx,input,s.line,"class '"+s.name+"' failed: "+failure);
    break;
   }
   case shay::StatementKind::If:{
    auto validate_log_config=[&](const auto&body,const auto&self)->void{
     for(const auto&nested:body){
      if(nested.kind==shay::StatementKind::LogText&&!log_configuration_ready(ctx))
       diagnose(ctx,input,nested.line,"'logit/>' requires both '$logindxfl' and '$logindxnm' to be declared before use");
      self(nested.body,self);self(nested.else_body,self);
     }
    };
    validate_log_config(s.body,validate_log_config);
    validate_log_config(s.else_body,validate_log_config);
    bool condition=false;std::string error;
    if(!evaluate_condition(s.condition,ctx.numbers,ctx,condition,error)){diagnose(ctx,input,s.line,"invalid ifit condition: "+error);break;}
    {auto scope=ctx.numbers;{std::string ignored;if(condition)execute_statements(s.body,ctx,input,scope,ignored);else execute_statements(s.else_body,ctx,input,scope,ignored);}}
    break;
   }
   case shay::StatementKind::MarkFailure:
    diagnose(ctx,input,s.line,"!#"+s.name+" is only valid inside its matching function");
    break;
   case shay::StatementKind::ReturnValue:
    diagnose(ctx,input,s.line,"'forw' is only valid inside a function");
    break;
   case shay::StatementKind::OutputText:
    if(!ctx.check_only)shay_runtime_write((s.value+"\n").c_str());
    break;
   case shay::StatementKind::LogText:
    if(!log_configuration_ready(ctx))diagnose(ctx,input,s.line,"'logit/>' requires both '$logindxfl' and '$logindxnm' to be declared before use");
    else if(!ctx.check_only&&!append_log(ctx,s.value))diagnose(ctx,input,s.line,"cannot write log file '"+configured_log_path(ctx).string()+"'");
    break;
   case shay::StatementKind::OutputTextExit:
    if(!ctx.check_only)shay_runtime_write((s.value+"\n").c_str());
    ctx.exit_requested=true;
    break;
   case shay::StatementKind::OutputValue:{
    for(const auto&name:s.output_names){
     auto found=ctx.values.find(name);
     if(found==ctx.values.end())diagnose(ctx,input,s.line,"unknown variable '"+name+"'");
     else if(!ctx.check_only)shay_runtime_write((found->second+"\n").c_str());
    }
    break;
   }
   case shay::StatementKind::OutputCall:{
    auto scope=ctx.numbers;call_function(s,ctx,input,scope);break;
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
 if(context.exit_requested)return 1;
 for(const auto&failure:context.failures)if(!context.handled_failures.count(failure.first)){std::cerr<<"shaii: function '"<<failure.first<<"' failed: "<<failure.second<<'\n';return 1;}
 if(!context.diagnostics.empty()){for(const auto&d:context.diagnostics)std::cerr<<d.file.string()<<':'<<d.line<<": error: "<<d.message<<'\n';return 1;}
 if(check)std::cout<<"shaii: "<<source.string()<<": no errors\n";
 return 0;
}
