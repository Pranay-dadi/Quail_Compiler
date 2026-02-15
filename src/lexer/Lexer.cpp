#include "lexer/Lexer.h"
#include "utils/Logger.h"
#include <cctype>

Lexer::Lexer(const std::string& s) : src(s), pos(0) {}

std::vector<Token> Lexer::tokenize() {
    Logger::log(Stage::LEXER, "Starting tokenization");
    std::vector<Token> tokens;

    while (pos < src.size()) {
        char c = src[pos];

        if (isspace(c)) { pos++; continue; }

        if (isalpha(c)) {
            std::string id;
            while (isalnum(src[pos])) id += src[pos++];
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
                    if (isFloat) break; // Prevent multiple decimal points
                    isFloat = true;
                }
            num += src[pos++];
            }
            if (isFloat) 
                tokens.push_back({TokenType::FLOAT_VAL, num});
            else 
                tokens.push_back({TokenType::NUMBER, num});
        
            continue; // Skip the global pos++ at the end of the loop
        }

        

        switch (c) {
            case '+': tokens.push_back({TokenType::PLUS,"+"}); break;
            case '-': tokens.push_back({TokenType::MINUS,"-"}); break;
            case '*': tokens.push_back({TokenType::MUL,"*"}); break;
            case '/': tokens.push_back({TokenType::DIV,"/"}); break;
            case '=':
                if (src[pos+1] == '=') {
                    tokens.push_back({TokenType::EQ, "=="});
                    pos++;
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
                if (src[pos+1] == '&') {
                    tokens.push_back({TokenType::AND, "&&"});
                    pos++;
                }
                break;
            case '|':
                if (src[pos+1] == '|') {
                    tokens.push_back({TokenType::OR, "||"});
                    pos++;
                }
                break;
            case '!':
                if (src[pos+1] == '=') {
                    tokens.push_back({TokenType::NEQ, "!="});
                    pos++;
                } else tokens.push_back({TokenType::NOT, "!"});
                break;
            case '<':
                if (src[pos+1] == '=') {
                    tokens.push_back({TokenType::LE, "<="});
                    pos++;
                } else tokens.push_back({TokenType::LT, "<"});
                break;
            case '>':
                if (src[pos+1] == '=') {
                    tokens.push_back({TokenType::GE, ">="});
                    pos++;
                } else tokens.push_back({TokenType::GT, ">"});
                break;
        }
        pos++;
    }

    tokens.push_back({TokenType::EOF_TOK,""});
    return tokens;
}