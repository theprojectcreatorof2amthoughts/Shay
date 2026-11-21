#ifndef SHAY_SHUI_LANGUAGE_HPP
#define SHAY_SHUI_LANGUAGE_HPP
#include <cstddef>
#include <string>
#include <vector>
namespace shay::ui::language {
struct Diagnostic { std::size_t line = 1; std::string message; };
struct Result {
    std::vector<Diagnostic> diagnostics;
    bool ok() const { return diagnostics.empty(); }
};
struct AdapterConfig {
    std::string platform;
    std::string method;
    std::string language;
};
Result validate_source(const std::string& source, const std::string& extension);
bool inspect_adapter(const std::string& source, AdapterConfig& config, Result& result);
bool generate_output(const std::string& app_source, const AdapterConfig& adapter,
                     const std::string& app_name, std::string& output,
                     std::string& output_extension, Result& result);
}
#endif
