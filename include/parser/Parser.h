#pragma once
#include "lexer/Token.h"
#include "AST.h"
#include <vector>

class Parser {
public:
    Parser(const std::vector<Token>& t);
    std::unique_ptr<AST> parse();

private:
    const std::vector<Token>& tokens;
    size_t pos;

    std::unique_ptr<AST> statement();
    std::unique_ptr<AST> expression();
    std::unique_ptr<AST> primary();
    std::unique_ptr<BlockAST> block();
    std::unique_ptr<FunctionAST> function();

};

