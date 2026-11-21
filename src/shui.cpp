#include "shui.hpp"
#include <algorithm>
#include <cctype>
#include <set>
#include <sstream>
#include <unordered_set>

namespace shay::ui {
namespace {
class Parser {
public:
 explicit Parser(const std::vector<Token>& tokens):tokens_(tokens){}
 ParseResult run(){
  skip_newlines();
  if(!take(TokenKind::Symbol,"$")){fail(peek(),"expected '$app' at the start of a .shui file");return result_;}
  if(!take(TokenKind::Identifier,"app")){fail(peek(),"expected 'app' after '$'");return result_;}
  if(!take(TokenKind::String)){fail(peek(),"expected a quoted application name after '$app'");return result_;}
  result_.app.name=previous().text;
  if(!take(TokenKind::Symbol,"{")){fail(peek(),"expected '{' after the application name");return result_;}
  skip_newlines();
  while(!end()&&!is(TokenKind::Symbol,"}")){
   if(is(TokenKind::Identifier,"logic"))parse_logic();
   else if(is_node_kind(peek()))result_.app.nodes.push_back(parse_node());
   else{fail(peek(),"expected a UI element or 'logic' inside the application");synchronize();}
   skip_newlines();
  }
  if(!take(TokenKind::Symbol,"}"))fail(peek(),"expected '}' to close the application");
  skip_newlines();
  if(!end())fail(peek(),"unexpected content after the application");
  return result_;
 }
private:
 const std::vector<Token>&tokens_;
 std::size_t at_=0;
 ParseResult result_;
 const Token&peek()const{return tokens_[std::min(at_,tokens_.size()-1)];}
 const Token&previous()const{return tokens_[at_?at_-1:0];}
 bool end()const{return peek().kind==TokenKind::End;}
 bool is(TokenKind kind,const std::string&text={})const{return peek().kind==kind&&(text.empty()||peek().text==text);}
 bool take(TokenKind kind,const std::string&text={}){if(!is(kind,text))return false;++at_;return true;}
 void skip_newlines(){while(take(TokenKind::Newline)){}}
 void fail(const Token&t,const std::string&message){result_.errors.push_back({t.line,t.column,message});}
 void synchronize(){while(!end()&&!is(TokenKind::Newline)&&!is(TokenKind::Symbol,"}"))++at_;skip_newlines();}
 bool is_node_kind(const Token&t)const{
  if(t.kind!=TokenKind::Identifier)return false;
  static const std::unordered_set<std::string> names={"window","panel","row","column","scroll","text","image","separator","button","text_input","checkbox","dropdown"};
  return names.count(t.text)>0;
 }
 NodeKind node_kind(const std::string&name)const{
  if(name=="window")return NodeKind::Window;if(name=="row")return NodeKind::Row;if(name=="column")return NodeKind::Column;
  if(name=="scroll")return NodeKind::Scroll;if(name=="text")return NodeKind::Text;if(name=="image")return NodeKind::Image;
  if(name=="separator")return NodeKind::Separator;if(name=="button")return NodeKind::Button;if(name=="text_input")return NodeKind::TextInput;
  if(name=="checkbox")return NodeKind::Checkbox;if(name=="dropdown")return NodeKind::Dropdown;return NodeKind::Panel;
 }
 void parse_logic(){
  ++at_;skip_newlines();
  if(!take(TokenKind::Symbol,"{")){fail(peek(),"expected '{' after 'logic'");synchronize();return;}
  int depth=1;
  while(!end()&&depth>0){
   if(is(TokenKind::Symbol,"{")){++depth;result_.app.logic_tokens.push_back(peek());++at_;continue;}
   if(is(TokenKind::Symbol,"}")){--depth;if(depth==0){++at_;break;}result_.app.logic_tokens.push_back(peek());++at_;continue;}
   result_.app.logic_tokens.push_back(peek());++at_;
  }
  if(depth!=0)fail(peek(),"expected '}' to close the logic block");
  skip_newlines();
 }
 Node parse_node(){
  Node node;node.line=peek().line;const std::string kind=peek().text;node.kind=node_kind(kind);++at_;
  if(!take(TokenKind::Identifier)){fail(peek(),"expected an element ID after '"+kind+"'");synchronize();return node;}
  node.id=previous().text;
  if(!take(TokenKind::Symbol,"{")){fail(peek(),"expected '{' after element ID '"+node.id+"'");synchronize();return node;}
  skip_newlines();
  while(!end()&&!is(TokenKind::Symbol,"}")){
   if(is_node_kind(peek()))node.children.push_back(parse_node());
   else if(is(TokenKind::Identifier))parse_property(node);
   else{fail(peek(),"expected a property or child element inside '"+node.id+"'");synchronize();}
   skip_newlines();
  }
  if(!take(TokenKind::Symbol,"}"))fail(peek(),"expected '}' to close element '"+node.id+"'");
  skip_newlines();
  return node;
 }
 void parse_property(Node&node){
  const Token key=peek();++at_;
  if(!take(TokenKind::Symbol,":")){fail(peek(),"expected ':' after property '"+key.text+"'");synchronize();return;}
  std::string value;bool first=true;
  while(!end()&&!is(TokenKind::Newline)&&!is(TokenKind::Symbol,"}")){
   const Token part=peek();++at_;
   if(!first&&part.kind==TokenKind::Identifier&&part.text=="x"&&value.size()&&std::isdigit(static_cast<unsigned char>(value.back())))value+="x";
   else if(!first&&(value.empty()||value.back()!='x'))value+=" ";
   if(part.kind==TokenKind::String||part.kind==TokenKind::Number||part.kind==TokenKind::Identifier)value+=part.text;
   else{fail(part,"property values must be strings, numbers, or identifiers");}
   first=false;
  }
  if(value.empty())fail(key,"property '"+key.text+"' requires a value");
  else node.properties.push_back({key.text,value,key.line});
  skip_newlines();
 }
};

std::string escape_html(const std::string&value){
 std::string out;for(char c:value){switch(c){case '&':out+="&amp;";break;case '<':out+="&lt;";break;case '>':out+="&gt;";break;case '"':out+="&quot;";break;case '\'':out+="&#39;";break;default:out+=c;}}return out;
}
const std::string* property(const Node&node,const std::string&name){
 for(const auto&p:node.properties)if(p.name==name)return &p.value;return nullptr;
}
void validate_node(const Node&node,std::set<std::string>&ids,std::vector<ParseError>&errors){
 if(!ids.insert(node.id).second)errors.push_back({node.line,1,"duplicate element ID '"+node.id+"'"});
 static const std::unordered_set<std::string> common={"layout","gap","padding","align","direction","title","size"};
 static const std::unordered_set<std::string> text={"value","style","bind"};
 static const std::unordered_set<std::string> image={"src","alt"};
 static const std::unordered_set<std::string> button={"label","on_click"};
 static const std::unordered_set<std::string> input={"label","bind","placeholder"};
 static const std::unordered_set<std::string> dropdown={"label","bind","options"};
 for(const auto&p:node.properties){
  bool allowed=common.count(p.name)>0;
  if(node.kind==NodeKind::Text)allowed=allowed||text.count(p.name)>0;
  if(node.kind==NodeKind::Image)allowed=allowed||image.count(p.name)>0;
  if(node.kind==NodeKind::Button)allowed=allowed||button.count(p.name)>0;
  if(node.kind==NodeKind::TextInput||node.kind==NodeKind::Checkbox)allowed=allowed||input.count(p.name)>0;
  if(node.kind==NodeKind::Dropdown)allowed=allowed||dropdown.count(p.name)>0;
  if(node.kind==NodeKind::Separator)allowed=allowed||p.name=="orientation";
  if(!allowed)errors.push_back({p.line,1,"unknown property '"+p.name+"' for "+node_kind_name(node.kind)});
 }
 if(node.kind==NodeKind::Text&&!property(node,"value")&&!property(node,"bind"))errors.push_back({node.line,1,"text element '"+node.id+"' requires 'value' or 'bind'"});
 if(node.kind==NodeKind::Button&&!property(node,"label"))errors.push_back({node.line,1,"button '"+node.id+"' requires 'label'"});
 if(node.kind==NodeKind::Button&&!property(node,"on_click"))errors.push_back({node.line,1,"button '"+node.id+"' requires 'on_click'"});
 if(node.kind==NodeKind::Image&&!property(node,"src"))errors.push_back({node.line,1,"image '"+node.id+"' requires 'src'"});
 for(const auto&child:node.children)validate_node(child,ids,errors);
}
void render_node(std::ostringstream&out,const Node&node){
 const auto get=[&](const std::string&key,const std::string&fallback=std::string()){const auto*p=property(node,key);return p?*p:fallback;};
 const std::string id=escape_html(node.id);
 const std::string cls="shui-"+node_kind_name(node.kind);
 if(node.kind==NodeKind::Text){
  const std::string bind=get("bind");out<<"<span id=\""<<id<<"\" class=\""<<cls;
  if(get("style")=="title")out<<" shui-title";
  out<<"\"";if(!bind.empty())out<<" data-shay-bind=\""<<escape_html(bind)<<"\"";
  out<<">"<<escape_html(get("value",bind))<<"</span>";return;
 }
 if(node.kind==NodeKind::Image){out<<"<img id=\""<<id<<"\" class=\""<<cls<<"\" src=\""<<escape_html(get("src"))<<"\" alt=\""<<escape_html(get("alt"))<<"\">";return;}
 if(node.kind==NodeKind::Separator){out<<"<hr id=\""<<id<<"\" class=\""<<cls<<"\">";return;}
 if(node.kind==NodeKind::Button){out<<"<button id=\""<<id<<"\" class=\""<<cls<<"\" data-shay-action=\""<<escape_html(get("on_click"))<<"\">"<<escape_html(get("label"))<<"</button>";return;}
 if(node.kind==NodeKind::TextInput){out<<"<label class=\"shui-field\">"<<escape_html(get("label",node.id))<<"<input id=\""<<id<<"\" data-shay-bind=\""<<escape_html(get("bind"))<<"\" placeholder=\""<<escape_html(get("placeholder"))<<"\"></label>";return;}
 if(node.kind==NodeKind::Checkbox){out<<"<label class=\"shui-field\"><input type=\"checkbox\" id=\""<<id<<"\" data-shay-bind=\""<<escape_html(get("bind"))<<"\">"<<escape_html(get("label",node.id))<<"</label>";return;}
 if(node.kind==NodeKind::Dropdown){out<<"<label class=\"shui-field\">"<<escape_html(get("label",node.id))<<"<select id=\""<<id<<"\" data-shay-bind=\""<<escape_html(get("bind"))<<"\">";
  std::istringstream options(get("options"));std::string option;while(std::getline(options,option,','))out<<"<option>"<<escape_html(option)<<"</option>";out<<"</select></label>";return;}
 const std::string tag=node.kind==NodeKind::Window?"main":"div";
 out<<"<"<<tag<<" id=\""<<id<<"\" class=\""<<cls;
 if(node.kind==NodeKind::Row)out<<" shui-row";
 if(node.kind==NodeKind::Column||node.kind==NodeKind::Window)out<<" shui-column";
 if(!get("layout").empty())out<<" shui-layout-"<<escape_html(get("layout"));
 out<<"\"";
 if(node.kind==NodeKind::Window&&!get("size").empty()){
  const auto x=get("size").find('x');if(x!=std::string::npos)out<<" style=\"width:"<<escape_html(get("size").substr(0,x))<<"px;height:"<<escape_html(get("size").substr(x+1))<<"px\"";
 }
 out<<">";for(const auto&child:node.children)render_node(out,child);out<<"</"<<tag<<">";
}
}
std::string node_kind_name(NodeKind kind){
 switch(kind){case NodeKind::Window:return "window";case NodeKind::Panel:return "panel";case NodeKind::Row:return "row";case NodeKind::Column:return "column";case NodeKind::Scroll:return "scroll";case NodeKind::Text:return "text";case NodeKind::Image:return "image";case NodeKind::Separator:return "separator";case NodeKind::Button:return "button";case NodeKind::TextInput:return "text-input";case NodeKind::Checkbox:return "checkbox";case NodeKind::Dropdown:return "dropdown";}return "unknown";
}
ParseResult parse(const std::vector<Token>&tokens){return Parser(tokens).run();}
std::vector<ParseError> validate(const App&app){
 std::vector<ParseError> errors;if(app.name.empty())errors.push_back({1,1,"application name cannot be empty"});
 std::set<std::string> ids;std::size_t windows=0;for(const auto&node:app.nodes){if(node.kind==NodeKind::Window)++windows;validate_node(node,ids,errors);}
 if(windows==0)errors.push_back({1,1,"application must contain at least one window"});
 if(windows>1)errors.push_back({1,1,"only one root window is currently supported"});
 return errors;
}
std::string render_html(const App&app){
 std::ostringstream out;out<<"<!doctype html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n<title>"<<escape_html(app.name)<<"</title>\n<style>\n";
 out<<"*{box-sizing:border-box}body{margin:0;padding:24px;font:16px system-ui,sans-serif;background:#f3f5f8;color:#172033}.shui-window{margin:auto;max-width:100%;min-height:240px;padding:20px;background:white;border:1px solid #d8deea;border-radius:14px;box-shadow:0 8px 30px #14213d18}.shui-column{display:flex;flex-direction:column;gap:12px}.shui-row{display:flex;flex-direction:row;align-items:center;gap:12px;flex-wrap:wrap}.shui-panel{padding:12px;border:1px solid #e1e5ec;border-radius:10px}.shui-title{font-size:1.5rem;font-weight:700}.shui-button{border:0;border-radius:8px;padding:10px 16px;background:#2458c6;color:white;font:inherit;cursor:pointer}.shui-field{display:flex;gap:8px;align-items:center}.shui-field input,.shui-field select{font:inherit;padding:8px;border:1px solid #cbd3df;border-radius:6px}.shui-image{max-width:100%;height:auto}.shui-separator{width:100%;border:0;border-top:1px solid #d8deea}\n";
 out<<"</style>\n</head>\n<body>\n";for(const auto&node:app.nodes)render_node(out,node);
 out<<"<script>document.addEventListener('click',e=>{const b=e.target.closest('[data-shay-action]');if(b)window.dispatchEvent(new CustomEvent('shay:action',{detail:{name:b.dataset.shayAction}}));});document.addEventListener('input',e=>{if(e.target.dataset.shayBind)window.dispatchEvent(new CustomEvent('shay:input',{detail:{name:e.target.dataset.shayBind,value:e.target.type==='checkbox'?e.target.checked:e.target.value}}));});</script>\n</body>\n</html>\n";
 return out.str();
}
}
