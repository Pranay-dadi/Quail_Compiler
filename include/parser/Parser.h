#pragma once
#include "lexer/Token.h"
#include "AST.h"
#include <vector>
#include <string>
#include <set>

struct ParseError {
    int         line;
    std::string message;
};

class Parser {
public:
    explicit Parser(const std::vector<Token>& t);
    std::unique_ptr<AST> parse();

    bool hasErrors() const { return !errors.empty(); }
    const std::vector<ParseError>& getErrors() const { return errors; }

private:
    const std::vector<Token>& tokens;
    size_t pos;
    std::vector<ParseError> errors;

    // Known class names (populated as class declarations are parsed)
    std::set<std::string> classNames;

    // ── helpers ───────────────────────────────────────────────
    void addError(const std::string& msg);
    int  currentLine() const;
    void syncStatement();
    void syncFunction();

    std::vector<std::unique_ptr<AST>> drainComments();

    // ── grammar ───────────────────────────────────────────────
    std::unique_ptr<AST>         statement();
    std::unique_ptr<AST>         expression();
    std::unique_ptr<AST>         primary();
    std::unique_ptr<BlockAST>    block();
    std::unique_ptr<FunctionAST> function();
    std::unique_ptr<ClassDeclAST> parseClass();   // NEW: class definition
    std::unique_ptr<AST>         parseExpression(int minPrec = 0);
    int                          getPrecedence(TokenType type);
    bool                         isComment(TokenType t) const;
    bool                         isTypeKeyword(TokenType t) const;

    // ── OOP argument list parser (shared by call / method call) ──
    std::vector<std::unique_ptr<AST>> parseArgList();
};
