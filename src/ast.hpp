#ifndef SHAY_AST_HPP
#define SHAY_AST_HPP
#include <cstddef>
#include <string>
#include <utility>
#include <vector>
namespace shay {
enum class StatementKind { ImportLibrary, ImportTask, Declare, InputVariable, OutputText, OutputTextExit, MarkFailure, OutputValue, ListAppend, ListSet, ListRemove, MapSet, MapRemove, SetAdd, SetRemove, SetContains, ThrowError, TryCatch, FileRead, FileWrite, FileAppend, FileExists, FileDelete, DirectoryCreate, DirectoryExists, ReturnValue, DeclareFunction, DeclareClass, DeclareMethod, DoFunction, ExecuteClass, OutputCall, If, LogFileExtension, LogFileName, LogText, Invalid };
struct Statement {
    StatementKind kind = StatementKind::Invalid;
    std::size_t line = 1;
    std::string name, value, condition, type_annotation, error_category, path;
    std::vector<std::string> parameters, arguments, output_names;
    std::vector<Statement> body, else_body;
    bool has_return = false;
    bool dynamic_value = false;
    bool external_import = false;
    bool list_value = false;
    bool map_value = false;
    bool set_value = false;
    bool output_keyed = false;
    std::string output_key;
    bool output_indexed = false;
    std::size_t list_index = 0;
    std::vector<std::string> list_items;
    std::vector<std::pair<std::string,std::string>> map_items;
};
struct ParseError { std::size_t line; std::size_t column; std::string message; };
struct Program { std::vector<Statement> statements; std::vector<ParseError> errors; };
}
#endif
