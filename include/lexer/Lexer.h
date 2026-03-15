#ifndef LEXER_H
#define LEXER_H
#include "Token.h"
#include <vector>
#include <string>

struct LexError {
    int         line;
    std::string message;
};

class Lexer {
public:
    explicit Lexer(const std::string& src, bool keepComments = true);
    std::vector<Token> tokenize();

    bool hasErrors() const { return !errors.empty(); }
    const std::vector<LexError>& getErrors() const { return errors; }

private:
    std::string           src;
    size_t                pos;
    int                   line;
    bool                  keepComments;  // if false, drop comment tokens (compat mode)
    std::vector<LexError> errors;

    char getChar();
    char peek() const;
    void addError(const std::string& msg);
};
#endif
