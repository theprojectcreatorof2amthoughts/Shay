#include "parser.hpp"

namespace shay {
Parser::Parser(const std::vector<Token> &tokens) : tokens_(tokens) {}
const Token &Parser::peek() const { return tokens_[current_]; }
const Token &Parser::previous() const { return tokens_[current_ - 1]; }
bool Parser::atEnd() const { return peek().kind == TokenKind::End; }
bool Parser::match(TokenKind kind, const std::string &text) {
    if (peek().kind != kind || (!text.empty() && peek().text != text)) return false;
    ++current_;
    return true;
}
void Parser::skipLine() {
    while (!atEnd() && peek().kind != TokenKind::Newline) ++current_;
    if (peek().kind == TokenKind::Newline) ++current_;
}
void Parser::error(std::vector<ParseError> &errors, const std::string &message) {
    errors.push_back({peek().line, message});
}
bool Parser::consume(TokenKind kind, const std::string &text, std::vector<ParseError> &errors,
                     const std::string &message) {
    if (match(kind, text)) return true;
    error(errors, message);
    return false;
}

Program Parser::parse() {
    Program program;
    while (!atEnd()) {
        if (match(TokenKind::Newline)) continue;
        const auto line = peek().line;
        Statement statement;
        statement.line = line;
        bool valid = true;

        if (match(TokenKind::Symbol, "$")) {
            if (match(TokenKind::Identifier, "shaiiport")) {
                statement.kind = StatementKind::ImportLibrary;
                valid = consume(TokenKind::String, "", program.errors, "expected a quoted .shaii filename");
                if (valid) statement.value = previous().text;
            } else if (match(TokenKind::Identifier, "shazport")) {
                statement.kind = StatementKind::ImportTask;
                valid = consume(TokenKind::String, "", program.errors, "expected a quoted .shaz filename");
                if (valid) statement.value = previous().text;
            } else {
                error(program.errors, "expected shaiiport or shazport after '$'");
                valid = false;
            }
        } else if (match(TokenKind::Identifier, "brew")) {
            statement.kind = StatementKind::Declare;
            valid = consume(TokenKind::Identifier, "", program.errors, "expected a variable name after 'brew'");
            if (valid) statement.name = previous().text;
            if (valid) valid = consume(TokenKind::Symbol, "=", program.errors, "expected '=' after variable name");
            if (valid) {
                if (match(TokenKind::String)) {
                    statement.value = std::string("\\x01") + previous().text;
                } else if (match(TokenKind::Number) || match(TokenKind::Identifier, "true") ||
                           match(TokenKind::Identifier, "false")) {
                    statement.value = previous().text;
                } else if (match(TokenKind::Symbol, "-")) {
                    if (match(TokenKind::Number)) statement.value = "-" + previous().text;
                    else {
                        error(program.errors, "expected digits after unary '-'");
                        valid = false;
                    }
                } else {
                    error(program.errors, "expected a string, number, or boolean literal");
                    valid = false;
                }
            }
        } else if (match(TokenKind::Identifier, "instxt")) {
            statement.kind = StatementKind::OutputText;
            valid = consume(TokenKind::Symbol, "-", program.errors, "expected '-' after 'instxt'");
            if (valid) valid = consume(TokenKind::String, "", program.errors, "expected quoted text after 'instxt-'");
            if (valid) statement.value = previous().text;
        } else if (match(TokenKind::Identifier, "insval")) {
            statement.kind = StatementKind::OutputValue;
            valid = consume(TokenKind::Symbol, "-", program.errors, "expected '-' after 'insval'");
            if (valid) valid = consume(TokenKind::Identifier, "", program.errors, "expected a variable name after 'insval-'");
            if (valid) statement.name = previous().text;
        } else {
            error(program.errors, "unrecognized Shay statement");
            valid = false;
        }

        if (!atEnd() && peek().kind != TokenKind::Newline) {
            error(program.errors, "unexpected token after statement");
            valid = false;
        }
        if (valid) program.statements.push_back(statement);
        skipLine();
    }
    return program;
}
}
