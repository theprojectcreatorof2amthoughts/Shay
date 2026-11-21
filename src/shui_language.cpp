#include "shui_language.hpp"
#include <algorithm>
#include <cctype>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace shay::ui::language {
namespace {
std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r");
    return value.substr(first, last - first + 1);
}
bool matches(const std::string& value, const char* pattern) {
    return std::regex_match(value, std::regex(pattern));
}
void error(Result& result, std::size_t line, const std::string& message) {
    result.diagnostics.push_back({line, message});
}
std::vector<std::pair<std::size_t, std::string>> lines_of(const std::string& source) {
    std::vector<std::pair<std::size_t, std::string>> lines;
    std::istringstream input(source);
    std::string line;
    std::size_t number = 0;
    while (std::getline(input, line)) lines.emplace_back(++number, trim(line));
    return lines;
}
std::string capture(const std::string& source, const char* pattern) {
    std::smatch match;
    if (std::regex_search(source, match, std::regex(pattern)) && match.size() > 1)
        return match[1].str();
    return {};
}
std::string html_escape(const std::string& value) {
    std::string out;
    for (char c : value) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out += c; break;
        }
    }
    return out;
}
std::string csharp_escape(const std::string& value) {
    std::string out;
    for (char c : value) {
        if (c == '\\' || c == '"') out += '\\';
        if (c == '\n') { out += "\\n"; continue; }
        if (c == '\r') { out += "\\r"; continue; }
        out += c;
    }
    return out;
}
struct Element {
    std::string kind;
    std::string id;
    std::string metadata_key;
    std::string metadata_value;
};
std::vector<Element> elements_of(const std::string& source) {
    std::vector<Element> elements;
    const std::regex pattern(R"(\*#drawui\s+<'([^']+)'>\s*(?:<<'([^']+)'>>)?\s*(?:<:([A-Za-z_][A-Za-z0-9_]*)=([A-Za-z0-9_-]+):>)?)");
    std::istringstream input(source);
    std::string line;
    std::size_t index = 0;
    while (std::getline(input, line)) {
        std::smatch match;
        if (!std::regex_search(line, match, pattern)) continue;
        Element element;
        element.kind = match[1].str();
        element.id = match[2].matched ? match[2].str() : "shay-element-" + std::to_string(++index);
        if (match[3].matched) element.metadata_key = match[3].str();
        if (match[4].matched) element.metadata_value = match[4].str();
        elements.push_back(std::move(element));
    }
    return elements;
}
Result validate_shui(const std::string& source) {
    Result result;
    const auto lines = lines_of(source);
    bool entry = false, app = false, app_closed = false, do_this = false, ui_finish = false;
    bool in_class = false, in_method = false;
    std::set<std::string> classes, ids;
    for (const auto& [number, line] : lines) {
        if (line.empty()) continue;
        if (!entry) {
            if (!matches(line, R"(\$useentry\s+UI\([A-Za-z_][A-Za-z0-9_]*\))")
                error(result, number, "expected '$useentry UI(Target)' before the UI app");
            else entry = true;
            continue;
        }
        if (!app) {
            if (line != "$browsElementapp <{")
                error(result, number, "expected '$browsElementapp <{' after the UI entry");
            else app = true;
            continue;
        }
        if (app_closed) {
            if (!do_this && line == "$dothis") { do_this = true; continue; }
            if (do_this && line == "$uiFinish") { ui_finish = true; continue; }
            error(result, number, "unexpected content after the UI app body");
            continue;
        }
        if (line == "}>") {
            if (in_method) error(result, number, "UI method is missing 'finish'");
            if (in_class) error(result, number, "UI class is missing 'finish'");
            app_closed = true;
            continue;
        }
        if (matches(line, R"(\*#uiContrl\s+"[^"]+\.shuil")")) continue;
        std::smatch match;
        if (std::regex_match(line, match, std::regex(R"(\*!#classUideclr\s+(!u#[A-Za-z_][A-Za-z0-9_]*))"))) {
            if (in_class) error(result, number, "UI classes cannot be nested");
            const auto name = match[1].str();
            if (!classes.insert(name).second) error(result, number, "duplicate UI class '" + name + "'");
            in_class = true;
            continue;
        }
        if (std::regex_match(line, match, std::regex(R"(\*!#doUiclass\s+<"(!u#[A-Za-z_][A-Za-z0-9_]*)">)"))) {
            if (classes.count(match[1].str()) == 0)
                error(result, number, "unknown UI class '" + match[1].str() + "'");
            continue;
        }
        if (in_class && line == "finish") {
            if (in_method) in_method = false;
            else in_class = false;
            continue;
        }
        if (in_class && matches(line, R"(\$uiSet\s+[A-Za-z0-9_]+)")) continue;
        if (in_class && matches(line, R"(\$declrmthd\s+[A-Za-z_][A-Za-z0-9_]*)")) {
            if (in_method) error(result, number, "a UI method is already open");
            in_method = true;
            continue;
        }
        if (in_method) continue;
        if (matches(line, R"(\*#drawui\s+<'[A-Za-z_][A-Za-z0-9_-]*'>\s*(<<'[A-Za-z0-9_-]+'>>|<:[A-Za-z_][A-Za-z0-9_]*=[A-Za-z0-9_-]+:>)?)")) {
            std::smatch id_match;
            if (std::regex_search(line, id_match, std::regex(R"(<<'([A-Za-z0-9_-]+)'>>)"))) {
                if (!ids.insert(id_match[1].str()).second)
                    error(result, number, "duplicate UI element ID '" + id_match[1].str() + "'");
            }
            continue;
        }
        error(result, number, "unrecognized .shui directive or malformed UI element");
    }
    if (!entry) error(result, 1, "missing '$useentry UI(Target)'");
    if (!app) error(result, 1, "missing '$browsElementapp <{'");
    if (!app_closed) error(result, lines.empty() ? 1 : lines.back().first, "missing '}> to close the UI app body");
    if (app_closed && !do_this) error(result, lines.empty() ? 1 : lines.back().first, "missing '$dothis' after the UI app body");
    if (do_this && !ui_finish) error(result, lines.empty() ? 1 : lines.back().first, "missing '$uiFinish' after '$dothis'");
    return result;
}
Result validate_shuil(const std::string& source) {
    Result result;
    const auto lines = lines_of(source);
    bool platform = false, method = false, language = false, translator_class = false;
    bool in_method = false, translator_loaded = false, translator_run = false;
    std::string selected_method;
    for (const auto& [number, line] : lines) {
        if (line.empty()) continue;
        std::smatch match;
        if (std::regex_match(line, match, std::regex(R"(#\*\$ContrlDefine\s+<([A-Za-z][A-Za-z0-9_-]*)>)"))) {
            if (platform) error(result, number, "platform target is declared more than once");
            platform = true; continue;
        }
        if (std::regex_match(line, match, std::regex(R"(#\*!\MethdUi\s+<([A-Za-z][A-Za-z0-9_-]*)>)"))) {
            if (method) error(result, number, "translation method is declared more than once");
            method = true; selected_method = match[1].str(); continue;
        }
        if (matches(line, R"(#\*!\LangUI\s+<[A-Za-z][A-Za-z0-9_-]*>)")) {
            if (language) error(result, number, "UI language is declared more than once");
            language = true; continue;
        }
        if (matches(line, R"(#\*!\classmethdDeclr\s+!m#[A-Za-z_][A-Za-z0-9_]*)")) {
            if (translator_class) error(result, number, "translator class is declared more than once");
            translator_class = true; continue;
        }
        if (translator_class && line == "finish") {
            if (in_method) in_method = false;
            else translator_class = false;
            continue;
        }
        if (translator_class && matches(line, R"(\$declrmthd\s+[A-Za-z_][A-Za-z0-9_]*)")) {
            if (in_method) error(result, number, "a translator method is already open");
            in_method = true; continue;
        }
        if (translator_class && matches(line, R"(\$uiset\s+[A-Za-z0-9_]+)")) continue;
        if (translator_class && matches(line, R"(\$mthdset\s+[A-Za-z0-9_]+)")) continue;
        if (in_method && matches(line, R"(#\*\$(javasport|csharpsport)\s+(\[[A-Za-z0-9_./-]+\]|\["[^"]+\.(js|cs)"\]))")) {
            translator_loaded = true; continue;
        }
        if (in_method && (line == "#*$doJavaScript" || line == "#*$doCSharp")) {
            if (!translator_loaded) error(result, number, "load the platform translator before running it");
            translator_run = true; continue;
        }
        if (in_method) continue;
        error(result, number, "unrecognized .shuil directive or malformed translator declaration");
    }
    if (!platform) error(result, 1, "missing '#*$ContrlDefine <platform>'");
    if (!method) error(result, 1, "missing '#*!MethdUi <translation-method>'");
    if (!language) error(result, 1, "missing '#*!LangUI <language>'");
    if (!translator_class) error(result, lines.empty() ? 1 : lines.back().first, "missing '#*!classmethdDeclr !m#Name' translator class");
    if (translator_class) error(result, lines.empty() ? 1 : lines.back().first, "translator class is missing its closing 'finish'");
    if (in_method) error(result, lines.empty() ? 1 : lines.back().first, "translator method is missing its closing 'finish'");
    if (!translator_loaded) error(result, 1, "no platform translator file was loaded");
    if (!translator_run) error(result, 1, "missing platform translator execution directive");
    if (selected_method == "javasTrans" && !source.empty() && source.find("#*$doJavaScript") == std::string::npos)
        error(result, 1, "javasTrans requires '#*$doJavaScript'");
    if (selected_method == "csharpTrans" && !source.empty() && source.find("#*$doCSharp") == std::string::npos)
        error(result, 1, "csharpTrans requires '#*$doCSharp'");
    if (selected_method != "javasTrans" && selected_method != "csharpTrans" && !selected_method.empty())
        error(result, 1, "unsupported translation method '" + selected_method + "'");
    return result;
}
}
Result validate_source(const std::string& source, const std::string& extension) {
    if (extension == ".shui") return validate_shui(source);
    if (extension == ".shuil") return validate_shuil(source);
    Result result; error(result, 1, "expected a .shui or .shuil file"); return result;
}
bool inspect_adapter(const std::string& source, AdapterConfig& config, Result& result) {
    result = validate_shuil(source);
    config.platform = capture(source, R"(#\*\$ContrlDefine\s+<([A-Za-z][A-Za-z0-9_-]*)>)");
    config.method = capture(source, R"(#\*!\MethdUi\s+<([A-Za-z][A-Za-z0-9_-]*)>)");
    config.language = capture(source, R"(#\*!\LangUI\s+<([A-Za-z][A-Za-z0-9_-]*)>)");
    return result.ok();
}
bool generate_output(const std::string& app_source, const AdapterConfig& adapter,
                     const std::string& app_name, std::string& output,
                     std::string& output_extension, Result& result) {
    const auto elements = elements_of(app_source);
    if (adapter.platform == "browser" && adapter.method == "javasTrans") {
        output_extension = ".html";
        std::ostringstream html;
        html << "<!doctype html>\n<html lang=\"" << html_escape(adapter.language) << "\">\n<head>\n"
             << "<meta charset=\"utf-8\">\n<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
             << "<title>" << html_escape(app_name) << "</title>\n<style>\n"
             << "body{font-family:system-ui,sans-serif;margin:0;padding:2rem;background:#f3f5f8;color:#172033}"
             << ".shay-ui{display:flex;flex-wrap:wrap;gap:1rem;align-items:flex-start}"
             << ".shay-square{width:120px;height:120px;background:#dce5f2;border:1px solid #9aabc1;border-radius:8px}"
             << ".shay-buttoncircle{width:112px;height:112px;border:0;border-radius:50%;background:#315b9a;color:white;font-weight:600;cursor:pointer}"
             << ".shay-label{padding:.6rem}.shay-textbox{padding:.65rem;border:1px solid #9aabc1;border-radius:6px}"
             << ".shay-checkbox{width:20px;height:20px}\n</style>\n</head>\n<body>\n<h1>"
             << html_escape(app_name) << "</h1>\n<main class=\"shay-ui\">\n";
        std::size_t index = 0;
        for (const auto& element : elements) {
            std::string tag, css, text;
            if (element.kind == "square") { tag = "div"; css = "shay-square"; }
            else if (element.kind == "buttoncircle") { tag = "button"; css = "shay-buttoncircle"; text = "buttoncircle"; }
            else if (element.kind == "label") { tag = "span"; css = "shay-label"; text = "label"; }
            else if (element.kind == "textbox") { tag = "input"; css = "shay-textbox"; }
            else if (element.kind == "checkbox") { tag = "input"; css = "shay-checkbox"; }
            else {
                error(result, ++index, "unsupported browser UI element kind '" + element.kind + "'");
                continue;
            }
            html << "<" << tag << " id=\"" << html_escape(element.id) << "\" class=\"" << css << "\"";
            if (element.kind == "textbox") html << " type=\"text\"";
            if (element.kind == "checkbox") html << " type=\"checkbox\"";
            if (element.metadata_key == "intercative" && element.metadata_value == "yes")
                html << " data-intercative=\"yes\"";
            if (tag == "input") html << " />";
            else html << ">" << text << "</" << tag << ">";
            html << "\n";
        }
        html << "</main>\n</body>\n</html>\n";
        output = html.str();
        return result.ok();
    }
    if (adapter.platform == "windows" && adapter.method == "csharpTrans") {
        output_extension = ".cs";
        std::ostringstream cs;
        cs << "using System;\nusing System.Drawing;\nusing System.Windows.Forms;\n\n"
           << "namespace ShayGeneratedUI\n{\n"
           << "    internal sealed class MainForm : Form\n    {\n"
           << "        public MainForm()\n        {\n"
           << "            Text = \"" << csharp_escape(app_name) << "\";\n"
           << "            StartPosition = FormStartPosition.CenterScreen;\n"
           << "            ClientSize = new Size(640, 420);\n";
        std::size_t index = 0;
        for (const auto& element : elements) {
            const std::string id = csharp_escape(element.id);
            const int top = 24 + static_cast<int>(index) * 138;
            std::string type, init;
            if (element.kind == "square") {
                type = "Panel";
                init = "BackColor = Color.LightSteelBlue";
            } else if (element.kind == "buttoncircle") {
                type = "Button";
                init = "Text = \"buttoncircle\", FlatStyle = FlatStyle.Flat, BackColor = Color.RoyalBlue, ForeColor = Color.White";
            } else if (element.kind == "label") {
                type = "Label";
                init = "Text = \"label\", AutoSize = true";
            } else if (element.kind == "textbox") {
                type = "TextBox";
                init = "Text = \"\"";
            } else if (element.kind == "checkbox") {
                type = "CheckBox";
                init = "Text = \"checkbox\", AutoSize = true";
            } else {
                error(result, index + 1, "unsupported Windows UI element kind '" + element.kind + "'");
                ++index;
                continue;
            }
            const int width = element.kind == "buttoncircle" ? 112 : (element.kind == "square" ? 120 : 180);
            const int height = element.kind == "buttoncircle" || element.kind == "square" ? 112 : 32;
            cs << "            var control" << index << " = new " << type << "\n"
               << "            {\n                Name = \"" << id << "\",\n"
               << "                " << init << ",\n"
               << "                Location = new Point(24, " << top << "),\n"
               << "                Size = new Size(" << width << ", " << height << ")\n"
               << "            };\n"
               << "            Controls.Add(control" << index << ");\n";
            ++index;
        }
        cs << "        }\n    }\n\n"
           << "    internal static class Program\n    {\n"
           << "        [STAThread]\n        private static void Main()\n        {\n"
           << "            Application.EnableVisualStyles();\n"
           << "            Application.SetCompatibleTextRenderingDefault(false);\n"
           << "            Application.Run(new MainForm());\n"
           << "        }\n    }\n}\n";
        output = cs.str();
        return result.ok();
    }
    error(result, 1, "adapter target/method combination is not implemented (supported: browser/javasTrans, windows/csharpTrans)");
    return false;
}
}
