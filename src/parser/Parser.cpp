#include "parser/Parser.h"
#include "utils/Logger.h"

Parser::Parser(const std::vector<Token>& t) : tokens(t), pos(0) {}

std::unique_ptr<AST> Parser::primary() {

    // NUMBER
    if (tokens[pos].type == TokenType::NUMBER)
        return std::make_unique<NumberAST>(std::stoi(tokens[pos++].lexeme));

    // FUNCTION CALL
    if (tokens[pos].type == TokenType::IDENT &&
        tokens[pos + 1].type == TokenType::LPAREN) {

        std::string fname = tokens[pos++].lexeme;
        pos++; // (

        std::vector<std::unique_ptr<AST>> args;
        while (tokens[pos].type != TokenType::RPAREN) {
            args.push_back(expression());
            if (tokens[pos].type == TokenType::COMMA) pos++;
        }
        pos++; // )

        auto call = std::make_unique<CallAST>();
        call->callee = fname;
        call->args = std::move(args);
        return call;
    }

    // VARIABLE
    if (tokens[pos].type == TokenType::IDENT)
        return std::make_unique<VariableAST>(tokens[pos++].lexeme);

    return nullptr;
}

std::unique_ptr<AST> Parser::parse() {
    Logger::log(Stage::PARSER, "Parsing started");
    auto program = std::make_unique<ProgramAST>();

    while (tokens[pos].type != TokenType::EOF_TOK) {
        program->functions.push_back(function());
    }
    return program;
}

std::unique_ptr<FunctionAST> Parser::function() {
    pos++; // consume 'int'
    std::string fname = tokens[pos++].lexeme;
    pos++; // (

    std::vector<std::string> args;
    while (tokens[pos].type != TokenType::RPAREN) {
        pos++; // consume type
        args.push_back(tokens[pos++].lexeme);
        if (tokens[pos].type == TokenType::COMMA) pos++;
    }
    pos++; // )

    auto proto = std::make_unique<PrototypeAST>();
    proto->name = fname;
    proto->args = args;

    auto body = block();
    return std::make_unique<FunctionAST>(
        FunctionAST{std::move(proto), std::move(body)});
}


std::unique_ptr<AST> Parser::expression() {
    auto lhs = primary();
    if (!lhs) return nullptr;

    while (tokens[pos].type == TokenType::PLUS ||
           tokens[pos].type == TokenType::MINUS ||
           tokens[pos].type == TokenType::LT ||
           tokens[pos].type == TokenType::GT ||
           tokens[pos].type == TokenType::EQ) {

        char op = tokens[pos++].lexeme[0];
        auto rhs = primary();

        lhs = std::make_unique<BinaryAST>(
            op, std::move(lhs), std::move(rhs));
    }
    return lhs;
}

std::unique_ptr<BlockAST> Parser::block() {
    auto block = std::make_unique<BlockAST>();
    pos++; // consume '{'

    while (tokens[pos].type != TokenType::RBRACE) {
        block->statements.push_back(statement());
    }
    pos++; // consume '}'
    return block;
}

std::unique_ptr<AST> Parser::statement() {
        // Return statement
    if (tokens[pos].type == TokenType::RETURN) {
        pos++; // consume 'return'
        auto expr = expression();
        pos++; // consume ';'
        return std::make_unique<ReturnAST>(std::move(expr));
    }

    // Variable declaration
    if (tokens[pos].type == TokenType::INT) {
        pos++;
        std::string name = tokens[pos++].lexeme;
        pos++; // ;
        return std::make_unique<VarDeclAST>(name);
    }

    // Assignment
    if (tokens[pos].type == TokenType::IDENT &&
        tokens[pos+1].type == TokenType::ASSIGN) {
        std::string name = tokens[pos++].lexeme;
        pos++; // =
        auto expr = expression();
        pos++; // ;
        return std::make_unique<AssignAST>(name, std::move(expr));
    }

    // If
    if (tokens[pos].type == TokenType::IF) {
        pos++; pos++; // if (
        auto cond = expression();
        pos++; // )
        auto thenBlock = block();

        std::unique_ptr<AST> elseBlock = nullptr;
        if (tokens[pos].type == TokenType::ELSE) {
            pos++;
            elseBlock = block();
        }
        auto node = std::make_unique<IfAST>();
        node->cond = std::move(cond);
        node->thenBlock = std::move(thenBlock);
        node->elseBlock = std::move(elseBlock);
        return node;
    }

    // While
    if (tokens[pos].type == TokenType::WHILE) {
        pos++; pos++; // while (
        auto cond = expression();
        pos++; // )
        auto body = block();

        auto node = std::make_unique<WhileAST>();
        node->cond = std::move(cond);
        node->body = std::move(body);
        return node;
    }

    return expression();
}
