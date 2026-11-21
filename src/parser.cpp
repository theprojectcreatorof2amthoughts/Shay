#include "parser.hpp"
#include <filesystem>
#include <functional>
#include <unordered_set>
namespace shay {
Parser::Parser(const std::vector<Token> &tokens):tokens_(tokens){}
const Token &Parser::peek() const{return tokens_[current_];}
const Token &Parser::previous() const{return tokens_[current_-1];}
bool Parser::atEnd() const{return peek().kind==TokenKind::End;}
bool Parser::match(TokenKind kind,const std::string &text){if(peek().kind!=kind||(!text.empty()&&peek().text!=text))return false;++current_;return true;}
void Parser::skipLine(){while(!atEnd()&&peek().kind!=TokenKind::Newline)++current_;if(peek().kind==TokenKind::Newline)++current_;}
void Parser::error(std::vector<ParseError>&errors,const std::string&message){errors.push_back({peek().line,peek().column,message});}
bool Parser::consume(TokenKind kind,const std::string&text,std::vector<ParseError>&errors,const std::string&message){if(match(kind,text))return true;error(errors,message);return false;}
namespace {
bool expression_token(const Token&t){
 if(t.kind==TokenKind::Identifier||t.kind==TokenKind::Number)return true;
 return t.kind==TokenKind::Symbol&&(t.text=="+"||t.text=="-"||t.text=="*"||t.text=="/"||t.text=="("||t.text==")");
}
std::string collect_expression(const std::vector<Token>&tokens,std::size_t&at,const std::string&stop,bool&valid){
 std::string result;int depth=0;
 while(at<tokens.size()&&tokens[at].kind!=TokenKind::End&&tokens[at].kind!=TokenKind::Newline){
  const Token&t=tokens[at];
  if(t.kind==TokenKind::Symbol&&depth==0&&(t.text==stop||t.text==")"))break;
  if(!expression_token(t)){valid=false;break;}
  if(t.kind==TokenKind::Symbol&&t.text=="(")++depth;
  if(t.kind==TokenKind::Symbol&&t.text==")"){if(depth==0){valid=false;break;}--depth;}
  result+=t.text;++at;
 }
 if(depth!=0)valid=false;
 return result;
}
}
Program Parser::parse(){
 Program program;
 while(!atEnd()){
  if(match(TokenKind::Newline))continue;
  const auto line=peek().line;Statement statement;statement.line=line;bool valid=true;bool skipCurrentLine=true;
  if(match(TokenKind::Symbol,"!#")){
   statement.kind=StatementKind::MarkFailure;
   valid=consume(TokenKind::Identifier,"",program.errors,"expected a function name after '!#'");
   if(valid)statement.name=previous().text;
  }else if(match(TokenKind::Identifier,"ifit")){
   statement.kind=StatementKind::If;
   while(!atEnd()&&peek().kind!=TokenKind::Newline){statement.condition+=peek().text;statement.condition.push_back(' ');++current_;}
   if(statement.condition.empty()){error(program.errors,"ifit requires a condition");valid=false;}
   if(peek().kind==TokenKind::Newline)++current_;
   std::vector<Token> bodyTokens,elseTokens;bool foundExec=false,seenElse=false;int ifDepth=1;
   while(!atEnd()){
    if(peek().kind==TokenKind::Identifier&&peek().text=="ifit"){
     ++ifDepth;(seenElse?elseTokens:bodyTokens).push_back(peek());++current_;continue;
    }
    if(peek().kind==TokenKind::Identifier&&peek().text=="exec"){
     --ifDepth;
     if(ifDepth==0){
      foundExec=true;++current_;
      if(peek().kind!=TokenKind::Newline&&!atEnd()){error(program.errors,"unexpected token after 'exec'");valid=false;}
      if(peek().kind==TokenKind::Newline)++current_;
      skipCurrentLine=false;break;
     }
     (seenElse?elseTokens:bodyTokens).push_back(peek());++current_;continue;
    }
    if(ifDepth==1&&peek().kind==TokenKind::Identifier&&peek().text=="elseit"){
     if(seenElse){error(program.errors,"an ifit block can contain only one elseit branch");valid=false;}
     seenElse=true;++current_;
     if(peek().kind!=TokenKind::Newline&&!atEnd()){error(program.errors,"unexpected token after 'elseit'");valid=false;}
     if(peek().kind==TokenKind::Newline)++current_;
     continue;
    }
    (seenElse?elseTokens:bodyTokens).push_back(peek());++current_;
   }
   const Token endToken{TokenKind::End,"",peek().line,peek().column};
   bodyTokens.push_back(endToken);elseTokens.push_back(endToken);
   if(!foundExec){error(program.errors,"expected 'exec' to close the ifit block");valid=false;}
   Parser nested(bodyTokens);Program nestedProgram=nested.parse();
   statement.body=std::move(nestedProgram.statements);
   for(const auto&e:nestedProgram.errors)program.errors.push_back(e);
   if(seenElse){
    Parser elseNested(elseTokens);Program elseProgram=elseNested.parse();
    statement.else_body=std::move(elseProgram.statements);
    for(const auto&e:elseProgram.errors)program.errors.push_back(e);
   }
  }else if(match(TokenKind::Symbol,"$")){
   if(match(TokenKind::Identifier,"shaiiport")||match(TokenKind::Identifier,"shazport")){
    const bool library=previous().text=="shaiiport";statement.kind=library?StatementKind::ImportLibrary:StatementKind::ImportTask;
    const std::string ext=library?".shaii":".shaz";
    std::string filename;
    if(library&&match(TokenKind::String)){
     // Quoted imports are explicit paths on disk; unlike bracket imports,
     // they must not fall back to the siil standard-library search path.
     statement.external_import=true;
     filename=previous().text;
     if(filename.empty()){error(program.errors,"expected an external .shaii filename inside quotes");valid=false;}
     else if(std::filesystem::path(filename).extension()!=ext){error(program.errors,"quoted $shaiiport imports require a .shaii file");valid=false;}
    }else{
     valid=consume(TokenKind::Symbol,"[",program.errors,"expected '[' before the "+ext+" filename");
     while(valid&&!atEnd()&&peek().kind!=TokenKind::Newline&&!(peek().kind==TokenKind::Symbol&&peek().text=="]")){
      if(peek().kind==TokenKind::Identifier||peek().kind==TokenKind::Number||(peek().kind==TokenKind::Symbol&&(peek().text=="."||peek().text=="-"||peek().text=="_"))){filename+=peek().text;++current_;}
      else{error(program.errors,"invalid character in "+ext+" filename");valid=false;}
     }
     if(valid&&filename.empty()){error(program.errors,"expected a "+ext+" filename inside '[' and ']'");valid=false;}
     if(valid)valid=consume(TokenKind::Symbol,"]",program.errors,"expected ']' after the filename");
    }
    if(valid)statement.value=filename;
   }else if(match(TokenKind::Identifier,"logindxfl")){
    statement.kind=StatementKind::LogFileExtension;
    if(match(TokenKind::Symbol,"<")){
     valid=consume(TokenKind::Symbol,".",program.errors,"expected an extension beginning with '.'");
     if(valid)valid=consume(TokenKind::Identifier,"",program.errors,"expected a file extension after '.'");
     if(valid){statement.value="."+previous().text;
      if(statement.value.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-")!=std::string::npos){error(program.errors,"invalid log file extension");valid=false;}}
     if(valid)valid=consume(TokenKind::Symbol,">",program.errors,"expected '>' after the log file extension");
    }else if(match(TokenKind::Identifier)){
     statement.name=previous().text;statement.dynamic_value=true;
    }else{error(program.errors,"expected '<.extension>' or a variable name after '$logindxfl'");valid=false;}
   }else if(match(TokenKind::Identifier,"logindxnm")){
    statement.kind=StatementKind::LogFileName;
    if(match(TokenKind::String)){
     statement.value=previous().text;
     if(statement.value.empty()){error(program.errors,"log filename cannot be empty");valid=false;}
    }else if(match(TokenKind::Identifier)){
     statement.name=previous().text;statement.dynamic_value=true;
    }else{error(program.errors,"expected a quoted log filename or variable name after '$logindxnm'");valid=false;}
   }else if(match(TokenKind::Identifier,"classfunc")){
    statement.kind=StatementKind::DeclareClass;
    valid=consume(TokenKind::Symbol,"!",program.errors,"expected '!' before the class name");
    if(valid)valid=consume(TokenKind::Identifier,"",program.errors,"expected a class name after '!'");
    if(valid)statement.name="!"+previous().text;
    if(valid&&peek().kind!=TokenKind::Newline){error(program.errors,"expected a newline after class declaration");valid=false;}
    if(peek().kind==TokenKind::Newline)++current_;
    while(match(TokenKind::Newline)){}
    while(valid&&!atEnd()&&!(peek().kind==TokenKind::Identifier&&peek().text=="finish")){
     if(!consume(TokenKind::Symbol,"$",program.errors,"expected '$declrmthd' or 'finish' inside class")){valid=false;break;}
     if(!consume(TokenKind::Identifier,"declrmthd",program.errors,"expected 'declrmthd' inside class")){valid=false;break;}
     Statement method;method.kind=StatementKind::DeclareMethod;method.line=previous().line;
     if(!consume(TokenKind::Identifier,"",program.errors,"expected a method name after '$declrmthd'")){valid=false;break;}
     method.name=previous().text;
     if(peek().kind!=TokenKind::Newline){error(program.errors,"expected a newline after method declaration");valid=false;break;}
     ++current_;while(match(TokenKind::Newline)){}
     std::vector<Token> methodTokens;bool foundMethodFinish=false;int methodIfDepth=0;
     while(!atEnd()){
      if(peek().kind==TokenKind::Identifier&&peek().text=="ifit"){++methodIfDepth;methodTokens.push_back(peek());++current_;continue;}
      if(peek().kind==TokenKind::Identifier&&peek().text=="exec"&&methodIfDepth>0){--methodIfDepth;methodTokens.push_back(peek());++current_;continue;}
      if(peek().kind==TokenKind::Identifier&&peek().text=="finish"&&methodIfDepth==0){foundMethodFinish=true;++current_;break;}
      methodTokens.push_back(peek());++current_;
     }
     if(!foundMethodFinish){error(program.errors,"expected 'finish' to close method '"+method.name+"'");valid=false;break;}
     methodTokens.push_back({TokenKind::End,"",peek().line,peek().column});
     Parser methodParser(methodTokens);Program methodProgram=methodParser.parse();
     method.body=std::move(methodProgram.statements);
     std::function<bool(const std::vector<Statement>&)> method_has_return=[&](const auto&items){
      for(const auto&item:items){
       if(item.kind==StatementKind::ReturnValue)return true;
       if(method_has_return(item.body)||method_has_return(item.else_body))return true;
      }
      return false;
     };
     method.has_return=method_has_return(method.body);
     for(const auto&e:methodProgram.errors)program.errors.push_back(e);
     statement.body.push_back(std::move(method));
     if(peek().kind!=TokenKind::Newline&&!atEnd()){error(program.errors,"unexpected token after method 'finish'");valid=false;break;}
     if(peek().kind==TokenKind::Newline)++current_;
     while(match(TokenKind::Newline)){}
    }
    if(valid)valid=consume(TokenKind::Identifier,"finish",program.errors,"expected 'finish' to close the class");
    if(valid&&peek().kind!=TokenKind::Newline&&!atEnd()){error(program.errors,"unexpected token after class 'finish'");valid=false;}
   }else if(match(TokenKind::Identifier,"doclass")){
    statement.kind=StatementKind::ExecuteClass;
    valid=consume(TokenKind::Symbol,"!",program.errors,"expected '!' before the class name");
    if(valid)valid=consume(TokenKind::Identifier,"",program.errors,"expected a class name after '!'");
    if(valid)statement.name="!"+previous().text;
   }else if(match(TokenKind::Identifier,"declr")){
    statement.kind=StatementKind::DeclareFunction;
    valid=consume(TokenKind::Identifier,"",program.errors,"expected a function name after '$declr");
    if(valid)statement.name=previous().text;
    if(valid)valid=consume(TokenKind::Symbol,"(",program.errors,"expected '(' after function name");
    std::unordered_set<std::string> names;
    while(valid&&peek().kind!=TokenKind::End&&!(peek().kind==TokenKind::Symbol&&peek().text==")")){
     if(!consume(TokenKind::Identifier,"",program.errors,"expected a parameter name")){valid=false;break;}
     const auto parameter=previous().text;
     if(!names.insert(parameter).second){error(program.errors,"duplicate function parameter '"+parameter+"'");valid=false;break;}
     statement.parameters.push_back(parameter);
     if(match(TokenKind::Symbol,","))continue;
     break;
    }
    if(valid)valid=consume(TokenKind::Symbol,")",program.errors,"expected ')' after function parameters");
    if(valid&&peek().kind!=TokenKind::Newline){error(program.errors,"expected a newline after function declaration");valid=false;}
    if(peek().kind==TokenKind::Newline)++current_;
    while(match(TokenKind::Newline)){}
    if(valid&&match(TokenKind::Identifier,"forw")){
     statement.has_return=true;
     statement.value=collect_expression(tokens_,current_,"",valid);
     if(statement.value.empty()){error(program.errors,"'forw' requires a return expression");valid=false;}
     if(peek().kind==TokenKind::Newline)++current_;else if(!atEnd()){error(program.errors,"expected a newline after return expression");valid=false;}
     while(match(TokenKind::Newline)){}
     if(valid)valid=consume(TokenKind::Identifier,"finish",program.errors,"expected 'finish' to close the function");
    }else if(valid){
     std::vector<Token> bodyTokens;bool foundFinish=false;int ifDepth=0;
     while(!atEnd()){
      if(peek().kind==TokenKind::Identifier&&peek().text=="ifit"){++ifDepth;bodyTokens.push_back(peek());++current_;continue;}
      if(peek().kind==TokenKind::Identifier&&peek().text=="exec"&&ifDepth>0){--ifDepth;bodyTokens.push_back(peek());++current_;continue;}
      if(peek().kind==TokenKind::Identifier&&peek().text=="finish"&&ifDepth==0){foundFinish=true;++current_;break;}
      bodyTokens.push_back(peek());++current_;
     }
     if(!foundFinish){error(program.errors,"expected 'finish' to close the function");valid=false;}
     bodyTokens.push_back({TokenKind::End,"",peek().line,peek().column});
     Parser nested(bodyTokens);Program nestedProgram=nested.parse();
     statement.body=std::move(nestedProgram.statements);
     std::function<bool(const std::vector<Statement>&)> has_return=[&](const auto&items){
      for(const auto&item:items){
       if(item.kind==StatementKind::ReturnValue)return true;
       if(has_return(item.body)||has_return(item.else_body))return true;
      }
      return false;
     };
     statement.has_return=has_return(statement.body);
     for(const auto&e:nestedProgram.errors)program.errors.push_back(e);
    }
    if(valid&&peek().kind!=TokenKind::Newline&&!atEnd()){error(program.errors,"unexpected token after 'finish'");valid=false;}
   }else{error(program.errors,"expected shaiiport, shazport, declr, classfunc, or doclass after '$'");valid=false;}
  }else if(match(TokenKind::Identifier,"dofunc")){
   statement.kind=StatementKind::DoFunction;
   valid=consume(TokenKind::Identifier,"",program.errors,"expected a function name after 'dofunc'");
   if(valid)statement.name=previous().text;
  }else if(match(TokenKind::Identifier,"brew")){
   statement.kind=StatementKind::Declare;
   valid=consume(TokenKind::Identifier,"",program.errors,"expected a variable name after 'brew'");
   if(valid)statement.name=previous().text;
   if(valid)valid=consume(TokenKind::Symbol,"=",program.errors,"expected '=' after variable name");
   if(valid&&match(TokenKind::Symbol,"{")){
    statement.map_value=true;
    while(valid&&!(peek().kind==TokenKind::Symbol&&peek().text=="}")&&!atEnd()&&peek().kind!=TokenKind::Newline){
     if(!consume(TokenKind::String,"",program.errors,"map keys must be quoted strings")){valid=false;break;}
     const std::string key=previous().text;
     for(const auto&entry:statement.map_items)if(entry.first==key){error(program.errors,"duplicate map key '"+key+"'");valid=false;break;}
     if(!valid)break;
     if(!consume(TokenKind::Symbol,":",program.errors,"expected ':' after map key")){valid=false;break;}
     std::string value;
     if(match(TokenKind::String))value=previous().text;
     else if(match(TokenKind::Number)||match(TokenKind::Identifier,"true")||match(TokenKind::Identifier,"false"))value=previous().text;
     else if(match(TokenKind::Symbol,"-")){if(match(TokenKind::Number))value="-"+previous().text;else{error(program.errors,"expected digits after unary '-' in map value");valid=false;}}
     else{error(program.errors,"map values must be strings, numbers, or booleans");valid=false;}
     if(!valid)break;
     statement.map_items.push_back({key,value});
     if(match(TokenKind::Symbol,","))continue;
     if(!(peek().kind==TokenKind::Symbol&&peek().text=="}")){error(program.errors,"expected ',' or '}' after map entry");valid=false;}
    }
    if(valid)valid=consume(TokenKind::Symbol,"}",program.errors,"expected '}' to close map literal");
   }else if(valid&&match(TokenKind::Identifier,"set")){
    statement.set_value=true;
    if(!consume(TokenKind::Symbol,"[",program.errors,"expected '[' after 'set'"))valid=false;
    while(valid&&!(peek().kind==TokenKind::Symbol&&peek().text=="]")&&!atEnd()&&peek().kind!=TokenKind::Newline){
     std::string item;
     if(match(TokenKind::String))item=previous().text;
     else if(match(TokenKind::Number)||match(TokenKind::Identifier,"true")||match(TokenKind::Identifier,"false"))item=previous().text;
     else if(match(TokenKind::Symbol,"-")){if(match(TokenKind::Number))item="-"+previous().text;else{error(program.errors,"expected digits after unary '-' in set");valid=false;break;}}
     else{error(program.errors,"set items must be strings, numbers, or booleans");valid=false;break;}
     statement.list_items.push_back(item);
     if(match(TokenKind::Symbol,","))continue;
     if(!(peek().kind==TokenKind::Symbol&&peek().text=="]")){error(program.errors,"expected ',' or ']' after set item");valid=false;}
    }
    if(valid)valid=consume(TokenKind::Symbol,"]",program.errors,"expected ']' to close set literal");
   }else if(valid&&match(TokenKind::Symbol,"[")){
    statement.list_value=true;
    while(valid&&!(peek().kind==TokenKind::Symbol&&peek().text=="]")&&!atEnd()&&peek().kind!=TokenKind::Newline){
     std::string item;
     if(match(TokenKind::String))item=previous().text;
     else if(match(TokenKind::Number)||match(TokenKind::Identifier,"true")||match(TokenKind::Identifier,"false"))item=previous().text;
     else if(match(TokenKind::Symbol,"-")){
      if(match(TokenKind::Number))item="-"+previous().text;
      else{error(program.errors,"expected digits after unary '-' in list");valid=false;break;}
     }else{error(program.errors,"list items must be strings, numbers, or booleans");valid=false;break;}
     statement.list_items.push_back(item);
     if(match(TokenKind::Symbol,","))continue;
     if(!(peek().kind==TokenKind::Symbol&&peek().text=="]")){error(program.errors,"expected ',' or ']' after list item");valid=false;}
    }
    if(valid)valid=consume(TokenKind::Symbol,"]",program.errors,"expected ']' to close list literal");
   }else if(valid){
    if(match(TokenKind::String))statement.value=std::string(1,static_cast<char>(1))+previous().text;
    else if(match(TokenKind::Number)||match(TokenKind::Identifier,"true")||match(TokenKind::Identifier,"false"))statement.value=previous().text;
    else if(match(TokenKind::Symbol,"-")){if(match(TokenKind::Number))statement.value="-"+previous().text;else{error(program.errors,"expected digits after unary '-'");valid=false;}}
    else{error(program.errors,"expected a string, number, or boolean literal");valid=false;}
   }
  }else if(match(TokenKind::Identifier,"inpvarforw")){
   statement.kind=StatementKind::InputVariable;
   valid=consume(TokenKind::Symbol,"/>",program.errors,"expected '/>' after 'inpvarforw'");
   if(valid)valid=consume(TokenKind::Identifier,"",program.errors,"expected a variable name after 'inpvarforw/>'");
   if(valid)statement.name=previous().text;
  }else if(match(TokenKind::Identifier,"forw")){
   statement.kind=StatementKind::ReturnValue;
   statement.value=collect_expression(tokens_,current_,"",valid);
   if(statement.value.empty()){error(program.errors,"'forw' requires a return expression");valid=false;}
  }else if(match(TokenKind::Identifier,"logit")){
   statement.kind=StatementKind::LogText;
   valid=consume(TokenKind::Symbol,"/>",program.errors,"expected '/>' after 'logit'");
   if(valid)valid=consume(TokenKind::String,"",program.errors,"expected quoted log text after 'logit/>'");
   if(valid)statement.value=previous().text;
  }else if(match(TokenKind::Identifier,"instxt")){
   statement.kind=StatementKind::OutputText;valid=consume(TokenKind::Symbol,"/>",program.errors,"expected '/>' after 'instxt'");
   if(valid)valid=consume(TokenKind::String,"",program.errors,"expected quoted text after 'instxt/>'");
   if(valid)statement.value=previous().text;
  }else if(match(TokenKind::Identifier,"instxtexit")){
   statement.kind=StatementKind::OutputTextExit;valid=consume(TokenKind::Symbol,"/>",program.errors,"expected '/>' after 'instxtexit'");
   if(valid)valid=consume(TokenKind::String,"",program.errors,"expected quoted text after 'instxtexit/>'");
   if(valid)statement.value=previous().text;
  }else if(match(TokenKind::Identifier,"insval")){
   statement.kind=StatementKind::OutputValue;valid=consume(TokenKind::Symbol,"/>",program.errors,"expected '/>' after 'insval'");
   if(valid)valid=consume(TokenKind::Identifier,"",program.errors,"expected a variable or function name after 'insval/>'");
   if(valid){statement.name=previous().text;statement.output_names.push_back(statement.name);}
   if(valid&&match(TokenKind::Symbol,"[")){
    if(match(TokenKind::String)){statement.output_keyed=true;statement.output_key=previous().text;}
    else if(consume(TokenKind::Number,"",program.errors,"expected a non-negative list index or quoted map key")){
     const std::string index=previous().text;
     if(index.find_first_not_of("0123456789")!=std::string::npos){error(program.errors,"list index must be a non-negative integer");valid=false;}
     else{try{statement.list_index=static_cast<std::size_t>(std::stoull(index));statement.output_indexed=true;}catch(...){error(program.errors,"list index is too large");valid=false;}}
    }else valid=false;
    if(valid)valid=consume(TokenKind::Symbol,"]",program.errors,"expected ']' after collection index");
   }
   if(valid&&match(TokenKind::Symbol,"(")){
    statement.kind=StatementKind::OutputCall;
    statement.output_names.clear();
    if(!(peek().kind==TokenKind::Symbol&&peek().text==")")){
     while(valid){
      auto arg=collect_expression(tokens_,current_,",",valid);
      if(!valid||arg.empty()){error(program.errors,"expected a function argument expression");valid=false;break;}
      statement.arguments.push_back(arg);
      if(match(TokenKind::Symbol,","))continue;
      break;
     }
    }
    if(valid)valid=consume(TokenKind::Symbol,")",program.errors,"expected ')' after function arguments");
   }else if(valid){
    while(match(TokenKind::Symbol,",")){
     if(statement.output_names.size()>=10){
      error(program.errors,"insval/> supports a maximum of 10 variables");
      valid=false;
      skipLine();
      skipCurrentLine=false;
      break;
     }
     if(!consume(TokenKind::Identifier,"",program.errors,"expected a variable name after ','")){
      valid=false;
      break;
     }
     statement.output_names.push_back(previous().text);
    }
   }
  }else if(match(TokenKind::Identifier,"listadd")||match(TokenKind::Identifier,"listset")||match(TokenKind::Identifier,"listremove")){
   const std::string operation=previous().text;
   statement.kind=operation=="listadd"?StatementKind::ListAppend:(operation=="listset"?StatementKind::ListSet:StatementKind::ListRemove);
   valid=consume(TokenKind::Symbol,"/>",program.errors,"expected '/>' after '"+operation+"'");
   if(valid)valid=consume(TokenKind::Identifier,"",program.errors,"expected a list variable name");
   if(valid)statement.name=previous().text;
   if(valid)valid=consume(TokenKind::Symbol,",",program.errors,"expected ',' after the list variable name");
   if(valid&&statement.kind==StatementKind::ListAppend){
    if(match(TokenKind::String))statement.value=previous().text;
    else if(match(TokenKind::Number)||match(TokenKind::Identifier,"true")||match(TokenKind::Identifier,"false"))statement.value=previous().text;
    else if(match(TokenKind::Symbol,"-")){if(match(TokenKind::Number))statement.value="-"+previous().text;else{error(program.errors,"expected digits after unary '-'");valid=false;}}
    else{error(program.errors,"listadd value must be a string, number, or boolean literal");valid=false;}
   }else if(valid){
    if(!consume(TokenKind::Number,"",program.errors,"expected a non-negative integer list index"))valid=false;
    else{
     const std::string index=previous().text;
     if(index.find_first_not_of("0123456789")!=std::string::npos){error(program.errors,"list index must be a non-negative integer");valid=false;}
     else{try{statement.list_index=static_cast<std::size_t>(std::stoull(index));}catch(...){error(program.errors,"list index is too large");valid=false;}}
    }
    if(valid&&statement.kind==StatementKind::ListSet){
     valid=consume(TokenKind::Symbol,",",program.errors,"expected ',' after list index");
     if(valid&&match(TokenKind::String))statement.value=previous().text;
     else if(valid&&(match(TokenKind::Number)||match(TokenKind::Identifier,"true")||match(TokenKind::Identifier,"false")))statement.value=previous().text;
     else if(valid&&match(TokenKind::Symbol,"-")){if(match(TokenKind::Number))statement.value="-"+previous().text;else{error(program.errors,"expected digits after unary '-'");valid=false;}}
     else if(valid){error(program.errors,"listset value must be a string, number, or boolean literal");valid=false;}
    }
   }
  }else if(match(TokenKind::Identifier,"mapset")||match(TokenKind::Identifier,"mapremove")||match(TokenKind::Identifier,"setadd")||match(TokenKind::Identifier,"setremove")||match(TokenKind::Identifier,"setcontains")){
   const std::string operation=previous().text;
   statement.kind=operation=="mapset"?StatementKind::MapSet:(operation=="mapremove"?StatementKind::MapRemove:(operation=="setadd"?StatementKind::SetAdd:(operation=="setremove"?StatementKind::SetRemove:StatementKind::SetContains)));
   valid=consume(TokenKind::Symbol,"/>",program.errors,"expected '/>' after '"+operation+"'");
   if(valid)valid=consume(TokenKind::Identifier,"",program.errors,"expected a collection variable name");
   if(valid)statement.name=previous().text;
   if(valid)valid=consume(TokenKind::Symbol,",",program.errors,"expected ',' after collection variable name");
   if(valid&&(statement.kind==StatementKind::MapSet||statement.kind==StatementKind::MapRemove)){
    valid=consume(TokenKind::String,"",program.errors,"map keys must be quoted strings");
    if(valid)statement.output_key=previous().text;
    if(valid&&statement.kind==StatementKind::MapSet){
     valid=consume(TokenKind::Symbol,",",program.errors,"expected ',' after map key");
     if(valid&&match(TokenKind::String))statement.value=previous().text;
     else if(valid&&(match(TokenKind::Number)||match(TokenKind::Identifier,"true")||match(TokenKind::Identifier,"false")))statement.value=previous().text;
     else if(valid&&match(TokenKind::Symbol,"-")){if(match(TokenKind::Number))statement.value="-"+previous().text;else{error(program.errors,"expected digits after unary '-'");valid=false;}}
     else if(valid){error(program.errors,"mapset value must be a string, number, or boolean literal");valid=false;}
    }
   }else if(valid){
    if(match(TokenKind::String))statement.value=previous().text;
    else if(match(TokenKind::Number)||match(TokenKind::Identifier,"true")||match(TokenKind::Identifier,"false"))statement.value=previous().text;
    else if(match(TokenKind::Symbol,"-")){if(match(TokenKind::Number))statement.value="-"+previous().text;else{error(program.errors,"expected digits after unary '-'");valid=false;}}
    else{error(program.errors,"set operation value must be a string, number, or boolean literal");valid=false;}
   }
  }else{error(program.errors,"unrecognized Shay statement");valid=false;}
  if(skipCurrentLine&&!atEnd()&&peek().kind!=TokenKind::Newline){error(program.errors,"unexpected token after statement");valid=false;}
  if(valid)program.statements.push_back(statement);
  if(skipCurrentLine)skipLine();
 }
 return program;
}
}
