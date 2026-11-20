#include "ast.hpp"
#include "imports.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#ifdef _WIN32
#include <process.h>
#endif
#include <iostream>
#include <iterator>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
namespace fs=std::filesystem;
namespace {
constexpr char StringMarker='\x01';
struct Diagnostic{fs::path file;std::size_t line;std::string message;std::string source_line;std::size_t column=1;std::size_t length=1;bool warning=false;};
struct BuildContext{
 std::unordered_set<std::string>loaded,active,variables;
 std::vector<std::pair<fs::path,shay::Statement>>variable_declarations;
 std::unordered_map<std::string,shay::Statement>functions;
 std::unordered_map<std::string,std::vector<std::string>>classes;
 std::vector<std::pair<fs::path,shay::Statement>>pending_function_validation;
 std::vector<Diagnostic>diagnostics;
 std::unordered_map<std::string,std::vector<std::string>>source_lines;
 std::vector<shay::Statement>statements;
 std::string log_file_name="shay";
 std::string log_file_extension=".log";
 bool log_file_name_set=false;
 bool log_file_extension_set=false;
};
void diagnose(BuildContext&ctx,const fs::path&file,std::size_t line,const std::string&message,bool warning=false){
 Diagnostic diagnostic;diagnostic.file=file;diagnostic.line=line;diagnostic.message=message;diagnostic.warning=warning;
 auto source=ctx.source_lines.find(file.string());
 if(source!=ctx.source_lines.end()&&line>0&&line<=source->second.size()){
  diagnostic.source_line=source->second[line-1];
  const auto first=message.find('\'');
  if(first!=std::string::npos){
   const auto last=message.find('\'',first+1);
   if(last!=std::string::npos&&last>first+1){
    const std::string target=message.substr(first+1,last-first-1);
    const auto at=diagnostic.source_line.find(target);
    if(at!=std::string::npos){diagnostic.column=at+1;diagnostic.length=target.size();}
   }
  }
 }
 ctx.diagnostics.push_back(std::move(diagnostic));
}
std::unordered_set<std::string> expression_names(const std::string&source){
 std::unordered_set<std::string> names;const std::regex identifier(R"([A-Za-z_][A-Za-z0-9_]*)");
 for(std::sregex_iterator it(source.begin(),source.end(),identifier),end;it!=end;++it)names.insert(it->str());
 return names;
}

void collect_expression_uses(const std::string&source,const BuildContext&ctx,const std::unordered_set<std::string>&locals,std::unordered_set<std::string>&used){
 for(const auto&name:expression_names(source))if(ctx.variables.count(name)&&!locals.count(name))used.insert(name);
}
void collect_statement_uses(const shay::Statement&s,const BuildContext&ctx,const std::unordered_set<std::string>&locals,std::unordered_set<std::string>&used){
 switch(s.kind){
  case shay::StatementKind::OutputValue:
   for(const auto&name:s.output_names)
    if(ctx.variables.count(name)&&!locals.count(name))used.insert(name);
   break;
  case shay::StatementKind::OutputCall:
   for(const auto&arg:s.arguments)collect_expression_uses(arg,ctx,locals,used);
   break;
  case shay::StatementKind::ReturnValue:
   collect_expression_uses(s.value,ctx,locals,used);
   break;
  case shay::StatementKind::If:
   collect_expression_uses(s.condition,ctx,locals,used);
   for(const auto&child:s.body)collect_statement_uses(child,ctx,locals,used);
   for(const auto&child:s.else_body)collect_statement_uses(child,ctx,locals,used);
   break;
  case shay::StatementKind::LogFileExtension:
  case shay::StatementKind::LogFileName:
   if(s.dynamic_value&&ctx.variables.count(s.name)&&!locals.count(s.name))used.insert(s.name);
   break;
  case shay::StatementKind::DeclareFunction:{
   std::unordered_set<std::string>function_locals(s.parameters.begin(),s.parameters.end());
   if(s.has_return)collect_expression_uses(s.value,ctx,function_locals,used);
   for(const auto&child:s.body)collect_statement_uses(child,ctx,function_locals,used);
   break;
  }
  default:break;
 }
}
void warn_about_unused_variables(BuildContext&ctx){
 std::unordered_set<std::string>used;
 const std::unordered_set<std::string>no_locals;
 for(const auto&s:ctx.statements)collect_statement_uses(s,ctx,no_locals,used);
 for(const auto&declaration:ctx.variable_declarations)
  if(!used.count(declaration.second.name))
   diagnose(ctx,declaration.first,declaration.second.line,"variable '"+declaration.second.name+"' is never used",true);
}
void validate_expression_names(BuildContext&ctx,const fs::path&file,std::size_t line,const std::string&source,const std::unordered_set<std::string>&locals,const std::string&label){
 for(const auto&name:expression_names(source))if(!ctx.variables.count(name)&&!locals.count(name)&&name!="true"&&name!="false"&&name!="andit"&&name!="orit"&&name!="notit")
  diagnose(ctx,file,line,label+" references unknown variable '"+name+"'");
}
void validate_condition_names(BuildContext&ctx,const fs::path&file,std::size_t line,const std::string&source,const std::unordered_set<std::string>&locals,const std::string&label){
 auto trim=[](const std::string&value){const auto first=value.find_first_not_of(" \t\r\n");if(first==std::string::npos)return std::string();const auto last=value.find_last_not_of(" \t\r\n");return value.substr(first,last-first+1);};
 const std::string condition=trim(source);
 if(condition.rfind("!#",0)==0){
  const std::string name=trim(condition.substr(2));
  if(name.empty())diagnose(ctx,file,line,"expected a function name after '!#'");
  else if(!ctx.functions.count(name))diagnose(ctx,file,line,"error condition references unknown function '"+name+"'");
  return;
 }
 validate_expression_names(ctx,file,line,condition,locals,label);
}
void validate_body(BuildContext&ctx,const fs::path&file,const std::vector<shay::Statement>&body,const std::unordered_set<std::string>&locals,const std::string&bodyLabel,const std::string&function_name=""){
 for(const auto&s:body){
  if(s.kind==shay::StatementKind::If){
   validate_condition_names(ctx,file,s.line,s.condition,locals,"ifit condition");
   validate_body(ctx,file,s.body,locals,bodyLabel,function_name);
   validate_body(ctx,file,s.else_body,locals,bodyLabel,function_name);
  }else if(s.kind==shay::StatementKind::MarkFailure){
   if(function_name.empty())diagnose(ctx,file,s.line,"!#"+s.name+" is only valid inside a function");
   else if(s.name!=function_name)diagnose(ctx,file,s.line,"!#"+s.name+" can only mark the currently executing function '"+function_name+"'");
  }else if(s.kind==shay::StatementKind::OutputValue){
   for(const auto&name:s.output_names)
    if(!ctx.variables.count(name)&&!locals.count(name))
     diagnose(ctx,file,s.line,"unknown variable '"+name+"' inside "+bodyLabel);
  }else if(s.kind==shay::StatementKind::ReturnValue){
   if(function_name.empty())diagnose(ctx,file,s.line,"'forw' is only valid inside a function");
   else validate_expression_names(ctx,file,s.line,s.value,locals,"function return expression");
  }else if(s.kind==shay::StatementKind::OutputCall){
   auto fn=ctx.functions.find(s.name);
   if(fn==ctx.functions.end())diagnose(ctx,file,s.line,"unknown function '"+s.name+"' inside "+bodyLabel);
   else if(s.arguments.size()!=fn->second.parameters.size())
    diagnose(ctx,file,s.line,"function '"+s.name+"' expects "+std::to_string(fn->second.parameters.size())+" argument(s), got "+std::to_string(s.arguments.size()));
   for(const auto&arg:s.arguments)validate_expression_names(ctx,file,s.line,arg,locals,"function argument");
  }else if(s.kind==shay::StatementKind::DoFunction){
   auto fn=ctx.functions.find(s.name);
   if(fn==ctx.functions.end())diagnose(ctx,file,s.line,"unknown function '"+s.name+"' inside "+bodyLabel);
   else if(!fn->second.parameters.empty())diagnose(ctx,file,s.line,"dofunc only calls functions declared without parameters");
  }else if(s.kind==shay::StatementKind::ExecuteClass){
   if(!ctx.classes.count(s.name))diagnose(ctx,file,s.line,"unknown class '"+s.name+"' inside "+bodyLabel);
  }else if(s.kind==shay::StatementKind::LogText&&(!ctx.log_file_name_set&&!ctx.log_file_extension_set)){
   diagnose(ctx,file,s.line,"'logit/>' requires '$logindxfl' or '$logindxnm' to be configured before use");
  }else if(s.kind!=shay::StatementKind::OutputText&&s.kind!=shay::StatementKind::OutputTextExit&&s.kind!=shay::StatementKind::LogText){
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
 {
  std::istringstream lines(source);std::string text;
  auto&stored=ctx.source_lines[input.string()];
  while(std::getline(lines,text))stored.push_back(text);
 }
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
  if(s.kind==shay::StatementKind::LogFileExtension){if(!s.dynamic_value)ctx.log_file_extension=s.value;ctx.log_file_extension_set=true;ctx.statements.push_back(s);continue;}
  if(s.kind==shay::StatementKind::LogFileName){if(!s.dynamic_value)ctx.log_file_name=s.value;ctx.log_file_name_set=true;ctx.statements.push_back(s);continue;}
  if(s.kind==shay::StatementKind::Declare){
   if(extension==".shaz"){diagnose(ctx,input,s.line,"Zeccurs files are variable-free; 'brew' is not allowed");continue;}
   if(!ctx.variables.insert(s.name).second){diagnose(ctx,input,s.line,"variable '"+s.name+"' is already declared");continue;}
   ctx.variable_declarations.emplace_back(input,s);
  }else if(s.kind==shay::StatementKind::InputVariable){
   if(extension==".shaz"){diagnose(ctx,input,s.line,"Zeccurs files are variable-free; 'inpvarforw' is not allowed");continue;}
   // First use declares the variable; later uses intentionally reuse it.
   if(ctx.variables.insert(s.name).second)ctx.variable_declarations.emplace_back(input,s);
  }else if(s.kind==shay::StatementKind::DeclareFunction){
   if(extension!=".shaii"){diagnose(ctx,input,s.line,"'$declr' is only allowed in .shaii files");continue;}
   if(ctx.functions.count(s.name)){diagnose(ctx,input,s.line,"function '"+s.name+"' is already declared");continue;}
   ctx.functions.emplace(s.name,s);
   ctx.pending_function_validation.emplace_back(input,s);
  }else if(s.kind==shay::StatementKind::DeclareClass){
   if(extension!=".shaii"){diagnose(ctx,input,s.line,"'$classfunc' is only allowed in .shaii files");continue;}
   if(ctx.classes.count(s.name)){diagnose(ctx,input,s.line,"class '"+s.name+"' is already declared");continue;}
   std::vector<std::string> methods;bool valid=true;
   for(const auto&method:s.body)
    if(ctx.functions.count(method.name)){diagnose(ctx,input,method.line,"method or function '"+method.name+"' is already declared");valid=false;}
   if(valid){
    for(const auto&method:s.body){
     shay::Statement function=method;function.kind=shay::StatementKind::DeclareFunction;
     ctx.functions.emplace(function.name,function);
     ctx.pending_function_validation.emplace_back(input,function);
     ctx.statements.push_back(function);
     methods.push_back(function.name);
    }
    ctx.classes.emplace(s.name,std::move(methods));
   }
  }else if(s.kind==shay::StatementKind::DoFunction){
   if(extension==".shaz")diagnose(ctx,input,s.line,"'dofunc' is not allowed in .shaz files");
   else{
    auto fn=ctx.functions.find(s.name);
    if(fn==ctx.functions.end())diagnose(ctx,input,s.line,"unknown function '"+s.name+"'");
    else if(!fn->second.parameters.empty())diagnose(ctx,input,s.line,"dofunc only calls functions declared without parameters");
   }
  }else if(s.kind==shay::StatementKind::ExecuteClass){
   if(extension==".shaz")diagnose(ctx,input,s.line,"'$doclass' is not allowed in .shaz files");
   else if(!ctx.classes.count(s.name))diagnose(ctx,input,s.line,"unknown class '"+s.name+"'");
  }else if(s.kind==shay::StatementKind::If){
   validate_condition_names(ctx,input,s.line,s.condition,{},"ifit condition");
   validate_body(ctx,input,s.body,{},"ifit block");
   validate_body(ctx,input,s.else_body,{},"elseit block");
  }else if(s.kind==shay::StatementKind::OutputValue){
   for(const auto&name:s.output_names)
    if(!ctx.variables.count(name))diagnose(ctx,input,s.line,"unknown variable '"+name+"'");
  }else if(s.kind==shay::StatementKind::OutputCall){
   auto fn=ctx.functions.find(s.name);
   if(fn==ctx.functions.end())diagnose(ctx,input,s.line,"unknown function '"+s.name+"'");
   else if(s.arguments.size()!=fn->second.parameters.size())
    diagnose(ctx,input,s.line,"function '"+s.name+"' expects "+std::to_string(fn->second.parameters.size())+" argument(s), got "+std::to_string(s.arguments.size()));
   for(const auto&arg:s.arguments)validate_expression_names(ctx,input,s.line,arg,{},"function argument");
  }else if(s.kind==shay::StatementKind::MarkFailure){
   diagnose(ctx,input,s.line,"!#"+s.name+" is only valid inside a function");
  }else if(s.kind==shay::StatementKind::ReturnValue){
   diagnose(ctx,input,s.line,"'forw' is only valid inside a function");
  }
  if(s.kind==shay::StatementKind::LogText&&(!ctx.log_file_name_set&&!ctx.log_file_extension_set))
   diagnose(ctx,input,s.line,"'logit/>' requires '$logindxfl' or '$logindxnm' to be configured before use");
  ctx.statements.push_back(s);
 }
 ctx.active.erase(key);ctx.loaded.insert(key);
}
std::string object_entry_symbol(const fs::path&object){
 std::string symbol="shay_program_entry_";
 for(unsigned char c:object.stem().string())symbol+=std::isalnum(c)?static_cast<char>(c):'_';
 return symbol;
}
std::string cpp_string(const std::string&value){
 std::ostringstream out;out<<'"';for(unsigned char c:value){switch(c){case '\\':out<<"\\\\";break;case '"':out<<"\\\"";break;case '\n':out<<"\\n";break;case '\r':out<<"\\r";break;case '\t':out<<"\\t";break;default:if(c<32||c>=127){const char octal[]="01234567";out<<"\\"<<octal[(c>>6)&7]<<octal[(c>>3)&7]<<octal[c&7];}else out<<static_cast<char>(c);}}out<<'"';return out.str();
}
class ExpressionEmitter{
 std::string source;std::size_t at=0;
 void skip(){while(at<source.size()&&std::isspace(static_cast<unsigned char>(source[at])))++at;}
 bool take(char c){skip();if(at<source.size()&&source[at]==c){++at;return true;}return false;}
 std::string factor(){
  skip();
  if(take('+'))return "(+"+factor()+")";
  if(take('-'))return "(-"+factor()+")";
  if(take('(')){auto value=expression();if(!take(')'))throw std::runtime_error("missing ')' in generated expression");return "("+value+")";}
  skip();const std::size_t start=at;
  if(at<source.size()&&(std::isdigit(static_cast<unsigned char>(source[at]))||source[at]=='.')){
   while(at<source.size()&&(std::isdigit(static_cast<unsigned char>(source[at]))||source[at]=='.'))++at;
   return source.substr(start,at-start);
  }
  if(at<source.size()&&(std::isalpha(static_cast<unsigned char>(source[at]))||source[at]=='_')){
   while(at<source.size()&&(std::isalnum(static_cast<unsigned char>(source[at]))||source[at]=='_'))++at;
   return "shay_number(values,"+cpp_string(source.substr(start,at-start))+")";
  }
  throw std::runtime_error("invalid generated numeric expression");
 }
 std::string term(){
  std::string value=factor();
  for(;;){if(take('*'))value="("+value+" * "+factor()+")";else if(take('/'))value="shay_div("+value+", "+factor()+")";else break;}
  return value;
 }
 std::string expression(){
  std::string value=term();
  for(;;){if(take('+'))value="("+value+" + "+term()+")";else if(take('-'))value="("+value+" - "+term()+")";else break;}
  return value;
 }
public:
 explicit ExpressionEmitter(const std::string&text):source(text){}
 std::string emit(){auto result=expression();skip();if(at!=source.size())throw std::runtime_error("unexpected generated expression token");return result;}
};
std::string cpp_expression(const std::string&source){try{return ExpressionEmitter(source).emit();}catch(...){return "0";}}
std::string cpp_condition(const std::string&raw){
 auto trim=[](const std::string&s){
  std::size_t first=0,last=s.size();
  while(first<last&&std::isspace(static_cast<unsigned char>(s[first])))++first;
  while(last>first&&std::isspace(static_cast<unsigned char>(s[last-1])))--last;
  return s.substr(first,last-first);
 };
 std::string condition=trim(raw);
 if(condition.rfind("!#",0)==0)return "shay_failed("+cpp_string(trim(condition.substr(2)))+")";
 auto wraps=[](const std::string&s){
  if(s.size()<2||s.front()!='('||s.back()!=')')return false;
  int depth=0;
  for(std::size_t i=0;i<s.size();++i){
   if(s[i]=='(')++depth;else if(s[i]==')')--depth;
   if(depth==0&&i+1<s.size())return false;
   if(depth<0)return false;
  }
  return depth==0;
 };
 while(wraps(condition))condition=trim(condition.substr(1,condition.size()-2));
 auto split=[&](const std::string&word){
  int depth=0;
  for(std::size_t i=0;i+word.size()<=condition.size();++i){
   if(condition[i]=='('){++depth;continue;}
   if(condition[i]==')'){--depth;continue;}
   if(depth==0&&condition.compare(i,word.size(),word)==0){
    const bool left=i==0||std::isspace(static_cast<unsigned char>(condition[i-1]));
    const auto end=i+word.size();
    const bool right=end==condition.size()||std::isspace(static_cast<unsigned char>(condition[end]));
    if(left&&right)return i;
   }
  }
  return std::string::npos;
 };
 auto pos=split("orit");
 if(pos!=std::string::npos)return "("+cpp_condition(condition.substr(0,pos))+" || "+cpp_condition(condition.substr(pos+4))+")";
 pos=split("andit");
 if(pos!=std::string::npos)return "("+cpp_condition(condition.substr(0,pos))+" && "+cpp_condition(condition.substr(pos+5))+")";
 if(condition.size()>=5&&condition.compare(0,5,"notit")==0&&
    (condition.size()==5||std::isspace(static_cast<unsigned char>(condition[5]))))
  return "!("+cpp_condition(condition.substr(5))+")";
 const std::string operators[]={">=","<=","==","!=",">","<","="};
 for(const auto&op:operators){
  int depth=0;
  for(std::size_t i=0;i+op.size()<=condition.size();++i){
   if(condition[i]=='('){++depth;continue;}if(condition[i]==')'){--depth;continue;}
   if(depth==0&&condition.compare(i,op.size(),op)==0){
    const std::string cpp_op=op=="="?"==":op;
    return "("+cpp_expression(condition.substr(0,i))+" "+cpp_op+" "+cpp_expression(condition.substr(i+op.size()))+")";
   }
  }
 }
 return cpp_expression(condition);
}
#ifndef _WIN32
std::string shell_quote(const std::string&value){
 std::string out="'";for(char c:value){if(c=='\'')out+="'\\''";else out+=c;}return out+"'";
}
#endif
void emit_body(std::ostream&out,const std::vector<shay::Statement>&body,const BuildContext&ctx,const std::string&indent,const std::string&function_name=""){
 for(const auto&s:body){
  switch(s.kind){
   case shay::StatementKind::MarkFailure:
    if(!function_name.empty()){
     out<<indent<<"shay_failures["<<cpp_string(function_name)<<"]="<<cpp_string("function explicitly marked as failed")<<"; shay_handled_failures.erase("<<cpp_string(function_name)<<");\n";
    }
    break;
   case shay::StatementKind::ReturnValue:
    out<<indent<<"return ("<<cpp_expression(s.value)<<");\n";break;
   case shay::StatementKind::OutputText:
    out<<indent<<"shay_runtime_write("<<cpp_string(s.value+"\n")<<");\n";break;
   case shay::StatementKind::LogText:
    out<<indent<<"shay_log("<<cpp_string(s.value)<<");\n";break;
   case shay::StatementKind::OutputTextExit:
    out<<indent<<"shay_runtime_write("<<cpp_string(s.value+"\n")<<"); throw ShayExit{};\n";break;
   case shay::StatementKind::OutputValue:{
    const auto&names=s.output_names.empty()?std::vector<std::string>{s.name}:s.output_names;
    for(const auto&name:names)
     out<<indent<<"shay_runtime_write((values["<<cpp_string(name)<<"] + "<<cpp_string("\n")<<").c_str());\n";
    break;
   }
   case shay::StatementKind::If:
    out<<indent<<"if ("<<cpp_condition(s.condition)<<") {\n";
    emit_body(out,s.body,ctx,indent+" ",function_name);
    out<<indent<<"}";
    if(!s.else_body.empty()){out<<" else {\n";emit_body(out,s.else_body,ctx,indent+" ",function_name);out<<indent<<"}";}
    out<<"\n";break;
   case shay::StatementKind::OutputCall:{
    auto fn=ctx.functions.find(s.name);
    if(fn==ctx.functions.end())break;
    out<<indent<<"shay_failures.erase("<<cpp_string(s.name)<<"); shay_handled_failures.erase("<<cpp_string(s.name)<<");\n";
    out<<indent<<"try {\n";
    if(fn->second.has_return)out<<indent<<" { auto shay_result = ";
    out<<indent<<(fn->second.has_return?"  ":" ")<<"shay_fn_"<<s.name<<"(values";
    for(const auto&arg:s.arguments)out<<", "<<cpp_expression(arg);
    out<<")";
    if(fn->second.has_return)out<<"; if(!shay_failures.count("<<cpp_string(s.name)<<")) { std::ostringstream shay_nested_call; shay_nested_call<<std::setprecision(15)<<shay_result<<"<<cpp_string("\n")<<"; shay_runtime_write(shay_nested_call.str().c_str()); } }";
    out<<";\n";
    out<<indent<<"} catch(const std::exception& shay_error) { shay_failures["<<cpp_string(s.name)<<"]=shay_error.what(); throw; }\n";
    break;
   }
   case shay::StatementKind::DoFunction:{
    auto fn=ctx.functions.find(s.name);if(fn==ctx.functions.end())break;
    out<<indent<<"shay_failures.erase("<<cpp_string(s.name)<<"); shay_handled_failures.erase("<<cpp_string(s.name)<<");\n";
     if(fn->second.has_return)
      out<<indent<<"try { { std::ostringstream shay_call_output; shay_call_output<<std::setprecision(15)<<shay_fn_"<<s.name<<"(values)<<"<<cpp_string("\n")<<"; shay_runtime_write(shay_call_output.str().c_str()); } } catch(const std::exception& shay_error) { shay_failures["<<cpp_string(s.name)<<"]=shay_error.what(); throw; }\n";
     else
      out<<indent<<"try { shay_fn_"<<s.name<<"(values); } catch(const std::exception& shay_error) { shay_failures["<<cpp_string(s.name)<<"]=shay_error.what(); throw; }\n";
    break;
   }
   case shay::StatementKind::ExecuteClass:{
    auto klass=ctx.classes.find(s.name);if(klass==ctx.classes.end())break;
    for(const auto&method:klass->second){
     out<<indent<<"shay_failures.erase("<<cpp_string(method)<<"); shay_handled_failures.erase("<<cpp_string(method)<<");\n";
      auto fn=ctx.functions.find(method);
      if(fn!=ctx.functions.end()&&fn->second.has_return)
       out<<indent<<"try { { std::ostringstream shay_call_output; shay_call_output<<std::setprecision(15)<<shay_fn_"<<method<<"(values)<<"<<cpp_string("\n")<<"; shay_runtime_write(shay_call_output.str().c_str()); } } catch(const std::exception& shay_error) { shay_failures["<<cpp_string(method)<<"]=shay_error.what(); throw; }\n";
      else
       out<<indent<<"try { shay_fn_"<<method<<"(values); } catch(const std::exception& shay_error) { shay_failures["<<cpp_string(method)<<"]=shay_error.what(); throw; }\n";
    }
    break;
   }
   default:break;
  }
 }
}
bool write_translation(const BuildContext&ctx,const fs::path&path,bool add_main,const std::string&entry_symbol){
 std::ofstream out(path,std::ios::binary);if(!out)return false;
 out<<"#include <cstdio>\n#include <exception>\n#include <fstream>\n#include <iomanip>\n#include <iostream>\n#include <sstream>\n#include <stdexcept>\n#include <string>\n#include <unordered_map>\n#include <unordered_set>\n"
       "struct ShayExit {};\n"
       "static std::unordered_map<std::string,std::string> shay_failures;\n"
       "static std::unordered_set<std::string> shay_handled_failures;\n"
       "static bool shay_failed(const std::string& name){auto it=shay_failures.find(name);if(it==shay_failures.end())return false;shay_handled_failures.insert(name);return true;}\n"
       "static double shay_number(const std::unordered_map<std::string,std::string>& values,const std::string& name){auto it=values.find(name);if(it==values.end())throw std::runtime_error(\"unknown numeric value '\"+name+\"'\");std::size_t used=0;double value=std::stod(it->second,&used);if(used!=it->second.size())throw std::runtime_error(\"value '\"+name+\"' is not numeric\");return value;}\n"
       "static double shay_div(double left,double right){if(right==0.0)throw std::runtime_error(\"division by zero\");return left/right;}\n"
       "static void shay_runtime_write(const char* text) { if(text){std::fputs(text,stdout);std::fflush(stdout);} }\n";
 out<<"static std::string shay_log_name = "<<cpp_string(ctx.log_file_name)<<";\n"
    "static std::string shay_log_extension = "<<cpp_string(ctx.log_file_extension)<<";\n"
    "static std::string shay_log_path(){std::string path=shay_log_name;auto slash=path.find_last_of(\"/\\\\\");auto dot=path.find_last_of('.');if(dot!=std::string::npos&&(slash==std::string::npos||dot>slash))path.erase(dot);return path+shay_log_extension;}\n"
    "static void shay_log(const std::string& message){const std::string path=shay_log_path();std::ofstream stream(path,std::ios::binary|std::ios::app);if(!stream)throw std::runtime_error(\"cannot open Shay log file: \"+path);stream.write(message.data(),static_cast<std::streamsize>(message.size()));if(message.empty()||message.back()!='\\n')stream.put('\\n');if(!stream)throw std::runtime_error(\"cannot write Shay log file: \"+path);}\n";
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
  if(s.has_return&&s.body.empty())out<<" return ("<<cpp_expression(s.value)<<");\n";
  else{
   emit_body(out,s.body,ctx," ",s.name);
   if(s.has_return)out<<" return 0;\n";
  }
  out<<"}\n";
 }
 out<<"extern \"C\" int "<<entry_symbol<<"() {\n try {\n std::unordered_map<std::string,std::string> values;\n";
 for(const auto&s:ctx.statements){
  switch(s.kind){
   case shay::StatementKind::InputVariable:
    out<<" std::getline(std::cin, values["<<cpp_string(s.name)<<"]);\n";break;
   case shay::StatementKind::LogFileExtension:
    if(s.dynamic_value)out<<" { auto it=values.find("<<cpp_string(s.name)<<"); if(it==values.end())throw std::runtime_error(std::string("<<cpp_string("unknown log extension variable '")<<")+"<<cpp_string(s.name)<<"+"<<cpp_string("'")<<"); shay_log_extension=it->second; }\n";
    else out<<" shay_log_extension="<<cpp_string(s.value)<<";\n";
    out<<" if(shay_log_extension.empty()||shay_log_extension[0]!='.'||shay_log_extension.find_first_not_of(\".abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-\")!=std::string::npos)throw std::runtime_error(\"invalid log file extension\");\n";break;
   case shay::StatementKind::LogFileName:
    if(s.dynamic_value)out<<" { auto it=values.find("<<cpp_string(s.name)<<"); if(it==values.end())throw std::runtime_error(std::string("<<cpp_string("unknown log filename variable '")<<")+"<<cpp_string(s.name)<<"+"<<cpp_string("'")<<"); shay_log_name=it->second; }\n";
    else out<<" shay_log_name="<<cpp_string(s.value)<<";\n";
    out<<" if(shay_log_name.empty())throw std::runtime_error(\"log filename cannot be empty\");\n";break;
   case shay::StatementKind::Declare:{
    std::string value=s.value;if(!value.empty()&&value.front()==StringMarker)value.erase(value.begin());
    out<<" values["<<cpp_string(s.name)<<"] = "<<cpp_string(value)<<";\n";break;
   }
   case shay::StatementKind::If:
    out<<" if ("<<cpp_condition(s.condition)<<") {\n";
    emit_body(out,s.body,ctx," ");
    out<<" }";
    if(!s.else_body.empty()){out<<" else {\n";emit_body(out,s.else_body,ctx," ");out<<" }";}
    out<<"\n";break;
   case shay::StatementKind::OutputText:
    out<<" shay_runtime_write("<<cpp_string(s.value+"\n")<<");\n";break;
   case shay::StatementKind::LogText:
    out<<" shay_log("<<cpp_string(s.value)<<");\n";break;
   case shay::StatementKind::OutputTextExit:
    out<<" shay_runtime_write("<<cpp_string(s.value+"\n")<<"); throw ShayExit{};\n";break;
   case shay::StatementKind::OutputValue:{
    const auto&names=s.output_names.empty()?std::vector<std::string>{s.name}:s.output_names;
    for(const auto&name:names)
     out<<" shay_runtime_write((values["<<cpp_string(name)<<"] + "<<cpp_string("\n")<<").c_str());\n";
    break;
   }
   case shay::StatementKind::OutputCall:{
    auto fn=ctx.functions.find(s.name);if(fn==ctx.functions.end())break;
    out<<" shay_failures.erase("<<cpp_string(s.name)<<"); shay_handled_failures.erase("<<cpp_string(s.name)<<");\n";
    out<<" try {\n";
    if(fn->second.has_return)out<<"  { std::ostringstream shay_call_output; shay_call_output<<std::setprecision(15)<<";
    out<<(fn->second.has_return?"   ":"  ")<<"shay_fn_"<<s.name<<"(values";
    for(const auto&arg:s.arguments)out<<", "<<cpp_expression(arg);
    out<<")";
    if(fn->second.has_return)out<<"; shay_call_output<<"<<cpp_string("\n")<<"; shay_runtime_write(shay_call_output.str().c_str()); }";
    out<<";\n";
    out<<" } catch(const std::exception& shay_error) { shay_failures["<<cpp_string(s.name)<<"]=shay_error.what(); }\n";
    break;
   }
   case shay::StatementKind::DoFunction:{
    auto fn=ctx.functions.find(s.name);if(fn==ctx.functions.end())break;
    out<<" shay_failures.erase("<<cpp_string(s.name)<<"); shay_handled_failures.erase("<<cpp_string(s.name)<<");\n";
     if(fn->second.has_return)
      out<<" try { { std::ostringstream shay_call_output; shay_call_output<<std::setprecision(15)<<shay_fn_"<<s.name<<"(values)<<"<<cpp_string("\n")<<"; shay_runtime_write(shay_call_output.str().c_str()); } } catch(const std::exception& shay_error) { shay_failures["<<cpp_string(s.name)<<"]=shay_error.what(); }\n";
     else
      out<<" try { shay_fn_"<<s.name<<"(values); } catch(const std::exception& shay_error) { shay_failures["<<cpp_string(s.name)<<"]=shay_error.what(); }\n";
    break;
   }
   case shay::StatementKind::ExecuteClass:{
    auto klass=ctx.classes.find(s.name);if(klass==ctx.classes.end())break;
    for(const auto&method:klass->second){
     out<<" shay_failures.erase("<<cpp_string(method)<<"); shay_handled_failures.erase("<<cpp_string(method)<<");\n";
      auto fn=ctx.functions.find(method);
      if(fn!=ctx.functions.end()&&fn->second.has_return)
       out<<" try { { std::ostringstream shay_call_output; shay_call_output<<std::setprecision(15)<<shay_fn_"<<method<<"(values)<<"<<cpp_string("\n")<<"; shay_runtime_write(shay_call_output.str().c_str()); } } catch(const std::exception& shay_error) { shay_failures["<<cpp_string(method)<<"]=shay_error.what(); }\n";
      else
       out<<" try { shay_fn_"<<method<<"(values); } catch(const std::exception& shay_error) { shay_failures["<<cpp_string(method)<<"]=shay_error.what(); }\n";
    }
    break;
   }
   default:break;
  }
 }
 out<<" for(const auto& failure : shay_failures) if(!shay_handled_failures.count(failure.first)){std::fprintf(stderr,\"Shay function '%s' failed: %s\\n\",failure.first.c_str(),failure.second.c_str());return 1;}\n"
       " return 0;\n } catch(const ShayExit&) { return 1; }\n"
       " catch(const std::exception& error) { std::fprintf(stderr,\"Shay runtime error: %s\\n\",error.what()); return 1; }\n}\n";
 if(!add_main&&entry_symbol!="shay_program_entry")
  out<<"#if defined(__GNUC__) || defined(__clang__)\n"
        "extern \"C\" int shay_program_entry() __attribute__((weak));\n"
        "extern \"C\" int shay_program_entry(){return "<<entry_symbol<<"();}\n"
        "#endif\n";
 if(add_main)out<<"int main(){return shay_program_entry();}\n";
 return static_cast<bool>(out);
}
void print_diagnostics(const BuildContext&ctx){
 for(const auto&d:ctx.diagnostics){
  std::cerr<<d.file.string()<<(d.warning?": warning: ":": error: ")<<d.message<<'\n';
  if(!d.source_line.empty()){
   std::cerr<<"    "<<d.source_line<<'\n'<<"    ";
   for(std::size_t i=1;i<d.column;++i)std::cerr<<' ';
   const std::size_t width=d.length?d.length:1;
   for(std::size_t i=1;i<width;++i)std::cerr<<'~';
   std::cerr<<'^'<<'\n';
  }
 }
}
} // namespace

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
  std::cout<<"Usage:\n  gshai [Shay flags] -ob [output.sho] program.shay|program.shaz   Compile a native object\n  gshai [Shay flags] -o  [output]     program.shay|program.shaz   Compile and link an executable\n  gshai [Shay flags] -o  [output]     program.sho [more.sho ...]   Link existing objects\n  gshai --version\n\n"
           <<"-ob emits .sho object files; -o compiles source or links one or more .sho objects in order.\n"
           <<"Shay compiler flags (may appear anywhere):\n"
           <<"  --shaywall    Enable common Shay warnings, including unused variables\n"
           <<"  --shaywextra  Enable additional Shay warnings\n"
           <<"  --shayo0      Disable optimization (-O0)\n"
           <<"  --shayo1      Enable level 1 optimization (-O1)\n"
           <<"  --shayo2      Enable level 2 optimization (-O2; default)\n"
           <<"  --shayo3      Enable aggressive optimization (-O3)\n";
  return 0;
 }
 if(positional.size()<2){std::cerr<<"gshai: expected -ob or -o and an input file (try --help)\n";return 2;}
 const std::string mode=positional[0];if(mode!="-ob"&&mode!="-o"){std::cerr<<"gshai: unknown output mode: "<<mode<<'\n';return 2;}
 const bool object_mode=mode=="-ob";
 fs::path output;std::vector<fs::path> inputs;
 if(positional.size()==2)inputs.emplace_back(positional[1]);
 else{output=positional[1];for(std::size_t i=2;i<positional.size();++i)inputs.emplace_back(positional[i]);}
 if(inputs.empty()){std::cerr<<"gshai: no input files provided\n";return 2;}
 const bool link_mode=inputs.front().extension()==".sho";
 if(link_mode){
  if(object_mode){std::cerr<<"gshai: -ob requires a .shay or .shaz source file; use -o to link .sho files\n";return 2;}
  for(const auto&input:inputs)if(input.extension()!=".sho"){std::cerr<<"gshai: cannot mix .sho objects with source files in one link\n";return 2;}
  if(output.empty()){
   if(inputs.size()!=1){std::cerr<<"gshai: multiple .sho inputs require an explicit output path\n";return 2;}
   output=inputs.front().parent_path()/inputs.front().stem();
  }
  std::unordered_set<std::string> symbols;
  for(const auto&input:inputs)if(!symbols.insert(object_entry_symbol(input)).second){std::cerr<<"gshai: object files have colliding entry names (same filename stem)\n";return 2;}
 }else{
  if(inputs.size()!=1){std::cerr<<"gshai: source compilation accepts exactly one .shay or .shaz input\n";return 2;}
  const fs::path&input=inputs.front();
  if(input.extension()!=".shay"&&input.extension()!=".shaz"){std::cerr<<"gshai: input must be a .shay, .shaz, or (with -o) .sho file\n";return 2;}
  if(output.empty()){output=input.parent_path()/input.stem();if(object_mode)output+=".sho";}
  else if(object_mode&&output.extension()!=".sho"){std::cerr<<"gshai: -ob output must use the .sho extension\n";return 2;}
 }
 if(link_mode){
  const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
  const fs::path wrapper=fs::temp_directory_path()/("shay-link-"+std::to_string(stamp)+".cpp");
  {std::ofstream out(wrapper,std::ios::binary);if(!out){std::cerr<<"gshai: cannot write temporary linker entry point\n";return 1;}
   for(const auto&input:inputs)out<<"extern \"C\" int "<<object_entry_symbol(input)<<"();\n";
   out<<"int main(){\n";
   for(const auto&input:inputs)out<<" if(int result="<<object_entry_symbol(input)<<"();result!=0)return result;\n";
   out<<" return 0;\n}\n";
  }
  const char*configured=std::getenv("CXX");const std::string cxx=configured&&*configured?configured:"c++";
  int status=0;
#ifdef _WIN32
  std::vector<std::string> arguments={cxx,"-std=c++20","-O"+optimization};
  arguments.push_back("-w");
  for(const auto&input:inputs)arguments.push_back(input.string());
  arguments.push_back(wrapper.string());arguments.push_back("-o");arguments.push_back(output.string());
  std::vector<const char*> compiler_argv;compiler_argv.reserve(arguments.size()+1);
  for(const auto&argument:arguments)compiler_argv.push_back(argument.c_str());
  compiler_argv.push_back(nullptr);
  status=static_cast<int>(_spawnvp(_P_WAIT,cxx.c_str(),compiler_argv.data()));
#else
  std::string command=shell_quote(cxx)+" -std=c++20 -O"+optimization+" ";
  command+="-w ";
  for(const auto&input:inputs)command+=shell_quote(input.string())+" ";
  command+=shell_quote(wrapper.string())+" -o "+shell_quote(output.string());
  status=std::system(command.c_str());
#endif
  std::error_code ignored;fs::remove(wrapper,ignored);
  if(status!=0){fs::remove(output,ignored);std::cerr<<"gshai: native linker failed\n";return 1;}
  std::cout<<"gshai: linked executable "<<output.string()<<" from "<<inputs.size()<<" .sho object(s)\n";return 0;
 }
 const fs::path&input=inputs.front();
 BuildContext context;load_file(input,context);
 for(const auto&entry:context.pending_function_validation){
  const auto&file=entry.first;const auto&s=entry.second;
  std::unordered_set<std::string> parameters(s.parameters.begin(),s.parameters.end());
  if(s.has_return)validate_expression_names(context,file,s.line,s.value,parameters,"function '"+s.name+"' return expression");
  validate_body(context,file,s.body,parameters,"function body",s.name);
 }
 for(const auto&s:context.statements)
  if((s.kind==shay::StatementKind::LogFileExtension||s.kind==shay::StatementKind::LogFileName)&&s.dynamic_value&&!context.variables.count(s.name))
   diagnose(context,input,s.line,"log configuration references unknown variable '"+s.name+"'");
 if(warn_all||warn_extra)warn_about_unused_variables(context);
 if(!context.diagnostics.empty()){
  print_diagnostics(context);
  for(const auto&diagnostic:context.diagnostics)if(!diagnostic.warning)return 1;
 }
 const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();const fs::path temp=fs::temp_directory_path()/("shay-build-"+std::to_string(stamp)+".cpp");
 if(!write_translation(context,temp,!object_mode,object_mode?object_entry_symbol(output):"shay_program_entry")){std::cerr<<"gshai: cannot write temporary translation unit\n";return 1;}
 const char*configured=std::getenv("CXX");const std::string cxx=configured&&*configured?configured:"c++";
 int status=0;
#ifdef _WIN32
 std::vector<std::string> arguments={cxx,"-std=c++20","-O"+optimization};
 arguments.push_back("-w");
 if(object_mode)arguments.push_back("-c");
 arguments.push_back(temp.string());
 arguments.push_back("-o");
 arguments.push_back(output.string());
 std::vector<const char*> compiler_argv;compiler_argv.reserve(arguments.size()+1);
 for(const auto&argument:arguments)compiler_argv.push_back(argument.c_str());
 compiler_argv.push_back(nullptr);
 status=static_cast<int>(_spawnvp(_P_WAIT,cxx.c_str(),compiler_argv.data()));
#else
 std::string command=shell_quote(cxx)+" -std=c++20 -O"+optimization+" ";
 command+="-w ";
 command+=(object_mode?"-c ":"")+shell_quote(temp.string())+" -o "+shell_quote(output.string());
 status=std::system(command.c_str());
#endif
 std::error_code ignored;fs::remove(temp,ignored);
 if(status!=0){fs::remove(output,ignored);std::cerr<<"gshai: native compiler/linker failed\n";return 1;}
 std::cout<<"gshai: wrote "<<(object_mode?"object file ":"executable ")<<output.string()<<'\n';return 0;
}
