#ifndef SHAY_SHUIL_HPP
#define SHAY_SHUIL_HPP

#include <string>
#include <vector>

namespace shay::ui::library {
struct Method {
    std::string name;
    int method_set = 0;
    std::string javascript_file;
    bool external_javascript = false;
    bool execute_javascript = false;
};
struct Config {
    std::string platform;
    std::string translation_method;
    std::string language;
    std::string class_name;
    int ui_set = 0;
    std::vector<Method> methods;
};
struct ParseError {
    std::size_t line = 1;
    std::size_t column = 1;
    std::string message;
};
struct ParseResult {
    Config config;
    std::vector<ParseError> errors;
    bool ok() const { return errors.empty(); }
};

ParseResult parse(const std::string& source);
}

#endif
