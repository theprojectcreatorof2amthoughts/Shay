#ifndef SHAY_AST_HPP
#define SHAY_AST_HPP
#include <cstddef>
#include <string>
#include <vector>
namespace shay {
enum class StatementKind { ImportLibrary, ImportTask, Declare, InputVariable, OutputText, OutputTextExit, MarkFailure, OutputValue, ReturnValue, DeclareFunction, OutputCall, If, Invalid };
struct Statement {
    StatementKind kind = StatementKind::Invalid;
    std::size_t line = 1;
    std::string name, value, condition;
    std::vector<std::string> parameters, arguments;
    std::vector<Statement> body, else_body;
    bool has_return = false;
    bool external_import = false;
};
struct ParseError { std::size_t line; std::string message; };
struct Program { std::vector<Statement> statements; std::vector<ParseError> errors; };
}
#endif
