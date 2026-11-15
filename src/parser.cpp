#include "parser.hpp"
#include <filesystem>
#include <unordered_set>
namespace shay {
Parser::Parser(const std::vector<Token> &tokens):tokens_(tokens){}
const Token &Parser::peek() const{return tokens_[current_];}
const Token &Parser::previous() const{return tokens_[current_-1];}
bool Parser::atEnd() const{return peek().kind==TokenKind::End;}
bool Parser::match(TokenKind kind,const std::string &text){if(peek().kind!=kind||(!text.empty()&&peek().text!=text))return false;++current_;return true;}
void Parser::skipLine(){while(!atEnd()&&peek().kind!=TokenKind::Newline)++current_;if(peek().kind==TokenKind::Newline)++current_;}
void Parser::error(std::vector<ParseError>&errors,const std::string&message){errors.push_back({peek().line,message});}
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
  if(match(TokenKind::Identifier,"ifit")){
   statement.kind=StatementKind::If;
   while(!atEnd()&&peek().kind!=TokenKind::Newline){statement.condition+=peek().text;++current_;}
   if(statement.condition.empty()){error(program.errors,"ifit requires a condition");valid=false;}
   if(peek().kind==TokenKind::Newline)++current_;
   std::vector<Token> bodyTokens;bool foundExec=false;
   while(!atEnd()){
    if(peek().kind==TokenKind::Identifier&&peek().text=="exec"){
     foundExec=true;++current_;
     if(peek().kind!=TokenKind::Newline&&!atEnd()){error(program.errors,"unexpected token after 'exec'");valid=false;}
     if(peek().kind==TokenKind::Newline)++current_;
     skipCurrentLine=false;
     break;
    }
    bodyTokens.push_back(peek());++current_;
   }
   bodyTokens.push_back({TokenKind::End,"",peek().line,peek().column});
   if(!foundExec){error(program.errors,"expected 'exec' to close the ifit block");valid=false;}
   Parser nested(bodyTokens);Program nestedProgram=nested.parse();
   statement.body=std::move(nestedProgram.statements);
   for(const auto&e:nestedProgram.errors)program.errors.push_back(e);
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
     else if(fs::path(filename).extension()!=ext){error(program.errors,"quoted $shaiiport imports require a .shaii file");valid=false;}
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
    }
    if(valid)valid=consume(TokenKind::Identifier,"finish",program.errors,"expected 'finish' to close the function");
    if(valid&&peek().kind!=TokenKind::Newline&&!atEnd()){error(program.errors,"unexpected token after 'finish'");valid=false;}
   }else{error(program.errors,"expected shaiiport, shazport, or declr after '$'");valid=false;}
  }else if(match(TokenKind::Identifier,"brew")){
   statement.kind=StatementKind::Declare;
   valid=consume(TokenKind::Identifier,"",program.errors,"expected a variable name after 'brew'");
   if(valid)statement.name=previous().text;
   if(valid)valid=consume(TokenKind::Symbol,"=",program.errors,"expected '=' after variable name");
   if(valid){
    if(match(TokenKind::String))statement.value=std::string(1,static_cast<char>(1))+previous().text;
    else if(match(TokenKind::Number)||match(TokenKind::Identifier,"true")||match(TokenKind::Identifier,"false"))statement.value=previous().text;
    else if(match(TokenKind::Symbol,"-")){if(match(TokenKind::Number))statement.value="-"+previous().text;else{error(program.errors,"expected digits after unary '-'");valid=false;}}
    else{error(program.errors,"expected a string, number, or boolean literal");valid=false;}
   }
  }else if(match(TokenKind::Identifier,"instxt")){
   statement.kind=StatementKind::OutputText;valid=consume(TokenKind::Symbol,"-",program.errors,"expected '-' after 'instxt'");
   if(valid)valid=consume(TokenKind::String,"",program.errors,"expected quoted text after 'instxt-'");
   if(valid)statement.value=previous().text;
  }else if(match(TokenKind::Identifier,"insval")){
   statement.kind=StatementKind::OutputValue;valid=consume(TokenKind::Symbol,"-",program.errors,"expected '-' after 'insval'");
   if(valid)valid=consume(TokenKind::Identifier,"",program.errors,"expected a variable or function name after 'insval-'");
   if(valid)statement.name=previous().text;
   if(valid&&match(TokenKind::Symbol,"(")){
    statement.kind=StatementKind::OutputCall;
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
   }
  }else{error(program.errors,"unrecognized Shay statement");valid=false;}
  if(skipCurrentLine&&!atEnd()&&peek().kind!=TokenKind::Newline){error(program.errors,"unexpected token after statement");valid=false;}
  if(valid)program.statements.push_back(statement);
  if(skipCurrentLine)skipLine();
 }
 return program;
}
}
