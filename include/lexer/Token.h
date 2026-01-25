#pragma once
#include <string>

enum class TokenType {
    INT, FLOAT, RETURN,
    IF, ELSE, WHILE,
    IDENT, NUMBER,
    PLUS, MINUS, MUL, DIV,
    ASSIGN, EQ, LT, GT,
    LPAREN, RPAREN,
    LBRACE, RBRACE,
    SEMI, COMMA,
    EOF_TOK
};

struct Token {
    TokenType type;
    std::string lexeme;
};
