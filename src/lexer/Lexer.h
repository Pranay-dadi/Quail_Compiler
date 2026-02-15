#ifndef LEXER_H
#define LEXER_H
#include "Token.h"
#include <vector>

class Lexer {
public:
    Lexer(const std::string& src);
    std::vector<Token> tokenize();

private:
    std::string src;
    size_t pos;
    char getChar();
    char peek() const;
};
#endif