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
 const std::string&source_;const std::unordered_map<std::string,double>&values_;std::size_t position_=0;std::string error_;
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
}
#endif
