#ifndef SHAY_EXPRESSION_HPP
#define SHAY_EXPRESSION_HPP
#include <cctype>
#include <cstdlib>
#include <string>
#include <unordered_map>
namespace shay {
class NumericExpression {
public:
 NumericExpression(const std::string&source,const std::unordered_map<std::string,double>&values):source_(source),values_(values){}
 bool evaluate(double&result,std::string&error){position_=0;error_.clear();result=expression();skip();if(error_.empty()&&position_!=source_.size())fail("unexpected token in expression");error=error_;return error_.empty();}
private:
 std::string source_;const std::unordered_map<std::string,double>&values_;std::size_t position_=0;std::string error_;
 void skip(){while(position_<source_.size()&&std::isspace(static_cast<unsigned char>(source_[position_])))++position_;}
 bool take(char c){skip();if(position_<source_.size()&&source_[position_]==c){++position_;return true;}return false;}
 void fail(const std::string&m){if(error_.empty())error_=m;}
 double expression(){double v=term();while(error_.empty()){if(take('+'))v+=term();else if(take('-'))v-=term();else break;}return v;}
 double term(){double v=factor();while(error_.empty()){if(take('*'))v*=factor();else if(take('/')){double d=factor();if(d==0){fail("division by zero");return 0;}v/=d;}else break;}return v;}
 double factor(){
  skip();if(take('+'))return factor();if(take('-'))return -factor();
  if(take('(')){double v=expression();if(!take(')'))fail("expected ')' in expression");return v;}
  skip();const std::size_t start=position_;
  if(position_<source_.size()&&(std::isdigit(static_cast<unsigned char>(source_[position_]))||source_[position_]=='.')){
   char*end=nullptr;const char*begin=source_.c_str()+position_;double v=std::strtod(begin,&end);if(end==begin){fail("invalid number");return 0;}position_=static_cast<std::size_t>(end-source_.c_str());return v;
  }
  if(position_<source_.size()&&(std::isalpha(static_cast<unsigned char>(source_[position_]))||source_[position_]=='_')){
   while(position_<source_.size()&&(std::isalnum(static_cast<unsigned char>(source_[position_]))||source_[position_]=='_'))++position_;
   const std::string name=source_.substr(start,position_-start);auto it=values_.find(name);if(it==values_.end()){fail("unknown numeric value '"+name+"'");return 0;}return it->second;
  }
  fail("expected a number, parameter, or parenthesized expression");return 0;
 }
};
namespace detail {
inline std::string trim_condition(const std::string&s){
 const auto first=s.find_first_not_of(" \t\r\n");if(first==std::string::npos)return {};
 const auto last=s.find_last_not_of(" \t\r\n");return s.substr(first,last-first+1);
}
inline bool wraps_condition(const std::string&s){
 if(s.size()<2||s.front()!='('||s.back()!=')')return false;
 int depth=0;for(std::size_t i=0;i<s.size();++i){if(s[i]=='(')++depth;else if(s[i]==')')--depth;if(depth==0&&i+1<s.size())return false;if(depth<0)return false;}return depth==0;
}
inline std::size_t find_logic(const std::string&s,const std::string&word){
 int depth=0;
 for(std::size_t i=0;i+word.size()<=s.size();++i){
  if(s[i]=='('){++depth;continue;}if(s[i]==')'){--depth;continue;}
  if(depth==0&&s.compare(i,word.size(),word)==0){
   const bool left=i==0||std::isspace(static_cast<unsigned char>(s[i-1]));
   const std::size_t end=i+word.size();
   const bool right=end==s.size()||std::isspace(static_cast<unsigned char>(s[end]));
   if(left&&right)return i;
  }
 }
 return std::string::npos;
}
inline bool condition(const std::string&raw,const std::unordered_map<std::string,double>&values,bool&result,std::string&error){
 std::string s=trim_condition(raw);
 while(wraps_condition(s))s=trim_condition(s.substr(1,s.size()-2));
 auto pos=find_logic(s,"orit");
 if(pos!=std::string::npos){bool a=false;if(!condition(s.substr(0,pos),values,a,error))return false;if(a){result=true;return true;}bool b=false;if(!condition(s.substr(pos+4),values,b,error))return false;result=b;return true;}
 pos=find_logic(s,"andit");
 if(pos!=std::string::npos){bool a=false;if(!condition(s.substr(0,pos),values,a,error))return false;if(!a){result=false;return true;}bool b=false;if(!condition(s.substr(pos+5),values,b,error))return false;result=b;return true;}
 if(s.rfind("notit ",0)==0){if(!condition(s.substr(6),values,result,error))return false;result=!result;return true;}
 const std::string ops[]={">=","<=","==","!=",">","<","="};
 for(const auto&op:ops){
  int depth=0;
  for(std::size_t i=0;i+op.size()<=s.size();++i){
   if(s[i]=='('){++depth;continue;}if(s[i]==')'){--depth;continue;}
   if(depth==0&&s.compare(i,op.size(),op)==0){
    double left=0,right=0;NumericExpression a(s.substr(0,i),values),b(s.substr(i+op.size()),values);
    if(!a.evaluate(left,error)){error="left operand '"+s.substr(0,i)+"': "+error;return false;}
    if(!b.evaluate(right,error)){error="right operand '"+s.substr(i+op.size())+"': "+error;return false;}
    if(op==">")result=left>right;else if(op=="<")result=left<right;else if(op==">=")result=left>=right;else if(op=="<=")result=left<=right;else if(op=="=="||op=="=")result=left==right;else result=left!=right;
    return true;
   }
  }
 }
 error="expected a comparison operator (>, <, >=, <=, ==, =, !=)";return false;
}
}
inline bool evaluate_condition(const std::string&condition,const std::unordered_map<std::string,double>&values,bool&result,std::string&error){
 return detail::condition(condition,values,result,error);
}
}
#endif
