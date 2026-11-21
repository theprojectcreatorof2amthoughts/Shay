#ifndef SHAY_SHUI_HPP
#define SHAY_SHUI_HPP

#include "lexer.hpp"
#include <string>
#include <vector>

namespace shay::ui {
enum class NodeKind { Window, Panel, Row, Column, Scroll, Text, Image, Separator, Button, TextInput, Checkbox, Dropdown };
struct Property {
    std::string name;
    std::string value;
    std::size_t line = 1;
};
struct Node {
    NodeKind kind = NodeKind::Panel;
    std::string id;
    std::vector<Property> properties;
    std::vector<Node> children;
    std::size_t line = 1;
};
struct App {
    std::string name;
    std::vector<Node> nodes;
    std::vector<Token> logic_tokens;
};
struct ParseError {
    std::size_t line = 1;
    std::size_t column = 1;
    std::string message;
};
struct ParseResult {
    App app;
    std::vector<ParseError> errors;
    bool ok() const { return errors.empty(); }
};
ParseResult parse(const std::vector<Token>& tokens);
std::vector<ParseError> validate(const App& app);
std::string render_html(const App& app);
std::string node_kind_name(NodeKind kind);
}

#endif
