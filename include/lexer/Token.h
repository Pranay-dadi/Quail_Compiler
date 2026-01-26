#pragma once
#include <string>

enum class TokenType {
    INT, FLOAT, RETURN,
    IF, ELSE, WHILE,FOR,
    IDENT, NUMBER,
    PLUS, MINUS, MUL, DIV,
    ASSIGN, EQ,NEQ, LT, GT,LE, GE,
    AND, OR, NOT,
    LPAREN, RPAREN,
    LBRACE, RBRACE,
    LBRACKET, RBRACKET,
    SEMI, COMMA,
    EOF_TOK,
    BREAK,CONTINUE
};

struct Token {
    TokenType type;
    std::string lexeme;
};
