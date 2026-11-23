#ifndef SHAY_SHUI_SOURCE_HPP
#define SHAY_SHUI_SOURCE_HPP

#include "shui_language.hpp"
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace shay::ui::language {
struct SourceDiagnostic {
    std::size_t line = 1;
    std::string message;
};

// Convert the UI envelope into the ordinary Shay statements that belong to
// the existing runtime. UI declarations are retained as validated metadata;
// platform rendering/event dispatch is deliberately handled by the UI bridge.
inline bool prepare_shui_source(const std::filesystem::path& input,
                                const std::string& source,
                                std::string& shay_source,
                                std::vector<SourceDiagnostic>& diagnostics) {
    const Result app_result = validate_source(source, ".shui");
    for (const auto& item : app_result.diagnostics)
        diagnostics.push_back({item.line, item.message});
    if (!app_result.ok()) return false;

    std::smatch adapter_match;
    const std::regex adapter_pattern(R"REGEX(\*#uiContrl\s+"([^"]+\.shuil)")REGEX");
    if (!std::regex_search(source, adapter_match, adapter_pattern)) {
        diagnostics.push_back({1, "missing *#uiContrl adapter reference"});
        return false;
    }

    const std::filesystem::path adapter_path =
        input.parent_path() / adapter_match[1].str();
    std::ifstream adapter_file(adapter_path, std::ios::binary);
    if (!adapter_file) {
        diagnostics.push_back({1, "cannot open platform adapter '" +
                                   adapter_path.string() + "'"});
        return false;
    }
    std::ostringstream adapter_buffer;
    adapter_buffer << adapter_file.rdbuf();
    AdapterConfig adapter;
    Result adapter_result;
    inspect_adapter(adapter_buffer.str(), adapter, adapter_result);
    for (const auto& item : adapter_result.diagnostics)
        diagnostics.push_back({item.line, item.message});

    const std::string expected_envelope =
        adapter.platform == "windows" ? "!%#csElementapp <{" : "!%#browsElementapp <{";
    const std::string unexpected_envelope =
        adapter.platform == "windows" ? "!%#browsElementapp <{" : "!%#csElementapp <{";
    if (source.find(unexpected_envelope) != std::string::npos)
        diagnostics.push_back({1, "UI app envelope does not match its .shuil adapter"});
    if (source.find(expected_envelope) == std::string::npos)
        diagnostics.push_back({1, "missing UI app envelope for the selected .shuil adapter"});
    if (adapter.method == "mozTrans") {
        if (adapter.source_file.empty()) {
            diagnostics.push_back({1, "Mouz adapter must declare a .mozf source with '#*$mozfsport [file.mozf]'"});
        } else {
            const std::filesystem::path mouz_source = adapter_path.parent_path() / adapter.source_file;
            if (!std::filesystem::is_regular_file(mouz_source))
                diagnostics.push_back({1, "cannot open Mouz source '" + mouz_source.string() + "'"});
        }
    }
    if (!adapter_result.ok() || adapter.platform != expected_platform)
        return false;

    std::istringstream lines(source);
    std::string line;
    std::ostringstream translated;
    const std::regex ui_class(
        R"REGEX(\*!#classUideclr\s+!u#([A-Za-z_][A-Za-z0-9_]*))REGEX");
    const std::regex ui_do_class(
        R"REGEX(\*!#doUiclass\s+<"!u#([A-Za-z_][A-Za-z0-9_]*)">)REGEX");
    const std::regex ignored_directive(
        R"REGEX((\$useentry\s+[a-z]+|!%#(brows|cs)Elementapp\s+<\{|\}>|\*#uiContrl\s+".+\.shuil"|\*#drawui\b.*|\$uiSet\s+\S+|\$dothis|\$uiFinish))REGEX");
    while (std::getline(lines, line)) {
        const std::string directive = std::regex_replace(
            line, std::regex(R"(^\s+|\s+$)"), "");
        std::smatch match;
        if (std::regex_match(directive, match, ui_class)) {
            translated << "$classfunc !" << match[1].str() << '\n';
        } else if (std::regex_match(directive, match, ui_do_class)) {
            translated << "$doclass !" << match[1].str() << '\n';
        } else if (std::regex_match(directive, ignored_directive)) {
            translated << '\n';
        } else {
            translated << line << '\n';
        }
    }
    shay_source = translated.str();
    return true;
}
}

#endif
