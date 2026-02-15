#include "lexer/Lexer.h"
#include "utils/Logger.h"
#include <cctype>

Lexer::Lexer(const std::string& s) : src(s), pos(0) {}
char Lexer::peek() const {
    if (pos + 1 >= src.size()) return '\0';
    return src[pos + 1];
}

char Lexer::getChar() {
    if (pos >= src.size()) return '\0';
    return src[pos++];
}

std::vector<Token> Lexer::tokenize() {
    Logger::log(Stage::LEXER, "Starting tokenization");
    std::vector<Token> tokens;

    while (pos < src.size()) {
        char c = src[pos];

        if (isspace(c)) { pos++; continue; }

        if (isalpha(c)) {
            std::string id;
            while (pos < src.size() && isalnum(src[pos])) id += src[pos++];
            if (id == "int") tokens.push_back({TokenType::INT, id});
            else if (id == "float") tokens.push_back({TokenType::FLOAT, id});
            else if (id == "return") tokens.push_back({TokenType::RETURN, id});
            else if (id == "if") tokens.push_back({TokenType::IF, id});
            else if (id == "else") tokens.push_back({TokenType::ELSE, id});
            else if (id == "while") tokens.push_back({TokenType::WHILE, id});
            else if (id == "for") tokens.push_back({TokenType::FOR, id});
            else if (id == "break") tokens.push_back({TokenType::BREAK, id});
            else if (id == "continue") tokens.push_back({TokenType::CONTINUE, id});
            else tokens.push_back({TokenType::IDENT, id});
            continue;
        }

        if (isdigit(c)) {
            std::string num;
            bool isFloat = false;
            while (pos < src.size() && (isdigit(src[pos]) || src[pos] == '.')) {
                if (src[pos] == '.') {
                    if (isFloat) break; 
                    isFloat = true;
                }
                num += src[pos++];
            }
            if (isFloat) tokens.push_back({TokenType::FLOAT_VAL, num});
            else tokens.push_back({TokenType::NUMBER, num});
            continue; 
        }

        switch (c) {
            case '+': 
                if (peek() == '+') {
                    getChar(); // consume the first '+'
                    getChar(); // consume the second '+'
                    tokens.push_back({TokenType::INC, "++"});
                    continue; // Skip the global pos++ at the bottom
                } else {
                    tokens.push_back({TokenType::PLUS, "+"});
                }
                break;
            case '-': tokens.push_back({TokenType::MINUS,"-"}); break;
            case '*': tokens.push_back({TokenType::MUL,"*"}); break;
            case '/': tokens.push_back({TokenType::DIV,"/"}); break;
            case '=':
                if (peek() == '=') {
                    getChar(); getChar();
                    tokens.push_back({TokenType::EQ, "=="});
                    continue;
                } else {
                    tokens.push_back({TokenType::ASSIGN, "="});
                }
                break;
            case '(': tokens.push_back({TokenType::LPAREN,"("}); break;
            case ')': tokens.push_back({TokenType::RPAREN,")"}); break;
            case '{': tokens.push_back({TokenType::LBRACE,"{"}); break;
            case '}': tokens.push_back({TokenType::RBRACE,"}"}); break;
            case ';': tokens.push_back({TokenType::SEMI,";"}); break;
            case '[': tokens.push_back({TokenType::LBRACKET, "["}); break;
            case ']': tokens.push_back({TokenType::RBRACKET, "]"}); break;
            case ',': tokens.push_back({TokenType::COMMA, ","}); break;
            case '&':
                if (peek() == '&') {
                    getChar(); getChar();
                    tokens.push_back({TokenType::AND, "&&"});
                    continue;
                }
                break;
            case '|':
                if (peek() == '|') {
                    getChar(); getChar();
                    tokens.push_back({TokenType::OR, "||"});
                    continue;
                }
                break;
            case '!':
                if (peek() == '=') {
                    getChar(); getChar();
                    tokens.push_back({TokenType::NEQ, "!="});
                    continue;
                } else tokens.push_back({TokenType::NOT, "!"});
                break;
            case '<':
                if (peek() == '=') {
                    getChar(); getChar();
                    tokens.push_back({TokenType::LE, "<="});
                    continue;
                } else tokens.push_back({TokenType::LT, "<"});
                break;
            case '>':
                if (peek() == '=') {
                    getChar(); getChar();
                    tokens.push_back({TokenType::GE, ">="});
                    continue;
                } else tokens.push_back({TokenType::GT, ">"});
                break;
        }
        pos++;
    }

    tokens.push_back({TokenType::EOF_TOK,""});
    return tokens;
}