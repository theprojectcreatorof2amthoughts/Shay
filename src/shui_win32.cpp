#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "shui_win32.hpp"
#include <algorithm>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace shay::ui {
namespace {
std::unordered_map<int,std::string> action_by_control;
int next_control_id=1000;
std::wstring wide(const std::string&value){
 if(value.empty())return {};
 const int size=MultiByteToWideChar(CP_UTF8,0,value.data(),static_cast<int>(value.size()),nullptr,0);
 std::wstring result(static_cast<std::size_t>(size),L'\0');
 MultiByteToWideChar(CP_UTF8,0,value.data(),static_cast<int>(value.size()),result.data(),size);
 return result;
}
const std::string* prop(const Node&node,const std::string&name){
 for(const auto&p:node.properties)if(p.name==name)return &p.value;
 return nullptr;
}
std::string value(const Node&node,const std::string&name,const std::string&fallback={}){
 const auto*p=prop(node,name);return p?*p:fallback;
}
struct Rect{int x,y,width,height;};
void add_control(HWND parent,const Node&node,const Rect&r){
 const int id=next_control_id++;
 DWORD style=WS_CHILD|WS_VISIBLE|WS_TABSTOP;
 const wchar_t*klass=L"STATIC";
 std::wstring caption=wide(value(node,"value",value(node,"label",value(node,"alt"))));
 DWORD ex=0;
 switch(node.kind){
  case NodeKind::Text:klass=L"STATIC";style|=SS_LEFT;break;
  case NodeKind::Button:klass=L"BUTTON";style|=BS_PUSHBUTTON;caption=wide(value(node,"label"));break;
  case NodeKind::TextInput:klass=L"EDIT";style|=ES_AUTOHSCROLL;ex=WS_EX_CLIENTEDGE;caption=wide(value(node,"value"));break;
  case NodeKind::Checkbox:klass=L"BUTTON";style|=BS_AUTOCHECKBOX;caption=wide(value(node,"label",node.id));break;
  case NodeKind::Dropdown:klass=L"COMBOBOX";style|=CBS_DROPDOWNLIST|WS_VSCROLL;ex=WS_EX_CLIENTEDGE;caption=L"";break;
  case NodeKind::Separator:klass=L"STATIC";style|=SS_ETCHEDHORZ;caption=L"";break;
  case NodeKind::Image:klass=L"STATIC";style|=SS_CENTER;break;
  default:return;
 }
 HWND control=CreateWindowExW(ex,klass,caption.c_str(),style,r.x,r.y,r.width,r.height,parent,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
 if(!control)return;
 SendMessageW(control,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);
 if(node.kind==NodeKind::Button){
  const auto action=value(node,"on_click");if(!action.empty())action_by_control[id]=action;
 }
 if(node.kind==NodeKind::Dropdown){
  std::istringstream options(value(node,"options"));std::string option;
  while(std::getline(options,option,','))SendMessageW(control,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(wide(option).c_str()));
  SendMessageW(control,CB_SETCURSEL,0,0);
 }
}
void layout_nodes(HWND parent,const std::vector<Node>&nodes,const Rect&area,bool horizontal){
 if(nodes.empty())return;
 const int gap=10;
 const int available=horizontal?area.width-gap*static_cast<int>(nodes.size()-1):area.height-gap*static_cast<int>(nodes.size()-1);
 const int cell=std::max(24,available/static_cast<int>(nodes.size()));
 int offset=0;
 for(const auto&node:nodes){
  Rect r=horizontal?Rect{area.x+offset,area.y,cell,32}:Rect{area.x,area.y+offset,area.width,32};
  if(node.kind==NodeKind::Row||node.kind==NodeKind::Column){
   layout_nodes(parent,node.children,{r.x,r.y,r.width,horizontal?cell:r.height},node.kind==NodeKind::Row);
  }else if(node.kind==NodeKind::Panel||node.kind==NodeKind::Scroll){
   const int panelHeight=std::max(80,static_cast<int>(node.children.size())*42+16);
   HWND panel=CreateWindowExW(WS_EX_CLIENTEDGE,L"STATIC",L"",WS_CHILD|WS_VISIBLE|SS_ETCHEDFRAME,r.x,r.y,area.width,panelHeight,parent,nullptr,GetModuleHandleW(nullptr),nullptr);
   if(panel)layout_nodes(panel,node.children,{8,8,area.width-16,panelHeight-16},false);
   r.height=panelHeight;
  }else{
   if(node.kind==NodeKind::Separator)r.height=12;
   add_control(parent,node,r);
  }
  offset+=(horizontal?cell:std::max(cell,r.height))+gap;
 }
}
LRESULT CALLBACK window_proc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam){
 switch(message){
  case WM_COMMAND:{
   if(HIWORD(wparam)==BN_CLICKED){
    const auto found=action_by_control.find(LOWORD(wparam));
    if(found!=action_by_control.end()){
     const std::wstring debug=L"Shay UI action event: "+wide(found->second)+L"\n";
     OutputDebugStringW(debug.c_str());
    }
   }
   return 0;
  }
  case WM_DESTROY:PostQuitMessage(0);return 0;
  default:return DefWindowProcW(hwnd,message,wparam,lparam);
 }
}
}
int run_windows(const App&app){
 action_by_control.clear();next_control_id=1000;
 const wchar_t*className=L"ShayUIWindow";
 WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.lpfnWndProc=window_proc;wc.hInstance=GetModuleHandleW(nullptr);
 wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);wc.lpszClassName=className;
 if(!RegisterClassExW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS){MessageBoxW(nullptr,L"Could not register the Shay UI window class.",L"Shay UI",MB_ICONERROR);return 1;}
 int width=720,height=520;std::string title=app.name;
 if(!app.nodes.empty()){
  if(const auto*p=prop(app.nodes.front(),"size")){const auto x=p->find('x');if(x!=std::string::npos){try{width=std::stoi(p->substr(0,x));height=std::stoi(p->substr(x+1));}catch(...){}}}
  title=value(app.nodes.front(),"title",title);
 }
 HWND hwnd=CreateWindowExW(0,className,wide(title).c_str(),WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,width,height,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 if(!hwnd){MessageBoxW(nullptr,L"Could not create the Shay UI window.",L"Shay UI",MB_ICONERROR);return 1;}
 RECT client{};GetClientRect(hwnd,&client);
 for(const auto&node:app.nodes)if(node.kind==NodeKind::Window){
  const bool horizontal=value(node,"layout")=="row";
  layout_nodes(hwnd,node.children,{16,16,client.right-32,client.bottom-32},horizontal);
 }
 ShowWindow(hwnd,SW_SHOW);UpdateWindow(hwnd);
 MSG message{};while(GetMessageW(&message,nullptr,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}
 return static_cast<int>(message.wParam);
}
}
#endif
