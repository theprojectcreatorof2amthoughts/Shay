#include "lexer.hpp"
#include <cctype>
#include <stdexcept>
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
  if(c=='!'&&peek(1)=='#'){take();take();tokens.push_back({TokenKind::Symbol,"!#",line_,column_-2});continue;}
  if(c=='#'&&(peek(1)=='#'||peek(1)=='%')){while(peek()!='\0'&&peek()!='\n')take();continue;}
  const auto startLine=line_,startColumn=column_;
  if(c=='"'){
   take();std::string value;bool closed=false;
   while(peek()!='\0'){
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
  // Base-prefixed numeric literals use Shay's explicit ::...:: delimiters.
  // Normalize them to decimal here so every downstream consumer (compiler,
  // interpreter, declarations, collections, and expression parsing) sees a
  // normal numeric token.
  if(c==':'&&peek(1)==':'){
   take();take();
   std::string digits;
   while(peek()!='\0'&&peek()!=':'&&peek()!='\n')digits.push_back(take());
   if(peek()==':'&&peek(1)==':'){
    take();take();
    try{
     if(digits.size()>2&&digits[0]=='0'&&(digits[1]=='b'||digits[1]=='B')){
      const std::string bits=digits.substr(2);
      if(bits.empty()||bits.find_first_not_of("01")!=std::string::npos)throw std::invalid_argument("binary");
      unsigned long long value=std::stoull(bits,nullptr,2);
      tokens.push_back({TokenKind::Number,std::to_string(value),startLine,startColumn,true});
     }else if(digits.size()>2&&digits[0]=='0'&&(digits[1]=='x'||digits[1]=='X')){
      const std::string hex=digits.substr(2);
      if(hex.empty()||hex.find_first_not_of("0123456789abcdefABCDEF")!=std::string::npos)throw std::invalid_argument("hexadecimal");
      unsigned long long value=std::stoull(hex,nullptr,16);
      tokens.push_back({TokenKind::Number,std::to_string(value),startLine,startColumn,true});
     }else{
      errors_.push_back({startLine,startColumn,"expected a binary (::0b...::) or hexadecimal (::0x...::) literal"});
      tokens.push_back({TokenKind::Invalid,digits,startLine,startColumn});
     }
    }catch(...){
     errors_.push_back({startLine,startColumn,"invalid or out-of-range binary/hexadecimal numeric literal"});
     tokens.push_back({TokenKind::Invalid,digits,startLine,startColumn});
    }
   }else{
    errors_.push_back({startLine,startColumn,"unterminated base-prefixed numeric literal; expected closing '::'"});
    tokens.push_back({TokenKind::Invalid,digits,startLine,startColumn});
   }
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
  if(c=='>'||c=='<'||c=='='||c=='!'){
   std::string op(1,take());if(peek()=='=')op.push_back(take());
   if(op=="!"||op==">"||op=="<"||op=="="||op==">="||op=="<="||op=="=="||op=="!="){
    tokens.push_back({TokenKind::Symbol,op,startLine,startColumn});continue;
   }
  }
  if(c=='/'&&peek(1)=='>'){take();take();tokens.push_back({TokenKind::Symbol,"/>",startLine,startColumn});continue;}
  if(c==static_cast<char>(36)||c=='-'||c=='['||c==']'||c=='.'||c=='+'||c=='*'||c=='/'||c=='('||c==')'||c==','||c=='{'||c=='}'||c==':'){
   tokens.push_back({TokenKind::Symbol,std::string(1,take()),startLine,startColumn});continue;
  }
  take();errors_.push_back({startLine,startColumn,std::string("unexpected character '")+c+"'"});tokens.push_back({TokenKind::Invalid,std::string(1,c),startLine,startColumn});
 }
 tokens.push_back({TokenKind::End,"",line_,column_});return tokens;
}
}
