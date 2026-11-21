#include "shui_language.hpp"
#include <algorithm>
#include <cctype>
#include <regex>
#include <set>
#include <sstream>
#include <string_view>

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
    for (const auto& [number, line] : lines) {
        if (line.empty()) continue;
        if (matches(line, R"(#\*\$ContrlDefine\s+<[A-Za-z][A-Za-z0-9_-]*>)")) {
            if (platform) error(result, number, "platform target is declared more than once");
            platform = true; continue;
        }
        if (matches(line, R"(#\*!MethdUi\s+<[A-Za-z][A-Za-z0-9_-]*>)")) {
            if (method) error(result, number, "translation method is declared more than once");
            method = true; continue;
        }
        if (matches(line, R"(#\*!LangUI\s+<[A-Za-z][A-Za-z0-9_-]*>)")) {
            if (language) error(result, number, "UI language is declared more than once");
            language = true; continue;
        }
        if (matches(line, R"(#\*!classmethdDeclr\s+!m#[A-Za-z_][A-Za-z0-9_]*)")) {
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
        if (in_method && matches(line, R"(#\*\$javasport\s+(\[[A-Za-z0-9_./-]+\]|\["[^"]+\.js"\]))")) {
            translator_loaded = true; continue;
        }
        if (in_method && line == "#*$doJavaScript") {
            if (!translator_loaded) error(result, number, "load the JavaScript translator before running it");
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
    if (!translator_loaded) error(result, 1, "no JavaScript translator file was loaded");
    if (!translator_run) error(result, 1, "missing '#*$doJavaScript' execution directive");
    return result;
}
}
Result validate_source(const std::string& source, const std::string& extension) {
    if (extension == ".shui") return validate_shui(source);
    if (extension == ".shuil") return validate_shuil(source);
    Result result; error(result, 1, "expected a .shui or .shuil file"); return result;
}
}
