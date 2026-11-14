#include "lexer.hpp"
#include <cctype>
namespace shay {
Lexer::Lexer(const std::string &source) : source_(source) {}
char Lexer::peek(std::size_t ahead) const { const auto at=offset_+ahead; return at<source_.size()?source_[at]:'\0'; }
char Lexer::take() { if(offset_>=source_.size()) return '\0'; const char c=source_[offset_++]; if(c=='\n'){++line_;column_=1;}else ++column_; return c; }
void Lexer::error(const std::string &message) { errors_.push_back({line_,column_,message}); }
const std::vector<LexError> &Lexer::errors() const { return errors_; }
std::vector<Token> Lexer::scan() {
 std::vector<Token> tokens;
 while(offset_<source_.size()) {
  const char c=peek();
  if(c==' '||c=='\t'||c=='\r'){take();continue;}
  if(c=='\n'){const auto l=line_,col=column_;take();tokens.push_back({TokenKind::Newline,"\n",l,col});continue;}
  if(c=='#'&&peek(1)=='#'){while(peek()!='\0'&&peek()!='\n')take();continue;}
  const auto startLine=line_,startColumn=column_;
  if(c=='"'){
   take();std::string value;bool closed=false;
   while(peek()!='\0'&&peek()!='\n'){
    char ch=take();if(ch=='"'){closed=true;break;}
    if(ch=='\\'){const char escaped=take();switch(escaped){
     case 'n':value.push_back('\n');break;case 't':value.push_back('\t');break;case 'r':value.push_back('\r');break;
     case '\\':value.push_back('\\');break;case '"':value.push_back('"');break;
     default:error(std::string("unsupported string escape \\") + escaped + "'");value.push_back(escaped);break;
    }}else value.push_back(ch);
   }
   if(!closed){errors_.push_back({startLine,startColumn,"unterminated string literal"});tokens.push_back({TokenKind::Invalid,value,startLine,startColumn});}
   else tokens.push_back({TokenKind::String,value,startLine,startColumn});
   continue;
  }
  if(std::isalpha(static_cast<unsigned char>(c))||c=='_'){
   std::string word;while(std::isalnum(static_cast<unsigned char>(peek()))||peek()=='_')word.push_back(take());
   tokens.push_back({TokenKind::Identifier,word,startLine,startColumn});continue;
  }
  if(std::isdigit(static_cast<unsigned char>(c))){
   std::string number;bool dot=false;while(std::isdigit(static_cast<unsigned char>(peek()))||(!dot&&peek()=='.')){if(peek()=='.')dot=true;number.push_back(take());}
   tokens.push_back({TokenKind::Number,number,startLine,startColumn});continue;
  }
  if(c=='$'||c=='-'||c=='='||c=='['||c==']'||c=='.'||c=='+'||c=='*'||c=='/'||c=='('||c==')'||c==','){
   tokens.push_back({TokenKind::Symbol,std::string(1,take()),startLine,startColumn});continue;
  }
  take();errors_.push_back({startLine,startColumn,std::string("unexpected character '")+c+"'"});tokens.push_back({TokenKind::Invalid,std::string(1,c),startLine,startColumn});
 }
 tokens.push_back({TokenKind::End,"",line_,column_});return tokens;
}
}
