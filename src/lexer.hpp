#ifndef SHAY_LEXER_HPP
#define SHAY_LEXER_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace shay {
enum class TokenKind { Identifier, Number, String, Symbol, Newline, End, Invalid };
struct Token {
    TokenKind kind;
    std::string text;
    std::size_t line;
    std::size_t column;
};
struct LexError {
    std::size_t line;
    std::size_t column;
    std::string message;
};
class Lexer {
public:
    explicit Lexer(const std::string &source);
    std::vector<Token> scan();
    const std::vector<LexError> &errors() const;
private:
    const std::string &source_;
    std::size_t offset_ = 0;
    std::size_t line_ = 1;
    std::size_t column_ = 1;
    std::vector<LexError> errors_;
    char peek(std::size_t ahead = 0) const;
    char take();
    void error(const std::string &message);
};
}
#endif
