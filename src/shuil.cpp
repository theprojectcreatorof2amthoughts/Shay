#include "shuil.hpp"
#include <cctype>
#include <sstream>
#include <string>
#include <vector>
#include <utility>

namespace shay::ui::library {
namespace {
std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r");
    return value.substr(first, last - first + 1);
}
bool identifier(const std::string& value) {
    if (value.empty() || !(std::isalpha(static_cast<unsigned char>(value[0])) || value[0] == '_')) return false;
    for (char c : value) if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) return false;
    return true;
}
bool wrapped(const std::string& line, const std::string& prefix, std::string& value) {
    if (line.rfind(prefix, 0) != 0 || line.size() <= prefix.size() + 2) return false;
    if (line[prefix.size()] != '<' || line.back() != '>') return false;
    value = trim(line.substr(prefix.size() + 1, line.size() - prefix.size() - 2));
    return !value.empty();
}
bool binary_setting(const std::string& line, const std::string& name, int& value) {
    const std::string prefix = "$" + name + " ";
    if (line.rfind(prefix, 0) != 0) return false;
    const std::string raw = trim(line.substr(prefix.size()));
    if (raw != "0" && raw != "1") return false;
    value = raw == "1" ? 1 : 0;
    return true;
}
void add_error(ParseResult& result, std::size_t line, const std::string& message) {
    result.errors.push_back({line, 1, message});
}
}

ParseResult parse(const std::string& source) {
    ParseResult result;
    std::vector<std::pair<std::size_t, std::string>> lines;
    std::istringstream input(source);
    std::string raw;
    std::size_t line_number = 0;
    while (std::getline(input, raw)) {
        ++line_number;
        std::string line = trim(raw);
        if (line.empty() || line.rfind("##", 0) == 0 || line.rfind("#%", 0) == 0) continue;
        lines.emplace_back(line_number, line);
    }
    std::size_t at = 0;
    auto next = [&](const std::string& expected, const std::string& message) -> bool {
        if (at >= lines.size() || lines[at].second != expected) {
            add_error(result, at < lines.size() ? lines[at].first : line_number + 1, message);
            return false;
        }
        ++at;
        return true;
    };
    auto next_wrapped = [&](const std::string& prefix, std::string& value, const std::string& message) -> bool {
        if (at >= lines.size() || !wrapped(lines[at].second, prefix, value)) {
            add_error(result, at < lines.size() ? lines[at].first : line_number + 1, message);
            return false;
        }
        ++at;
        return true;
    };

    if (!next_wrapped("#*$ContrlDefine ", result.config.platform, "expected '#*$ContrlDefine <platform>'")) return result;
    if (!next_wrapped("#*!MethdUi ", result.config.translation_method, "expected '#*!MethdUi <translation-method>'")) return result;
    if (!next_wrapped("#*!LangUI ", result.config.language, "expected '#*!LangUI <language>'")) return result;
    for (char c : result.config.language) {
        if (!(std::isalpha(static_cast<unsigned char>(c)) || c == '-')) {
            add_error(result, lines[at - 1].first, "UI language must contain only letters and optional hyphens");
            return result;
        }
    }
    if (at >= lines.size() || lines[at].second.rfind("#*!classmethdDeclr ", 0) != 0) {
        add_error(result, at < lines.size() ? lines[at].first : line_number + 1,
                  "expected '#*!classmethdDeclr !m#ClassName'");
        return result;
    }
    {
        const std::string prefix = "#*!classmethdDeclr ";
        const std::string name = trim(lines[at].second.substr(prefix.size()));
        if (name.rfind("!m#", 0) != 0 || !identifier(name.substr(3))) {
            add_error(result, lines[at].first, "UI library class names must use the '!m#' prefix");
            return result;
        }
        result.config.class_name = name;
        ++at;
    }

    int ui_set = -1;
    if (at >= lines.size() || !binary_setting(lines[at].second, "uiset", ui_set)) {
        add_error(result, at < lines.size() ? lines[at].first : line_number + 1,
                  "expected '$uiset 0' or '$uiset 1' inside the UI library class");
        return result;
    }
    result.config.ui_set = ui_set;
    ++at;

    while (at < lines.size() && lines[at].second.rfind("$declrmthd ", 0) == 0) {
        Method method;
        method.name = trim(lines[at].second.substr(std::string("$declrmthd ").size()));
        if (!identifier(method.name)) {
            add_error(result, lines[at].first, "expected a valid method name after '$declrmthd'");
            return result;
        }
        ++at;
        int method_set = -1;
        if (at >= lines.size() || !binary_setting(lines[at].second, "mthdset", method_set)) {
            add_error(result, at < lines.size() ? lines[at].first : line_number + 1,
                      "expected '$mthdset 0' or '$mthdset 1' inside the translation method");
            return result;
        }
        method.method_set = method_set;
        ++at;

        const std::string source_prefix = "#*$javasport [";
        if (at >= lines.size() || lines[at].second.rfind(source_prefix, 0) != 0 || lines[at].second.back() != ']') {
            add_error(result, at < lines.size() ? lines[at].first : line_number + 1,
                      "expected '#*$javasport [uicontrol.js]' or '#*$javasport [\"external.js\"]'");
            return result;
        }
        std::string file = trim(lines[at].second.substr(source_prefix.size(), lines[at].second.size() - source_prefix.size() - 1));
        if (file.size() >= 2 && file.front() == '"' && file.back() == '"') {
            method.external_javascript = true;
            file = file.substr(1, file.size() - 2);
        } else if (file != "uicontrol.js") {
            add_error(result, lines[at].first, "the integrated JavaScript translator must be named 'uicontrol.js'; quote external filenames");
            return result;
        }
        if (file.empty()) {
            add_error(result, lines[at].first, "JavaScript translator filename cannot be empty");
            return result;
        }
        method.javascript_file = file;
        ++at;
        if (!next("#*$doJavaScript", "expected '#*$doJavaScript' after the JavaScript translator source")) return result;
        if (!next("finish", "expected 'finish' to close the translation method")) return result;
        method.execute_javascript = true;
        result.config.methods.push_back(std::move(method));
    }

    if (result.config.methods.empty()) {
        add_error(result, at < lines.size() ? lines[at].first : line_number + 1,
                  "UI library class must declare at least one '$declrmthd' method");
        return result;
    }
    if (!next("finish", "expected 'finish' to close the UI library class")) return result;
    if (at != lines.size()) add_error(result, lines[at].first, "unexpected content after the UI library class");
    return result;
}
}
