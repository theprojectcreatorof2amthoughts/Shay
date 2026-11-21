#ifndef SHAY_JSON_HPP
#define SHAY_JSON_HPP
#include <cstddef>
#include <string>
namespace shay::json {
class Parser {
public:
 explicit Parser(const std::string& source):source_(source){}
 bool compact(std::string& output,std::string& error){
  output_.clear();error_.clear();skip();
  if(!value())return fail(output,error);
  skip();if(at_!=source_.size()){set_error("unexpected characters after JSON value");return fail(output,error);}
  output=output_;return true;
 }
private:
 const std::string&source_;std::size_t at_=0;std::string output_,error_;
 void skip(){while(at_<source_.size()&&(source_[at_]==' '||source_[at_]=='\t'||source_[at_]=='\r'||source_[at_]=='\n'))++at_;}
 void set_error(const std::string&m){if(error_.empty())error_=m+" at byte "+std::to_string(at_);}
 bool fail(std::string&out,std::string&error){out.clear();error=error_;return false;}
 bool take(char c){skip();if(at_<source_.size()&&source_[at_]==c){++at_;return true;}return false;}
 bool string_value(){
  if(at_>=source_.size()||source_[at_]!='"'){set_error("expected a JSON string");return false;}
  const std::size_t start=at_++;
  while(at_<source_.size()){
   const unsigned char c=static_cast<unsigned char>(source_[at_++]);
   if(c=='"'){output_.append(source_,start,at_-start);return true;}
   if(c<0x20){set_error("unescaped control character in JSON string");return false;}
   if(c=='\\'){
    if(at_>=source_.size()){set_error("unfinished escape sequence");return false;}
    const char e=source_[at_++];
    if(e=='"'||e=='\\'||e=='/'||e=='b'||e=='f'||e=='n'||e=='r'||e=='t')continue;
    if(e=='u'){
     for(int i=0;i<4;++i){if(at_>=source_.size()||!hex(source_[at_])){set_error("invalid Unicode escape");return false;}++at_;}
     continue;
    }
    set_error("invalid escape sequence");return false;
   }
  }
  set_error("unterminated JSON string");return false;
 }
 static bool hex(char c){return(c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F');}
 bool number(){
  const std::size_t start=at_;
  if(at_<source_.size()&&source_[at_]=='-')++at_;
  if(at_>=source_.size()){set_error("incomplete JSON number");return false;}
  if(source_[at_]=='0')++at_;
  else if(source_[at_]>='1'&&source_[at_]<='9'){while(at_<source_.size()&&source_[at_]>='0'&&source_[at_]<='9')++at_;}
  else{set_error("invalid JSON number");return false;}
  if(at_<source_.size()&&source_[at_]=='.'){
   ++at_;const auto digits=at_;while(at_<source_.size()&&source_[at_]>='0'&&source_[at_]<='9')++at_;
   if(at_==digits){set_error("fraction requires at least one digit");return false;}
  }
  if(at_<source_.size()&&(source_[at_]=='e'||source_[at_]=='E')){
   ++at_;if(at_<source_.size()&&(source_[at_]=='+'||source_[at_]=='-'))++at_;
   const auto digits=at_;while(at_<source_.size()&&source_[at_]>='0'&&source_[at_]<='9')++at_;
   if(at_==digits){set_error("exponent requires at least one digit");return false;}
  }
  output_.append(source_,start,at_-start);return true;
 }
 bool literal(const char*word){const std::string w(word);if(source_.compare(at_,w.size(),w)!=0){set_error("invalid JSON literal");return false;}output_+=w;at_+=w.size();return true;}
 bool value(){
  skip();if(at_>=source_.size()){set_error("expected a JSON value");return false;}
  const char c=source_[at_];
  if(c=='"')return string_value();
  if(c=='{' ){
   ++at_;output_+='{';skip();if(take('}')){output_+='}';return true;}
   for(;;){skip();if(!string_value())return false;if(!take(':')){set_error("expected ':' after object key");return false;}output_+=':';if(!value())return false;skip();if(take('}')){output_+='}';return true;}if(!take(',')){set_error("expected ',' or '}' in object");return false;}output_+=',';}
  }
  if(c=='['){
   ++at_;output_+='[';skip();if(take(']')){output_+=']';return true;}
   for(;;){if(!value())return false;skip();if(take(']')){output_+=']';return true;}if(!take(',')){set_error("expected ',' or ']' in array");return false;}output_+=',';}
  }
  if(c=='t')return literal("true");if(c=='f')return literal("false");if(c=='n')return literal("null");
  if(c=='-'||(c>='0'&&c<='9'))return number();
  set_error("unexpected character in JSON value");return false;
 }
};
inline bool compact(const std::string&source,std::string&output,std::string&error){return Parser(source).compact(output,error);}
}
#endif
