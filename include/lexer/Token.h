#pragma once
#include <string>

enum class TokenType {
    // ── Keywords ─────────────────────────────────────────────
    INT, FLOAT, RETURN,
    IF, ELSE, WHILE, FOR,
    BREAK, CONTINUE,

    // ── OOP keywords ──────────────────────────────────────────
    CLASS,      // class
    NEW,        // new  (reserved for future heap alloc)
    THIS,       // this
    PUBLIC,     // public  (parsed, treated as modifier, no enforcement)
    PRIVATE,    // private (same)
    VOID,       // void return type

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
    DOT,        // '.'  member-access operator

    // ── Comments (preserved through pipeline) ─────────────────
    LINE_COMMENT,    // // text until newline
    BLOCK_COMMENT,   // /* ... */

    // ── Sentinel ─────────────────────────────────────────────
    EOF_TOK
};

struct Token {
    TokenType   type;
    std::string lexeme;
    int         line = 0;
};
