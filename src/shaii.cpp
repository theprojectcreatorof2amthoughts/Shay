#include "ast.hpp"
#include "expression.hpp"
#include "imports.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "shui_source.hpp"
#include "shaye.hpp"
#include "pyinit_runtime.hpp"
#include "runtime.h"
#include <algorithm>
#include <cerrno>
#include <cstdint>
#ifdef _WIN32
#include <process.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
namespace fs=std::filesystem;
namespace {
constexpr char StringMarker='\x01';
struct Diagnostic{fs::path file;std::size_t line;std::string message;};
struct Context{
 std::unordered_map<std::string,std::string> values;
 std::unordered_map<std::string,double> numbers;
 std::unordered_map<std::string,std::vector<std::string>> lists;
 std::unordered_map<std::string,std::vector<std::pair<std::string,std::string>>> maps;
 std::unordered_map<std::string,std::vector<std::string>> sets;
 std::unordered_map<std::string,shay::Statement> functions;
 std::unordered_map<std::string,std::vector<std::string>> classes;
 std::unordered_set<std::string> loaded,active;
 fs::path py_runtime_dir;
 std::unique_ptr<shay::PythonSession> python_session;
 std::vector<Diagnostic> diagnostics;
 bool check_only=false;
 std::unordered_map<std::string,std::string> failures;
 std::unordered_set<std::string> handled_failures;
 std::vector<std::string> active_functions;
 bool exit_requested=false;
 std::string error_category,error_message;
 std::size_t error_line=0;
 std::string log_file_name="shay";
 std::string log_file_extension=".log";
 bool log_file_name_set=false;
 bool log_file_extension_set=false;
 ~Context() {
  python_session.reset();
  if (!py_runtime_dir.empty()) {
   std::error_code ec;
   fs::remove_all(py_runtime_dir, ec);
  }
 }
};
using NumberScope=std::unordered_map<std::string,double>;
int run_process(const std::string&program,const std::vector<std::string>&arguments){
 std::vector<std::string> storage;storage.reserve(arguments.size()+1);storage.push_back(program);
 storage.insert(storage.end(),arguments.begin(),arguments.end());
 std::vector<char*> argv;argv.reserve(storage.size()+1);
 for(auto&arg:storage)argv.push_back(const_cast<char*>(arg.c_str()));
 argv.push_back(nullptr);
#ifdef _WIN32
 const std::intptr_t result=_spawnvp(_P_WAIT,program.c_str(),argv.data());
 return result<0?127:static_cast<int>(result);
#else
 const pid_t child=fork();
 if(child<0)return 127;
 if(child==0){execvp(program.c_str(),argv.data());_exit(127);}
 int status=0;
 while(waitpid(child,&status,0)<0){if(errno!=EINTR)return 127;}
 if(WIFEXITED(status))return WEXITSTATUS(status);
 if(WIFSIGNALED(status))return 128+WTERMSIG(status);
 return 127;
#endif
}
struct ReturnFlow{bool returned=false;double value=0;};
bool execute_python_block(const shay::Statement& statement, Context& ctx, std::string& failure);
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
     auto map=ctx.maps.find(name);
     if(map!=ctx.maps.end()){
      if(s.output_keyed){auto entry=std::find_if(map->second.begin(),map->second.end(),[&](const auto&item){return item.first==s.output_key;});if(entry==map->second.end()){failure="unknown map key '"+s.output_key+"'";success=false;break;}if(!ctx.check_only)shay_runtime_write((entry->second+"\n").c_str());}
      else if(!ctx.check_only)for(const auto&entry:map->second)shay_runtime_write((entry.first+" = "+entry.second+"\n").c_str());
      continue;
     }
          if(s.output_keyed){failure="unknown map '"+name+"'";success=false;break;}
auto set=ctx.sets.find(name);
     if(set!=ctx.sets.end()){if(!ctx.check_only)for(const auto&item:set->second)shay_runtime_write((item+"\n").c_str());continue;}
     auto list=ctx.lists.find(name);
     if(list!=ctx.lists.end()){
      if(s.output_indexed){
       if(s.list_index>=list->second.size()){failure="list index "+std::to_string(s.list_index)+" is out of range for '"+name+"'";success=false;break;}
       if(!ctx.check_only)shay_runtime_write((list->second[s.list_index]+"\n").c_str());
      }else if(!ctx.check_only)for(const auto&item:list->second)shay_runtime_write((item+"\n").c_str());
      continue;
     }
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
   case shay::StatementKind::ListAppend:
   case shay::StatementKind::ListSet:
   case shay::StatementKind::ListRemove:{
    auto list=ctx.lists.find(s.name);
    if(list==ctx.lists.end()){failure="unknown list '"+s.name+"'";success=false;break;}
    if(s.kind==shay::StatementKind::ListAppend){if(!ctx.check_only)list->second.push_back(s.value);}
    else if(s.list_index>=list->second.size()){failure="list index "+std::to_string(s.list_index)+" is out of range for '"+s.name+"'";success=false;}
    else if(!ctx.check_only&&s.kind==shay::StatementKind::ListSet)list->second[s.list_index]=s.value;
    else if(!ctx.check_only&&s.kind==shay::StatementKind::ListRemove)list->second.erase(list->second.begin()+static_cast<std::ptrdiff_t>(s.list_index));
    break;
   }
   case shay::StatementKind::MapSet:
   case shay::StatementKind::MapRemove:
   case shay::StatementKind::SetAdd:
   case shay::StatementKind::SetRemove:
   case shay::StatementKind::SetContains:{
    if(s.kind==shay::StatementKind::MapSet||s.kind==shay::StatementKind::MapRemove){
     auto map=ctx.maps.find(s.name);if(map==ctx.maps.end()){failure="unknown map '"+s.name+"'";success=false;break;}
     auto entry=std::find_if(map->second.begin(),map->second.end(),[&](const auto&item){return item.first==s.output_key;});
     if(s.kind==shay::StatementKind::MapSet){if(!ctx.check_only){if(entry==map->second.end())map->second.push_back({s.output_key,s.value});else entry->second=s.value;}}
     else if(entry==map->second.end()){failure="unknown map key '"+s.output_key+"'";success=false;}
     else if(!ctx.check_only)map->second.erase(entry);
    }else{
     auto set=ctx.sets.find(s.name);if(set==ctx.sets.end()){failure="unknown set '"+s.name+"'";success=false;break;}
     auto item=std::find(set->second.begin(),set->second.end(),s.value);
     if(s.kind==shay::StatementKind::SetAdd){if(!ctx.check_only&&item==set->second.end())set->second.push_back(s.value);}
     else if(s.kind==shay::StatementKind::SetRemove){if(item==set->second.end()){failure="unknown set item '"+s.value+"'";success=false;}else if(!ctx.check_only)set->second.erase(item);}
     else if(!ctx.check_only)shay_runtime_write((item==set->second.end()?"false\n":"true\n"));
    }
    break;
   }
   case shay::StatementKind::ProcessRun:{
    if(!ctx.check_only){const int exit_code=run_process(s.path,s.arguments);shay_runtime_write((std::to_string(exit_code)+"\n").c_str());}
    break;
   }
   case shay::StatementKind::PythonBlock:{
    std::string pythonFailure;
    if (!execute_python_block(s, ctx, pythonFailure)) {
     failure = pythonFailure;
     success = false;
    }
    break;
   }
   case shay::StatementKind::FileRead:{
    std::ifstream input(s.path,std::ios::binary);
    if(!input){ctx.error_category="io";ctx.error_message="cannot read file '"+s.path+"'";ctx.error_line=s.line;failure=ctx.error_category+": "+ctx.error_message;success=false;break;}
    std::ostringstream contents;contents<<input.rdbuf();
    ctx.values[s.name]=contents.str();ctx.numbers.erase(s.name);ctx.lists.erase(s.name);ctx.maps.erase(s.name);ctx.sets.erase(s.name);
    break;
   }
   case shay::StatementKind::FileWrite:
   case shay::StatementKind::FileAppend:{
    const auto mode=std::ios::binary|(s.kind==shay::StatementKind::FileAppend?std::ios::app:std::ios::trunc);
    std::ofstream output(s.path,mode);
    if(!output){ctx.error_category="io";ctx.error_message="cannot write file '"+s.path+"'";ctx.error_line=s.line;failure=ctx.error_category+": "+ctx.error_message;success=false;break;}
    output<<s.value;
    if(!output){ctx.error_category="io";ctx.error_message="failed while writing file '"+s.path+"'";ctx.error_line=s.line;failure=ctx.error_category+": "+ctx.error_message;success=false;}
    break;
   }
   case shay::StatementKind::FileExists:
    if(!ctx.check_only)shay_runtime_write(fs::exists(s.path)?"true\n":"false\n");
    break;
   case shay::StatementKind::FileDelete:{
    std::error_code ec;const bool removed=fs::remove(s.path,ec);
    if(ec||!removed){ctx.error_category="io";ctx.error_message="cannot delete file '"+s.path+"'";ctx.error_line=s.line;failure=ctx.error_category+": "+ctx.error_message;success=false;}
    break;
   }
   case shay::StatementKind::DirectoryCreate:{
    std::error_code ec;fs::create_directories(s.path,ec);
    if(ec){ctx.error_category="io";ctx.error_message="cannot create directory '"+s.path+"'";ctx.error_line=s.line;failure=ctx.error_category+": "+ctx.error_message;success=false;}
    else if(!ctx.check_only)shay_runtime_write(fs::is_directory(s.path)?"true\n":"false\n");
    break;
   }
   case shay::StatementKind::DirectoryExists:
    if(!ctx.check_only)shay_runtime_write(fs::is_directory(s.path)?"true\n":"false\n");
    break;
   case shay::StatementKind::ThrowError:
    ctx.error_category=s.error_category;ctx.error_message=s.value;ctx.error_line=s.line;
    failure=s.error_category+": "+s.value;success=false;break;
   case shay::StatementKind::TryCatch:{
    std::string nestedFailure;
    const bool nestedOk=execute_statements(s.body,ctx,file,scope,nestedFailure,return_flow);
    if(!nestedOk){
     if(ctx.exit_requested||(return_flow&&return_flow->returned)){failure=nestedFailure;success=false;break;}
     if(ctx.error_category.empty()){ctx.error_category="runtime";ctx.error_message=nestedFailure;ctx.error_line=s.line;}
     ctx.values[s.name]=ctx.error_category+": "+ctx.error_message;
     ctx.numbers.erase(s.name);ctx.lists.erase(s.name);ctx.maps.erase(s.name);ctx.sets.erase(s.name);
     ctx.error_category.clear();ctx.error_message.clear();ctx.error_line=0;
     std::string catchFailure;
     if(!execute_statements(s.else_body,ctx,file,scope,catchFailure,return_flow)){failure=catchFailure;success=false;}
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
std::string pyinit_hex_encode(const std::string& value) {
 static constexpr char digits[] = "0123456789abcdef";
 std::string encoded;
 encoded.reserve(value.size() * 2);
 for (unsigned char byte : value) {
  encoded.push_back(digits[byte >> 4]);
  encoded.push_back(digits[byte & 0x0f]);
 }
 return encoded;
}
bool pyinit_hex_decode(const std::string& value, std::string& decoded) {
 if (value.size() % 2) return false;
 auto digit = [](char ch) -> int {
  if (ch >= '0' && ch <= '9') return ch - '0';
  if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
  if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
  return -1;
 };
 decoded.clear();
 decoded.reserve(value.size() / 2);
 for (std::size_t i = 0; i < value.size(); i += 2) {
  const int high = digit(value[i]), low = digit(value[i + 1]);
  if (high < 0 || low < 0) return false;
  decoded.push_back(static_cast<char>((high << 4) | low));
 }
 return true;
}
bool execute_python_block(const shay::Statement& statement, Context& ctx,
                          std::string& failure) {
 if (ctx.check_only) return true;
 if (ctx.py_runtime_dir.empty()) {
  failure = "Python runtime is not available; install the pyinit.shpak entry package";
  return false;
 }
 std::string code;
 if (!pyinit_hex_decode(statement.python_code, code)) {
  failure = "invalid encoded Python block";
  return false;
 }
 if (!ctx.python_session) ctx.python_session = std::make_unique<shay::PythonSession>();
 std::string error;
 if (!ctx.python_session->start(ctx.py_runtime_dir, error)) {
  failure = error;
  return false;
 }
 if (!ctx.python_session->execute(code, error,
  [](const std::string& prompt) {
   if (!prompt.empty()) shay_runtime_write(prompt.c_str());
   std::string entered;
   std::getline(std::cin, entered);
   return entered;
  },
  [](const std::string& output) {
   if (!output.empty()) shay_runtime_write(output.c_str());
  })) {
  failure = error.empty() ? "Python block failed" : error;
  return false;
 }
 return true;
}
bool prepare_python_blocks(const std::string& source, std::string& prepared,
                           const fs::path& file, Context& ctx) {
 std::istringstream input(source);
 std::ostringstream output;
 std::string line;
 std::size_t line_number = 0;
 bool pyinit_enabled = false;
 bool valid = true;
 while (std::getline(input, line)) {
  ++line_number;
  std::string current = line;
  const auto first = current.find_first_not_of(" \t\r");
  if (first == std::string::npos) current.clear();
  else {
   const auto last = current.find_last_not_of(" \t\r");
   current = current.substr(first, last - first + 1);
  }
  if (current == "$useentry pyinit") {
   pyinit_enabled = true;
   output << line << '\n';
   continue;
  }
  if (current != "$initpy") {
   output << line << '\n';
   continue;
  }
  if (!pyinit_enabled) {
   diagnose(ctx, file, line_number, "'$initpy' requires '$useentry pyinit' before the block");
   valid = false;
   output << line << '\n';
   continue;
  }
  std::string code;
  std::vector<std::string> consumed;
  bool closed = false;
  while (std::getline(input, line)) {
   ++line_number;
   consumed.push_back(line);
   std::string ending = line;
   const auto begin = ending.find_first_not_of(" \t\r");
   if (begin == std::string::npos) ending.clear();
   else {
    const auto end = ending.find_last_not_of(" \t\r");
    ending = ending.substr(begin, end - begin + 1);
   }
   if (ending == "$endpy") { closed = true; break; }
   code += line;
   code.push_back('\n');
  }
  if (!closed) {
   diagnose(ctx, file, line_number, "expected '$endpy' to close the Python block");
   valid = false;
   output << line << '\n';
   break;
  }
  output << "pyblock/> \"" << pyinit_hex_encode(code) << "\"\n";
  for (std::size_t i = 0; i < consumed.size(); ++i) output << '\n';
 }
 prepared = output.str();
 return valid;
}
void execute_file(const fs::path&input,Context&ctx){
 std::error_code ec;const fs::path canonical=fs::weakly_canonical(input,ec);const std::string key=(ec?input:canonical).string();
 if(ctx.active.count(key)){diagnose(ctx,input,1,"circular import detected");return;}
 if(ctx.loaded.count(key))return;
 const std::string file_extension=input.extension().string();
 std::string extension=file_extension;
 if(extension==".shaye")extension=".shaii";else if(extension==".shpak")extension=".shay";
 if(extension!=".shay"&&extension!=".shaz"&&extension!=".shaii"&&extension!=".shui"){diagnose(ctx,input,1,"unsupported file extension (expected .shay, .shaz, .shaii, .shui, or .shpak)");return;}
 std::string raw_source;
 if(file_extension==".shaye"||file_extension==".shpak"){
  shay::ShayeRecords package_records;
  if(!shay::read_shaye_records(input,package_records)){diagnose(ctx,input,1,"invalid binary entry package (bad magic, version, or bytecode)");return;}
  raw_source=std::move(package_records.source);
  if(file_extension==".shpak"&&!package_records.resources.empty()){
   fs::path extracted;
   if(!shay::extract_shaye_resources(input,package_records.resources,extracted)){
    diagnose(ctx,input,1,"cannot safely extract embedded package resources");return;
   }
   if(input.stem()=="pyinit")ctx.py_runtime_dir=std::move(extracted);
  }
 }else{
  std::ifstream stream(input,std::ios::binary);if(!stream){diagnose(ctx,input,1,"cannot open source file");return;}
  raw_source.assign(std::istreambuf_iterator<char>(stream),{});
 }
 std::string source=raw_source;
 shay::discover_shaye_entries(input);
 if (!prepare_python_blocks(raw_source, source, input, ctx)) return;
 if(extension==".shui"){
  std::vector<shay::ui::language::SourceDiagnostic> ui_diagnostics;
  if(!shay::ui::language::prepare_shui_source(input,raw_source,source,ui_diagnostics)){
   for(const auto&item:ui_diagnostics)diagnose(ctx,input,item.line,item.message);
   return;
  }
 }
 shay::Lexer lexer(source);const auto tokens=lexer.scan();shay::Parser parser(tokens);const shay::Program program=parser.parse();
 for(const auto&e:lexer.errors())diagnose(ctx,input,e.line,e.message);
 for(const auto&e:program.errors)diagnose(ctx,input,e.line,e.message);
 ctx.active.insert(key);
 for(const auto&s:program.statements){
  if(ctx.exit_requested)break;
  switch(s.kind){
   case shay::StatementKind::EntrySyntaxMetadata:break;
   case shay::StatementKind::UseEntry:{
    const auto entries=shay::discover_shaye_entries(input);
    const auto entry=entries.find(s.name);
    if(entry!=entries.end())execute_file(entry->second,ctx);
    break;
   }
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
    ctx.lists.erase(s.name);ctx.maps.erase(s.name);ctx.sets.erase(s.name);
    if(s.map_value){ctx.values.erase(s.name);ctx.numbers.erase(s.name);ctx.maps[s.name]=s.map_items;}
    else if(s.set_value){ctx.values.erase(s.name);ctx.numbers.erase(s.name);std::vector<std::string> unique;for(const auto&item:s.list_items)if(std::find(unique.begin(),unique.end(),item)==unique.end())unique.push_back(item);ctx.sets[s.name]=std::move(unique);}
    else if(s.list_value){ctx.values.erase(s.name);ctx.numbers.erase(s.name);ctx.lists[s.name]=s.list_items;}
    else if(!s.value.empty()&&s.value.front()==StringMarker){ctx.values[s.name]=s.value.substr(1);ctx.numbers.erase(s.name);}
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
    if(extension!=".shaii"&&file_extension!=".shpak"&&!s.entry_syntax_declaration){diagnose(ctx,input,s.line,"'$declr' is only allowed in .shaii files; entry-defined function syntax requires its $useentry");break;}
    if(ctx.functions.count(s.name))diagnose(ctx,input,s.line,"function '"+s.name+"' is already declared");
    else ctx.functions.emplace(s.name,s);
    break;
   case shay::StatementKind::DeclareClass:{
    if(extension!=".shaii"&&extension!=".shui"&&file_extension!=".shpak"){diagnose(ctx,input,s.line,"class declarations are only allowed in .shaii or .shui files");break;}
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
     auto map=ctx.maps.find(name);
     if(map!=ctx.maps.end()){
      if(s.output_keyed){auto entry=std::find_if(map->second.begin(),map->second.end(),[&](const auto&item){return item.first==s.output_key;});if(entry==map->second.end()){diagnose(ctx,input,s.line,"unknown map key '"+s.output_key+"'");break;}if(!ctx.check_only)shay_runtime_write((entry->second+"\n").c_str());}
      else if(!ctx.check_only)for(const auto&entry:map->second)shay_runtime_write((entry.first+" = "+entry.second+"\n").c_str());
      continue;
     }
          if(s.output_keyed){diagnose(ctx,input,s.line,"unknown map '"+name+"'");break;}
auto set=ctx.sets.find(name);
     if(set!=ctx.sets.end()){if(!ctx.check_only)for(const auto&item:set->second)shay_runtime_write((item+"\n").c_str());continue;}
     auto list=ctx.lists.find(name);
     if(list!=ctx.lists.end()){
      if(s.output_indexed){
       if(s.list_index>=list->second.size()){diagnose(ctx,input,s.line,"list index "+std::to_string(s.list_index)+" is out of range for '"+name+"'");break;}
       if(!ctx.check_only)shay_runtime_write((list->second[s.list_index]+"\n").c_str());
      }else if(!ctx.check_only)for(const auto&item:list->second)shay_runtime_write((item+"\n").c_str());
      continue;
     }
     auto found=ctx.values.find(name);
     if(found==ctx.values.end())diagnose(ctx,input,s.line,"unknown variable '"+name+"'");
     else if(!ctx.check_only)shay_runtime_write((found->second+"\n").c_str());
    }
    break;
   }
   case shay::StatementKind::OutputCall:{
    auto scope=ctx.numbers;call_function(s,ctx,input,scope);break;
   }
   case shay::StatementKind::ListAppend:
   case shay::StatementKind::ListSet:
   case shay::StatementKind::ListRemove:{
    auto list=ctx.lists.find(s.name);
    if(list==ctx.lists.end()){diagnose(ctx,input,s.line,"unknown list '"+s.name+"'");break;}
    if(s.kind==shay::StatementKind::ListAppend){if(!ctx.check_only)list->second.push_back(s.value);}
    else if(s.list_index>=list->second.size())diagnose(ctx,input,s.line,"list index "+std::to_string(s.list_index)+" is out of range for '"+s.name+"'");
    else if(!ctx.check_only&&s.kind==shay::StatementKind::ListSet)list->second[s.list_index]=s.value;
    else if(!ctx.check_only&&s.kind==shay::StatementKind::ListRemove)list->second.erase(list->second.begin()+static_cast<std::ptrdiff_t>(s.list_index));
    break;
   }
   case shay::StatementKind::MapSet:
   case shay::StatementKind::MapRemove:
   case shay::StatementKind::SetAdd:
   case shay::StatementKind::SetRemove:
   case shay::StatementKind::SetContains:{
    if(s.kind==shay::StatementKind::MapSet||s.kind==shay::StatementKind::MapRemove){
     auto map=ctx.maps.find(s.name);
     if(map==ctx.maps.end()){diagnose(ctx,input,s.line,"unknown map '"+s.name+"'");break;}
     auto entry=std::find_if(map->second.begin(),map->second.end(),[&](const auto&item){return item.first==s.output_key;});
     if(s.kind==shay::StatementKind::MapSet){if(!ctx.check_only){if(entry==map->second.end())map->second.push_back({s.output_key,s.value});else entry->second=s.value;}}
     else if(entry==map->second.end())diagnose(ctx,input,s.line,"unknown map key '"+s.output_key+"'");
     else if(!ctx.check_only)map->second.erase(entry);
    }else{
     auto set=ctx.sets.find(s.name);
     if(set==ctx.sets.end()){diagnose(ctx,input,s.line,"unknown set '"+s.name+"'");break;}
     auto item=std::find(set->second.begin(),set->second.end(),s.value);
     if(s.kind==shay::StatementKind::SetAdd){if(!ctx.check_only&&item==set->second.end())set->second.push_back(s.value);}
     else if(s.kind==shay::StatementKind::SetRemove){if(item==set->second.end())diagnose(ctx,input,s.line,"unknown set item '"+s.value+"'");else if(!ctx.check_only)set->second.erase(item);}
     else if(!ctx.check_only)shay_runtime_write(item==set->second.end()?"false\n":"true\n");
    }
    break;
   }
   case shay::StatementKind::ProcessRun:
    if(!ctx.check_only){const int exit_code=run_process(s.path,s.arguments);shay_runtime_write((std::to_string(exit_code)+"\n").c_str());}
    break;
   case shay::StatementKind::PythonBlock:{
    std::string pythonFailure;
    if (!execute_python_block(s, ctx, pythonFailure))
     diagnose(ctx, input, s.line, "Python: " + pythonFailure);
    break;
   }
   case shay::StatementKind::FileRead:{
    std::ifstream fileInput(s.path,std::ios::binary);
    if(!fileInput){diagnose(ctx,input,s.line,"io: cannot read file '"+s.path+"'");break;}
    std::ostringstream contents;contents<<fileInput.rdbuf();
    ctx.values[s.name]=contents.str();ctx.numbers.erase(s.name);ctx.lists.erase(s.name);ctx.maps.erase(s.name);ctx.sets.erase(s.name);
    break;
   }
   case shay::StatementKind::FileWrite:
   case shay::StatementKind::FileAppend:{
    const auto mode=std::ios::binary|(s.kind==shay::StatementKind::FileAppend?std::ios::app:std::ios::trunc);
    std::ofstream output(s.path,mode);
    if(!output){diagnose(ctx,input,s.line,"io: cannot write file '"+s.path+"'");break;}
    output<<s.value;
    if(!output)diagnose(ctx,input,s.line,"io: failed while writing file '"+s.path+"'");
    break;
   }
   case shay::StatementKind::FileExists:
    if(!ctx.check_only)shay_runtime_write(fs::exists(s.path)?"true\n":"false\n");
    break;
   case shay::StatementKind::FileDelete:{
    std::error_code ec;const bool removed=fs::remove(s.path,ec);
    if(ec||!removed)diagnose(ctx,input,s.line,"io: cannot delete file '"+s.path+"'");
    break;
   }
   case shay::StatementKind::DirectoryCreate:{
    std::error_code ec;fs::create_directories(s.path,ec);
    if(ec)diagnose(ctx,input,s.line,"io: cannot create directory '"+s.path+"'");
    else if(!ctx.check_only)shay_runtime_write(fs::is_directory(s.path)?"true\n":"false\n");
    break;
   }
   case shay::StatementKind::DirectoryExists:
    if(!ctx.check_only)shay_runtime_write(fs::is_directory(s.path)?"true\n":"false\n");
    break;
   case shay::StatementKind::ThrowError:
    diagnose(ctx,input,s.line,s.error_category+": "+s.value);break;
   case shay::StatementKind::TryCatch:{
    NumberScope scope;std::string nestedFailure;
    if(!execute_statements(s.body,ctx,input,scope,nestedFailure)){
     if(ctx.exit_requested){diagnose(ctx,input,s.line,nestedFailure);break;}
     if(ctx.error_category.empty()){ctx.error_category="runtime";ctx.error_message=nestedFailure;ctx.error_line=s.line;}
     ctx.values[s.name]=ctx.error_category+": "+ctx.error_message;
     ctx.numbers.erase(s.name);ctx.lists.erase(s.name);ctx.maps.erase(s.name);ctx.sets.erase(s.name);
     ctx.error_category.clear();ctx.error_message.clear();ctx.error_line=0;
     std::string catchFailure;
     if(!execute_statements(s.else_body,ctx,input,scope,catchFailure))diagnose(ctx,input,s.line,catchFailure);
    }
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
  else if(arg=="--help"||arg=="-h"){std::cout<<"Usage: shaii [--check] program.shay|program.shaz|program.shui|library.shaii|entry.shpak\n       shaii --version\n";return 0;}
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
