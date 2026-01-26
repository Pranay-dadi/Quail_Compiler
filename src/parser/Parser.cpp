#include "parser/Parser.h"
#include "utils/Logger.h"
#include <iostream>

Parser::Parser(const std::vector<Token>& t) : tokens(t), pos(0) {}

int Parser::getPrecedence(TokenType type) {
    switch (type) {
        case TokenType::MUL:
        case TokenType::DIV:     return 70;
        case TokenType::PLUS:
        case TokenType::MINUS:   return 60;
        case TokenType::LT:
        case TokenType::GT:
        case TokenType::LE:
        case TokenType::GE:      return 50;
        case TokenType::EQ:
        case TokenType::NEQ:     return 45;
        case TokenType::AND:     return 30;
        case TokenType::OR:      return 20;
        default:                 return -1;
    }
}

std::unique_ptr<AST> Parser::primary() {
    const auto& tok = tokens[pos];

    if (tok.type == TokenType::NUMBER) {
        int val = std::stoi(tok.lexeme);
        pos++;
        return std::make_unique<NumberAST>(val);
    }

    if (tok.type == TokenType::IDENT) {
        std::string name = tok.lexeme;
        pos++;

        if (tokens[pos].type == TokenType::LPAREN) {
            // function call
            pos++; // (
            std::vector<std::unique_ptr<AST>> args;
            if (tokens[pos].type != TokenType::RPAREN) {
                do {
                    args.push_back(expression());
                } while (tokens[pos].type == TokenType::COMMA && pos++);
            }
            if (tokens[pos].type == TokenType::RPAREN) pos++;
            auto call = std::make_unique<CallAST>();
            call->callee = name;
            call->args = std::move(args);
            return call;
        }

        if (tokens[pos].type == TokenType::LBRACKET) {
            pos++; // [
            auto idx = expression();
            if (tokens[pos].type == TokenType::RBRACKET) pos++;
            return std::make_unique<ArrayAccessAST>(name, std::move(idx));
        }

        return std::make_unique<VariableAST>(name);
    }

    if (tok.type == TokenType::LPAREN) {
        pos++;
        auto expr = expression();
        if (tokens[pos].type == TokenType::RPAREN) pos++;
        return expr;
    }

    // Unary minus and logical not
    if (tok.type == TokenType::MINUS || tok.type == TokenType::NOT) {
        std::string op = (tok.type == TokenType::MINUS) ? "-" : "!";
        pos++;
        auto operand = primary();
        auto u = std::make_unique<UnaryAST>();
        u->op = op;
        u->operand = std::move(operand);
        return u;
    }

    std::cerr << "Unexpected token in primary: " << static_cast<int>(tok.type) << "\n";
    pos++; // prevent hang
    return nullptr;
}

std::unique_ptr<AST> Parser::parseExpression(int minPrec) {
    auto lhs = primary();
    if (!lhs) return nullptr;

    while (true) {
        TokenType opType = tokens[pos].type;
        int prec = getPrecedence(opType);
        if (prec < minPrec) break;

        std::string opStr = tokens[pos++].lexeme;

        auto rhs = parseExpression(prec + 1);
        if (!rhs) break;

        if (opType == TokenType::AND || opType == TokenType::OR ||
            opType == TokenType::EQ  || opType == TokenType::NEQ) {
            auto log = std::make_unique<LogicalAST>();
            log->op = opStr;
            log->lhs = std::move(lhs);
            log->rhs = std::move(rhs);
            lhs = std::move(log);
        } else {
            auto bin = std::make_unique<BinaryAST>();
            bin->op = opStr;
            bin->lhs = std::move(lhs);
            bin->rhs = std::move(rhs);
            lhs = std::move(bin);
        }
    }
    return lhs;
}

