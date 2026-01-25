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
            else tokens.push_back({TokenType::IDENT, id});
            continue;
        }

        if (isdigit(c)) {
            std::string num;
            while (isdigit(src[pos])) num += src[pos++];
            tokens.push_back({TokenType::NUMBER, num});
            continue;
        }

        switch (c) {
            case '+': tokens.push_back({TokenType::PLUS,"+"}); break;
            case '-': tokens.push_back({TokenType::MINUS,"-"}); break;
            case '*': tokens.push_back({TokenType::MUL,"*"}); break;
            case '/': tokens.push_back({TokenType::DIV,"/"}); break;
            case '=': tokens.push_back({TokenType::ASSIGN,"="}); break;
            case '<': tokens.push_back({TokenType::LT,"<"}); break;
            case '>': tokens.push_back({TokenType::GT,">"}); break;
            case '(': tokens.push_back({TokenType::LPAREN,"("}); break;
            case ')': tokens.push_back({TokenType::RPAREN,")"}); break;
            case '{': tokens.push_back({TokenType::LBRACE,"{"}); break;
            case '}': tokens.push_back({TokenType::RBRACE,"}"}); break;
            case ';': tokens.push_back({TokenType::SEMI,";"}); break;
        }
        pos++;
    }

    tokens.push_back({TokenType::EOF_TOK,""});
    return tokens;
}
