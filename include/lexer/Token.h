#pragma once
#include <string>

enum class TokenType {
    // ── Core type keywords ────────────────────────────────────
    INT, FLOAT, VOID, CONST,

    // ── Control flow keywords ─────────────────────────────────
    RETURN,
    IF, ELSE, WHILE, FOR,
    BREAK, CONTINUE,
    SWITCH, CASE, DEFAULT,

    // ── OOP keywords ──────────────────────────────────────────
    CLASS,      // class
    NEW,        // new  (heap allocation)
    DELETE,     // delete (heap deallocation)
    THIS,       // this
    PUBLIC,     // public
    PRIVATE,    // private
    EXTENDS,    // extends (inheritance)
    SUPER,      // super (parent method/field access)
    OVERRIDE,   // override (method override marker)

    // ── String type ───────────────────────────────────────────
    STRING_TYPE, // 'string' keyword

    // ── I/O keywords ──────────────────────────────────────────
    PRINT,      // print(expr, ...)
    PRINTLN,    // println(expr, ...)
    SCAN,       // scan(var, ...)

    // ── Literals / identifiers ────────────────────────────────
    IDENT, NUMBER, FLOAT_VAL,
    STRING_LIT,  // "text"

    // ── Arithmetic operators ──────────────────────────────────
    PLUS, MINUS, MUL, DIV,

    // ── Assignment / comparison ───────────────────────────────
    ASSIGN, EQ, NEQ, INC,
    LT, GT, LE, GE,

    // ── Logical operators ─────────────────────────────────────
    AND, OR, NOT,

    // ── Pointer / reference operators ────────────────────────
    AMPERSAND,  // '&'  address-of (unary) / bitwise-and (not used yet)
    ARROW,      // '->' pointer member access

    // ── Punctuation ───────────────────────────────────────────
    LPAREN, RPAREN,
    LBRACE, RBRACE,
    LBRACKET, RBRACKET,
    SEMI, COMMA, COLON,
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