std::unique_ptr<BlockAST> Parser::block() {
    auto b = std::make_unique<BlockAST>();

    if (tokens[pos].type != TokenType::LBRACE) {
        std::cerr << "Expected '{' at pos " << pos << "\n";
        return b;
    }
    pos++; // {

    while (tokens[pos].type != TokenType::RBRACE && tokens[pos].type != TokenType::EOF_TOK) {
        auto stmt = statement();
        if (stmt) {
            b->statements.push_back(std::move(stmt));
        }
    }

    if (tokens[pos].type == TokenType::RBRACE) {
        pos++;
    } else {
        std::cerr << "Missing '}' at pos " << pos << "\n";
    }
    return b;
}

std::unique_ptr<AST> Parser::statement() {
    const auto& tok = tokens[pos];

    // return expr;
    if (tok.type == TokenType::RETURN) {
        pos++;
        std::unique_ptr<AST> expr = nullptr;
        if (tokens[pos].type != TokenType::SEMI) {
            expr = expression();
        }
        if (tokens[pos].type == TokenType::SEMI) pos++;
        else std::cerr << "Expected ';' after return at pos " << pos << "\n";
        return std::make_unique<ReturnAST>(std::move(expr));
    }

    // int x;
    if (tok.type == TokenType::INT) {
        pos++;
        if (tokens[pos].type != TokenType::IDENT) {
            std::cerr << "Expected identifier after 'int'\n";
            return nullptr;
        }
        std::string name = tokens[pos++].lexeme;
        if (tokens[pos].type == TokenType::SEMI) pos++;
        else std::cerr << "Expected ';' after declaration at pos " << pos << "\n";
        if (tokens[pos].type == TokenType::LBRACKET) {
            pos++; // [
            int size = std::stoi(tokens[pos++].lexeme);
            pos++; // ]
            pos++; // ;
            return std::make_unique<ArrayDeclAST>(name, size);
        }
        pos++; // ;
        return std::make_unique<VarDeclAST>(name);
    }

    // if (cond) { ... } else { ... }
    if (tok.type == TokenType::IF) {
        pos++; // if
        if (tokens[pos++].type != TokenType::LPAREN) std::cerr << "Expected '(' after if\n";
        auto cond = expression();
        if (tokens[pos++].type != TokenType::RPAREN) std::cerr << "Expected ')' after if cond\n";
        auto thenB = block();
        std::unique_ptr<AST> elseB = nullptr;
        if (tokens[pos].type == TokenType::ELSE) {
            pos++;
            elseB = block();
        }
        auto node = std::make_unique<IfAST>();
        node->cond = std::move(cond);
        node->thenBlock = std::move(thenB);
        node->elseBlock = std::move(elseB);
        return node;
    }

    // while (cond) { ... }
    if (tok.type == TokenType::WHILE) {
        pos++;
        if (tokens[pos++].type != TokenType::LPAREN) {/*error*/}
        auto cond = expression();
        if (tokens[pos++].type != TokenType::RPAREN) {/*error*/}
        auto body = block();
        auto node = std::make_unique<WhileAST>();
        node->cond = std::move(cond);
        node->body = std::move(body);
        return node;
    }

    // for (init; cond; inc) { ... }
    if (tok.type == TokenType::FOR) {
        pos++; // for
        if (tokens[pos++].type != TokenType::LPAREN) std::cerr << "Expected '(' after for\n";

        std::unique_ptr<AST> init = nullptr;
        if (tokens[pos].type != TokenType::SEMI) {
            init = statement(); // may consume its own ;
        } else {
            pos++; // ;
        }

        auto cond = expression();
        if (tokens[pos++].type != TokenType::SEMI) std::cerr << "Expected ';' after for cond\n";

        std::unique_ptr<AST> inc = nullptr;
        if (tokens[pos].type != TokenType::RPAREN) {
            inc = expression();
        }
        if (tokens[pos++].type != TokenType::RPAREN) std::cerr << "Expected ')' after for inc\n";

        auto body = block();

        auto f = std::make_unique<ForAST>();
        f->init = std::move(init);
        f->cond = std::move(cond);
        f->inc  = std::move(inc);
        f->body = std::move(body);
        return f;
    }

    // break;
    if (tok.type == TokenType::BREAK) {
        pos++;
        if (tokens[pos].type == TokenType::SEMI) pos++;
        return std::make_unique<BreakAST>();
    }

    // continue;
    if (tok.type == TokenType::CONTINUE) {
        pos++;
        if (tokens[pos].type == TokenType::SEMI) pos++;
        return std::make_unique<ContinueAST>();
    }

    // Assignment / array assignment / expression statement
    if (tok.type == TokenType::IDENT) {
        std::string name = tok.lexeme;
        pos++;

        // arr[idx] = value;
        if (tokens[pos].type == TokenType::LBRACKET) {
            pos++;
            auto idx = expression();
            if (tokens[pos].type == TokenType::RBRACKET) pos++;
            if (tokens[pos].type == TokenType::ASSIGN) pos++;
            auto val = expression();
            if (tokens[pos].type == TokenType::SEMI) pos++;
            else std::cerr << "Expected ';' after array assign\n";
            return std::make_unique<ArrayAssignAST>(name, std::move(idx), std::move(val));
        }

        // x = expr;
        if (tokens[pos].type == TokenType::ASSIGN) {
            pos++; // =
            auto rhs = parseExpression(0); // lowest precedence
            if (!rhs) {
                std::cerr << "Failed to parse RHS of assignment\n";
                return nullptr;
            }
            auto assign = std::make_unique<AssignAST>(name, std::move(rhs));
            if (tokens[pos].type == TokenType::SEMI) pos++;
            else std::cerr << "Expected ';' after assignment at pos " << pos << "\n";
            return assign;
        }

        // not assignment → rewind and try expression statement
        pos--;
    }

    // Expression statement fallback
    auto expr = parseExpression(0);
    if (expr) {
        if (tokens[pos].type == TokenType::SEMI) {
            pos++;
            return expr;
        } else {
            std::cerr << "Expected ';' after expression statement at pos " << pos << "\n";
            pos++; // safety
        }
    }

    // Safety net
    std::cerr << "Could not parse statement at pos " << pos
              << " token: " << static_cast<int>(tokens[pos].type) << "\n";
    pos++;
    return nullptr;
}

