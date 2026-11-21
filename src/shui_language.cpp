#include "shui_language.hpp"
#include <algorithm>
#include <cctype>
#include <iomanip>
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
    std::string entry_target;
    bool in_class = false, in_method = false;
    std::set<std::string> classes, ids;
    for (const auto& [number, line] : lines) {
        if (line.empty()) continue;
        if (!entry) {
            if (matches(line, R"(brew\s+.+)") ||
                matches(line, R"(\$shaiiport\s+.+)") ||
                matches(line, R"(\$shazport\s+.+)"))
                continue;
            if (!matches(line, R"(\$useentry\s+UI\([A-Za-z_][A-Za-z0-9_]*\))"))
                error(result, number, "expected Shay declarations/imports or '$useentry UI(Target)' before the UI app");
            else {
                std::smatch target_match;
                if (std::regex_search(line, target_match, std::regex(R"REGEX(UI\(([A-Za-z_][A-Za-z0-9_]*)\))REGEX")))
                    entry_target = target_match[1].str();
                entry = true;
            }
            continue;
        }
        if (!app) {
            const std::string expected = entry_target == "Windows" ? "!%#csElementapp <{" : "!%#browsElementapp <{";
            if (line != expected)
                error(result, number, "expected '" + expected + "' after the UI entry");
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
        if (std::regex_match(line, match, std::regex(R"REGEX(\*!#doUiclass\s+<"(!u#[A-Za-z_][A-Za-z0-9_]*)">)REGEX"))) {
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
        if (matches(line, R"(\*#drawui\s+<'[A-Za-z_][A-Za-z0-9_-]*'>\s*(?:<<'[A-Za-z0-9_-]+'>>)?\s*(?:<:[A-Za-z_][A-Za-z0-9_]*=[A-Za-z0-9_-]+:>)?)")) {
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
    bool platform = false, method = false, language = false;
    bool translator_class_seen = false, translator_class_open = false;
    bool in_method = false, translator_loaded = false, translator_run = false;
    std::string selected_method, loaded_translator;
    for (const auto& [number, line] : lines) {
        if (line.empty()) continue;
        std::smatch match;
        if (std::regex_match(line, match, std::regex(R"(#\*\$ContrlDefine\s+<([A-Za-z][A-Za-z0-9_-]*)>)"))) {
            if (platform) error(result, number, "platform target is declared more than once");
            platform = true; continue;
        }
        if (std::regex_match(line, match, std::regex(R"(#\*!MethdUi\s+<([A-Za-z][A-Za-z0-9_-]*)>)"))) {
            if (method) error(result, number, "translation method is declared more than once");
            method = true; selected_method = match[1].str(); continue;
        }
        if (matches(line, R"(#\*!LangUI\s+<[A-Za-z][A-Za-z0-9_-]*>)")) {
            if (language) error(result, number, "UI language is declared more than once");
            language = true; continue;
        }
        if (matches(line, R"(#\*!classmethdDeclr\s+!m#[A-Za-z_][A-Za-z0-9_]*)")) {
            if (translator_class_seen) error(result, number, "translator class is declared more than once");
            translator_class_seen = true; translator_class_open = true; continue;
        }
        if (translator_class_open && line == "finish") {
            if (in_method) in_method = false;
            else translator_class_open = false;
            continue;
        }
        if (translator_class_open && matches(line, R"(\$declrmthd\s+[A-Za-z_][A-Za-z0-9_]*)")) {
            if (in_method) error(result, number, "a translator method is already open");
            in_method = true; continue;
        }
        if (translator_class_open && matches(line, R"(\$uiset\s+[A-Za-z0-9_]+)")) continue;
        if (in_method && matches(line, R"(\$mthdset\s+[A-Za-z0-9_]+)")) continue;
        if (in_method && std::regex_match(line, match, std::regex(R"REGEX(#\*\$(javasport|csharpsport)\s+(\[[A-Za-z0-9_./-]+\]|\["[^"]+\.(js|cs)"\]))REGEX"))) {
            translator_loaded = true; loaded_translator = match[1].str(); continue;
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
    if (!translator_class_seen) error(result, lines.empty() ? 1 : lines.back().first, "missing '#*!classmethdDeclr !m#Name' translator class");
    if (translator_class_open) error(result, lines.empty() ? 1 : lines.back().first, "translator class is missing its closing 'finish'");
    if (in_method) error(result, lines.empty() ? 1 : lines.back().first, "translator method is missing its closing 'finish'");
    if (!translator_loaded) error(result, 1, "no platform translator file was loaded");
    if (!translator_run) error(result, 1, "missing platform translator execution directive");
    if (selected_method == "javasTrans" && source.find("#*$doJavaScript") == std::string::npos)
        error(result, 1, "javasTrans requires '#*$doJavaScript'");
    if (selected_method == "csharpTrans" && source.find("#*$doCSharp") == std::string::npos)
        error(result, 1, "csharpTrans requires '#*$doCSharp'");
    if (selected_method == "javasTrans" && loaded_translator != "javasport")
        error(result, 1, "javasTrans must load its translator with '#*$javasport'");
    if (selected_method == "csharpTrans" && loaded_translator != "csharpsport")
        error(result, 1, "csharpTrans must load its translator with '#*$csharpsport'");
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
    config.method = capture(source, R"(#\*!MethdUi\s+<([A-Za-z][A-Za-z0-9_-]*)>)");
    config.language = capture(source, R"(#\*!LangUI\s+<([A-Za-z][A-Za-z0-9_-]*)>)");
    if ((config.platform == "browser" && config.method != "javasTrans") ||
        (config.platform == "windows" && config.method != "csharpTrans") ||
        (config.platform != "browser" && config.platform != "windows"))
        error(result, 1, "platform and translation method do not match a supported backend");
    return result.ok();
}
bool generate_output(const std::string& app_source, const AdapterConfig& adapter,
                     const std::string& app_name, std::string& output,
                     std::string& output_extension, Result& result,
                     const std::string& runtime_token) {
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
            if (element.metadata_key == "onclick")
                html << " data-shay-onclick=\"" << html_escape(element.metadata_value) << "\"";
            if (element.metadata_key == "bind")
                html << " data-shay-bind=\"" << html_escape(element.metadata_value) << "\"";
            if (tag == "input") html << " />";
            else html << ">" << text << "</" << tag << ">";
            html << "\n";
        }
        html << "</main>\n";
        if (!runtime_token.empty()) {
            html << "<pre id=\"shay-output\" role=\"status\" aria-live=\"polite\"></pre>\n"
                 << "<script>\n(() => {\n"
                 << "const token=" << std::quoted(runtime_token) << ";\n"
                 << "const base='http://127.0.0.1:18765';\n"
                 << "const output=document.getElementById('shay-output');\n"
                 << "async function sync(){try{const r=await fetch(base+'/state?token='+encodeURIComponent(token));if(!r.ok)return;const state=await r.json();document.querySelectorAll('[data-shay-bind]').forEach(el=>{const key=el.dataset.shayBind;if(!(key in state))return;if(el.type==='checkbox')el.checked=state[key]==='true';else if(document.activeElement!==el)el.value=state[key];});}catch(_){}}\n"
                 << "document.querySelectorAll('[data-shay-onclick]').forEach(el=>el.addEventListener('click',async()=>{try{const r=await fetch(base+'/event',{method:'POST',headers:{'Content-Type':'text/plain'},body:token+'|'+el.dataset.shayOnclick});const value=await r.text();if(output)output.textContent=value;await sync();}catch(_){if(output)output.textContent='Shay runtime is not running. Start the compiled app first.';}}));\n"
                 << "document.querySelectorAll('[data-shay-bind]').forEach(el=>el.addEventListener(el.type==='checkbox'?'change':'input',async()=>{const value=el.type==='checkbox'?String(el.checked):el.value;try{await fetch(base+'/set',{method:'POST',headers:{'Content-Type':'text/plain'},body:token+'|'+el.dataset.shayBind+'='+encodeURIComponent(value)});}catch(_){}}));\n"
                 << "sync();\n})();\n</script>\n";
        }
        html << "</body>\n</html>\n";
        output = html.str();
        return result.ok();
    }
    if (adapter.platform == "windows" && adapter.method == "csharpTrans") {
        output_extension = ".cs";
        std::ostringstream cs;
        cs << "using System;\nusing System.Drawing;\nusing System.Diagnostics;\nusing System.IO;\nusing System.Net.Http;\nusing System.Text.Json;\nusing System.Threading.Tasks;\nusing System.Windows.Forms;\n\n"
           << "namespace ShayGeneratedUI\n{\n"
           << "    internal sealed class MainForm : Form\n    {\n"
           << "        private static readonly HttpClient ShayClient = new HttpClient();\n"
           << "        private const string ShayToken = " << std::quoted(runtime_token) << ";\n"
           << "        private bool _syncing;\n"
           << "        public MainForm()\n        {\n"
           << "            Text = \"" << csharp_escape(app_name) << "\";\n"
           << "            StartPosition = FormStartPosition.CenterScreen;\n"
           << "            ClientSize = new Size(640, 420);\n";
        std::size_t index = 0;
        for (const auto& element : elements) {
            const std::string id = csharp_escape(element.id);
            const int left = 24 + static_cast<int>(index % 2) * 210;
            const int top = 24 + static_cast<int>(index / 2) * 140;
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
               << "                Location = new Point(" << left << ", " << top << "),\n"
               << "                Size = new Size(" << width << ", " << height << ")\n"
               << "            };\n"
               << "            Controls.Add(control" << index << ");\n";
            if (element.metadata_key == "bind") {
                cs << "            control" << index << ".Tag = " << std::quoted(element.metadata_value) << ";\n";
                if (element.kind == "textbox")
                    cs << "            control" << index << ".TextChanged += async (_, __) => { if (!_syncing) await SendStateAsync(" << std::quoted(element.metadata_value) << ", control" << index << ".Text); };\n";
                else if (element.kind == "checkbox")
                    cs << "            control" << index << ".CheckedChanged += async (_, __) => { if (!_syncing) await SendStateAsync(" << std::quoted(element.metadata_value) << ", control" << index << ".Checked.ToString().ToLowerInvariant()); };\n";
            }
            if (element.metadata_key == "onclick" && element.kind == "buttoncircle") {
                cs << "            control" << index << ".Click += async (_, __) => { try { var response = await ShayClient.PostAsync(\"http://127.0.0.1:18765/event\", new StringContent(ShayToken + \"|\" + " << std::quoted(element.metadata_value) << ")); var message = await response.Content.ReadAsStringAsync(); if (!String.IsNullOrWhiteSpace(message)) MessageBox.Show(message, \"Shay\"); await SyncStateAsync(); } catch (Exception error) { MessageBox.Show(error.Message, \"Shay UI bridge\"); } };\n";
            }
            ++index;
        }
        cs << "            Shown += async (_, __) => await SyncStateAsync();\n"
           << "        }\n"
           << "        private async Task SendStateAsync(string name, string value)\n        {\n"
           << "            try { var body = ShayToken + \"|\" + Uri.EscapeDataString(name) + \"=\" + Uri.EscapeDataString(value); await ShayClient.PostAsync(\"http://127.0.0.1:18765/set\", new StringContent(body)); } catch { }\n"
           << "        }\n"
           << "        private async Task SyncStateAsync()\n        {\n"
           << "            JsonDocument? document = null;\n"
           << "            for (int attempt = 0; attempt < 30 && document == null; ++attempt) { try { var json = await ShayClient.GetStringAsync(\"http://127.0.0.1:18765/state?token=\" + Uri.EscapeDataString(ShayToken)); document = JsonDocument.Parse(json); } catch { await Task.Delay(100); } }\n"
           << "            if (document == null) return;\n"
           << "            try { _syncing = true; foreach (Control control in Controls) { if (control.Tag is string name && document.RootElement.TryGetProperty(name, out var item)) { var value = item.GetString() ?? \"\"; if (control is TextBox box && !box.Focused) box.Text = value; else if (control is CheckBox check) check.Checked = value == \"true\"; else if (control is Label label) label.Text = value; } } } finally { _syncing = false; document.Dispose(); }\n"
           << "        }\n"
           << "    }\n\n"
           << "    internal static class Program\n    {\n"
           << "        [STAThread]\n        private static void Main()\n        {\n"
           << "            Application.EnableVisualStyles();\n"
           << "            Application.SetCompatibleTextRenderingDefault(false);\n"
           << "            var runtimePath = Path.Combine(AppContext.BaseDirectory, " << std::quoted(app_name + ".exe") << ");\n"
           << "            if (!File.Exists(runtimePath)) { MessageBox.Show(\"Missing compiled Shay runtime: \" + runtimePath); return; }\n"
           << "            var runtime = Process.Start(new ProcessStartInfo(runtimePath) { UseShellExecute = false, CreateNoWindow = true });\n"
           << "            try { Application.Run(new MainForm()); }\n"
           << "            finally { if (runtime != null && !runtime.HasExited) runtime.Kill(true); }\n"
           << "        }\n    }\n}\n";
        output = cs.str();
        return result.ok();
    }
    error(result, 1, "adapter target/method combination is not implemented (supported: browser/javasTrans, windows/csharpTrans)");
    return false;
}
}
