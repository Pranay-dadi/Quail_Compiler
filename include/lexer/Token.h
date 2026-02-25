#pragma once
#include <string>

enum class TokenType {
    // ── Keywords ─────────────────────────────────────────────
    INT, FLOAT, RETURN,
    IF, ELSE, WHILE, FOR,
    BREAK, CONTINUE,

    // ── Literals / identifiers ────────────────────────────────
    IDENT, NUMBER, FLOAT_VAL,

    // ── Operators ────────────────────────────────────────────
    PLUS, MINUS, MUL, DIV,
    ASSIGN, EQ, NEQ, INC,
    LT, GT, LE, GE,
    AND, OR, NOT,

    // ── Punctuation ───────────────────────────────────────────
    LPAREN, RPAREN,
    LBRACE, RBRACE,
    LBRACKET, RBRACKET,
    SEMI, COMMA,

    // ── Comments (NEW — preserved through the pipeline) ───────
    LINE_COMMENT,    // // text until newline
    BLOCK_COMMENT,   // /* ... */  (may span multiple lines)

    // ── Sentinel ─────────────────────────────────────────────
    EOF_TOK
};

struct Token {
    TokenType   type;
    std::string lexeme;   // raw text (for comments: the comment body, not delimiters)
    int         line = 0; // 1-based source line
};