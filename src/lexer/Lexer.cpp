#include "lexer/Lexer.h"
#include <cctype>

Lexer::Lexer(const std::string& s, bool kc)
    : src(s), pos(0), line(1), keepComments(kc) {}

char Lexer::peek() const {
    if (pos + 1 >= src.size()) return '\0';
    return src[pos + 1];
}

char Lexer::getChar() {
    if (pos >= src.size()) return '\0';
    char c = src[pos++];
    if (c == '\n') line++;
    return c;
}

void Lexer::addError(const std::string& msg) {
    errors.push_back({line, msg});
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (pos < src.size()) {
        char c = src[pos];

        // ── Whitespace ────────────────────────────────────────
        if (c == '\n') { line++; pos++; continue; }
        if (isspace(c)) { pos++; continue; }

        int tokLine = line;

        // ── Line comment  // ──────────────────────────────────
        if (c == '/' && peek() == '/') {
            pos += 2;
            std::string body;
            while (pos < src.size() && src[pos] != '\n')
                body += src[pos++];
            size_t start = body.find_first_not_of(' ');
            if (start != std::string::npos) body = body.substr(start);
            if (keepComments)
                tokens.push_back({TokenType::LINE_COMMENT, body, tokLine});
            continue;
        }

        // ── Block comment  /* ... */ ──────────────────────────
        if (c == '/' && peek() == '*') {
            pos += 2;
            int startLine = line;
            std::string body;
            bool closed = false;
            while (pos + 1 < src.size()) {
                if (src[pos] == '\n') { line++; body += '\n'; pos++; continue; }
                if (src[pos] == '*' && src[pos+1] == '/') {
                    pos += 2; closed = true; break;
                }
                body += src[pos++];
            }
            if (!closed)
                addError("Unterminated block comment (opened at line "
                         + std::to_string(startLine) + ")");
            size_t s = body.find_first_not_of(" \t\n");
            size_t e = body.find_last_not_of(" \t\n");
            if (s != std::string::npos) body = body.substr(s, e - s + 1);
            else body = "";
            if (keepComments)
                tokens.push_back({TokenType::BLOCK_COMMENT, body, tokLine});
            continue;
        }

        // ── String literals  "..." ────────────────────────────
        if (c == '"') {
            pos++;
            std::string str;
            bool closed = false;
            while (pos < src.size()) {
                char ch = src[pos];
                if (ch == '"') { pos++; closed = true; break; }
                if (ch == '\n') { addError("Unterminated string literal"); break; }
                if (ch == '\\' && pos + 1 < src.size()) {
                    pos++;
                    switch (src[pos]) {
                        case 'n':  str += '\n'; break;
                        case 't':  str += '\t'; break;
                        case '"':  str += '"';  break;
                        case '\\': str += '\\'; break;
                        case '0':  str += '\0'; break;
                        default:
                            str += '\\';
                            str += src[pos];
                            break;
                    }
                    pos++;
                } else {
                    str += ch;
                    pos++;
                }
            }
            if (!closed && (pos >= src.size()))
                addError("Unterminated string literal at end of file");
            tokens.push_back({TokenType::STRING_LIT, str, tokLine});
            continue;
        }

        // ── Identifiers / keywords ────────────────────────────
        if (isalpha(c) || c == '_') {
            std::string id;
            while (pos < src.size() && (isalnum(src[pos]) || src[pos] == '_'))
                id += src[pos++];
            TokenType tt;
            if      (id == "int")      tt = TokenType::INT;
            else if (id == "float")    tt = TokenType::FLOAT;
            else if (id == "void")     tt = TokenType::VOID;
            else if (id == "const")    tt = TokenType::CONST;
            else if (id == "return")   tt = TokenType::RETURN;
            else if (id == "if")       tt = TokenType::IF;
            else if (id == "else")     tt = TokenType::ELSE;
            else if (id == "while")    tt = TokenType::WHILE;
            else if (id == "for")      tt = TokenType::FOR;
            else if (id == "break")    tt = TokenType::BREAK;
            else if (id == "continue") tt = TokenType::CONTINUE;
            else if (id == "switch")   tt = TokenType::SWITCH;
            else if (id == "case")     tt = TokenType::CASE;
            else if (id == "default")  tt = TokenType::DEFAULT;
            else if (id == "class")    tt = TokenType::CLASS;
            else if (id == "new")      tt = TokenType::NEW;
            else if (id == "delete")   tt = TokenType::DELETE;
            else if (id == "this")     tt = TokenType::THIS;
            else if (id == "public")   tt = TokenType::PUBLIC;
            else if (id == "private")  tt = TokenType::PRIVATE;
            else if (id == "extends")  tt = TokenType::EXTENDS;
            else if (id == "super")    tt = TokenType::SUPER;
            else if (id == "override") tt = TokenType::OVERRIDE;
            else if (id == "string")   tt = TokenType::STRING_TYPE;
            // ── I/O keywords ──────────────────────────────────
            else if (id == "print")    tt = TokenType::PRINT;
            else if (id == "println")  tt = TokenType::PRINTLN;
            else if (id == "scan")     tt = TokenType::SCAN;
            else                       tt = TokenType::IDENT;
            tokens.push_back({tt, id, tokLine});
            continue;
        }

        // ── Numbers ───────────────────────────────────────────
        if (isdigit(c)) {
            std::string num;
            bool isFloat = false;
            while (pos < src.size() && (isdigit(src[pos]) || src[pos] == '.')) {
                if (src[pos] == '.') { if (isFloat) break; isFloat = true; }
                num += src[pos++];
            }
            tokens.push_back({isFloat ? TokenType::FLOAT_VAL : TokenType::NUMBER,
                               num, tokLine});
            continue;
        }

        // ── Operators & punctuation ───────────────────────────
        switch (c) {
            case '+':
                if (peek() == '+') { getChar(); getChar();
                    tokens.push_back({TokenType::INC, "++", tokLine}); }
                else { tokens.push_back({TokenType::PLUS, "+", tokLine}); pos++; }
                continue;
            case '-':
                if (peek() == '>') { getChar(); getChar();
                    tokens.push_back({TokenType::ARROW, "->", tokLine}); }
                else { tokens.push_back({TokenType::MINUS, "-", tokLine}); pos++; }
                continue;
            case '*':
                tokens.push_back({TokenType::MUL, "*", tokLine}); pos++; continue;
            case '/':
                tokens.push_back({TokenType::DIV, "/", tokLine}); pos++; continue;
            case '.':
                tokens.push_back({TokenType::DOT, ".", tokLine}); pos++; continue;
            case '=':
                if (peek() == '=') { getChar(); getChar();
                    tokens.push_back({TokenType::EQ, "==", tokLine}); }
                else { tokens.push_back({TokenType::ASSIGN, "=", tokLine}); pos++; }
                continue;
            case '!':
                if (peek() == '=') { getChar(); getChar();
                    tokens.push_back({TokenType::NEQ, "!=", tokLine}); }
                else { tokens.push_back({TokenType::NOT, "!", tokLine}); pos++; }
                continue;
            case '<':
                if (peek() == '=') { getChar(); getChar();
                    tokens.push_back({TokenType::LE, "<=", tokLine}); }
                else { tokens.push_back({TokenType::LT, "<", tokLine}); pos++; }
                continue;
            case '>':
                if (peek() == '=') { getChar(); getChar();
                    tokens.push_back({TokenType::GE, ">=", tokLine}); }
                else { tokens.push_back({TokenType::GT, ">", tokLine}); pos++; }
                continue;
            case '&':
                if (peek() == '&') { getChar(); getChar();
                    tokens.push_back({TokenType::AND, "&&", tokLine}); }
                else {
                    // single & = address-of operator
                    tokens.push_back({TokenType::AMPERSAND, "&", tokLine}); pos++;
                }
                continue;
            case '|':
                if (peek() == '|') { getChar(); getChar();
                    tokens.push_back({TokenType::OR, "||", tokLine}); }
                else { addError("Unknown character '|' — did you mean '||'?"); pos++; }
                continue;
            case ':':
                tokens.push_back({TokenType::COLON,    ":", tokLine}); pos++; continue;
            case '(': tokens.push_back({TokenType::LPAREN,   "(", tokLine}); pos++; continue;
            case ')': tokens.push_back({TokenType::RPAREN,   ")", tokLine}); pos++; continue;
            case '{': tokens.push_back({TokenType::LBRACE,   "{", tokLine}); pos++; continue;
            case '}': tokens.push_back({TokenType::RBRACE,   "}", tokLine}); pos++; continue;
            case '[': tokens.push_back({TokenType::LBRACKET, "[", tokLine}); pos++; continue;
            case ']': tokens.push_back({TokenType::RBRACKET, "]", tokLine}); pos++; continue;
            case ';': tokens.push_back({TokenType::SEMI,     ";", tokLine}); pos++; continue;
            case ',': tokens.push_back({TokenType::COMMA,    ",", tokLine}); pos++; continue;
            default:
                addError(std::string("Unknown character '") + c + "'");
                pos++;
                continue;
        }
    }

    tokens.push_back({TokenType::EOF_TOK, "", line});
    return tokens;
}