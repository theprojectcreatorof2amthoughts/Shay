#ifndef SHAY_AST_HPP
#define SHAY_AST_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace shay {
enum class StatementKind { ImportLibrary, ImportTask, Declare, OutputText, OutputValue, Invalid };
struct Statement {
    StatementKind kind = StatementKind::Invalid;
    std::size_t line = 1;
    std::string name;
    std::string value;
};
struct ParseError {
    std::size_t line;
    std::string message;
};
struct Program {
    std::vector<Statement> statements;
    std::vector<ParseError> errors;
};
}
#endif
