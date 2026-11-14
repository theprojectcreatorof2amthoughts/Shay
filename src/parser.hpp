#ifndef SHAY_PARSER_HPP
#define SHAY_PARSER_HPP

#include "ast.hpp"
#include "lexer.hpp"

namespace shay {
class Parser {
public:
    explicit Parser(const std::vector<Token> &tokens);
    Program parse();
private:
    const std::vector<Token> &tokens_;
    std::size_t current_ = 0;
    const Token &peek() const;
    const Token &previous() const;
    bool match(TokenKind kind, const std::string &text = "");
    bool atEnd() const;
    void skipLine();
    void error(std::vector<ParseError> &errors, const std::string &message);
    bool consume(TokenKind kind, const std::string &text, std::vector<ParseError> &errors,
                 const std::string &message);
};
}
#endif
