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

    // ── I/O keywords ──────────────────────────────────────────
    PRINT,      // print(expr, ...)   — output without newline
    PRINTLN,    // println(expr, ...) — output with trailing newline
    SCAN,       // scan(var, ...)     — read from stdin into variables

    // ── Literals / identifiers ────────────────────────────────
    IDENT, NUMBER, FLOAT_VAL,
    STRING_LIT,  // "text" — string literal (usable in print/println)

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