std::unique_ptr<FunctionAST> Parser::function() {
    if (tokens[pos].type != TokenType::INT) {
        std::cerr << "Expected return type (currently only int supported)\n";
        return nullptr;
    }
    pos++; // int

    if (tokens[pos].type != TokenType::IDENT) {
        std::cerr << "Expected function name\n";
        return nullptr;
    }
    std::string name = tokens[pos++].lexeme;

    if (tokens[pos++].type != TokenType::LPAREN) {
        std::cerr << "Expected '(' after function name\n";
    }

    std::vector<std::string> args;
    while (tokens[pos].type != TokenType::RPAREN) {
        if (tokens[pos].type == TokenType::INT) pos++; // skip type for now
        if (tokens[pos].type == TokenType::IDENT) {
            args.push_back(tokens[pos++].lexeme);
        }
        if (tokens[pos].type == TokenType::COMMA) pos++;
    }
    pos++; // )

    auto proto = std::make_unique<PrototypeAST>();
    proto->name = name;
    proto->args = std::move(args);

    auto body = block();

    return std::make_unique<FunctionAST>(std::move(proto), std::move(body));
}

// Add this right before or after parse() function
std::unique_ptr<AST> Parser::expression() {
    return parseExpression(0);  // Start with lowest precedence
}

std::unique_ptr<AST> Parser::parse() {
    Logger::log(Stage::PARSER, "Parsing started");
    auto program = std::make_unique<ProgramAST>();

    while (tokens[pos].type != TokenType::EOF_TOK) {
        auto fn = function();
        if (fn) {
            program->functions.push_back(std::move(fn));
        } else {
            // skip bad function definition
            while (tokens[pos].type != TokenType::EOF_TOK && tokens[pos].type != TokenType::RBRACE) pos++;
            if (tokens[pos].type == TokenType::RBRACE) pos++;
        }
    }
    return program;
}