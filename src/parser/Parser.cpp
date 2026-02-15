#include "parser/Parser.h"
#include "utils/Logger.h"
#include <iostream>

Parser::Parser(const std::vector<Token>& t) : tokens(t), pos(0) {}

int Parser::getPrecedence(TokenType type) {
    switch (type) {
        case TokenType::MUL:
        case TokenType::DIV:   return 70;
        case TokenType::PLUS:
        case TokenType::MINUS: return 60;
        case TokenType::LT:
        case TokenType::GT:
        case TokenType::LE:
        case TokenType::GE:    return 50;
        case TokenType::EQ:
        case TokenType::NEQ:   return 45;
        case TokenType::AND:   return 30;
        case TokenType::OR:    return 20;
        default:               return -1;
    }
}

std::unique_ptr<AST> Parser::primary() {
    if (pos >= tokens.size()) return nullptr;
    const auto& tok = tokens[pos];

    if (tok.type == TokenType::ASSIGN) {
        std::cerr << "[DEBUG] Assignment slipped into primary at pos " << pos << "\n";
        pos++; 
        return nullptr;
    }

    if (tok.type == TokenType::NUMBER) {
        int val = std::stoi(tok.lexeme);
        pos++;
        return std::make_unique<NumberAST>(val);
    }

    if (tok.type == TokenType::IDENT) {
        std::string name = tok.lexeme;
        pos++;
        
        if (pos < tokens.size() && tokens[pos].type == TokenType::INC) {
            pos++; // consume '++'
            return std::make_unique<PostIncAST>(name);
        }

        if (pos < tokens.size() && tokens[pos].type == TokenType::LPAREN) {
            pos++; // (
            std::vector<std::unique_ptr<AST>> args;
            if (pos < tokens.size() && tokens[pos].type != TokenType::RPAREN) {
                do {
                    auto arg = expression();
                    if (arg) args.push_back(std::move(arg));
                } while (pos < tokens.size() && tokens[pos].type == TokenType::COMMA && ++pos);
            }
            if (pos < tokens.size() && tokens[pos].type == TokenType::RPAREN) pos++;
            else std::cerr << "Missing ')' in function call at pos " << pos << "\n";
            auto call = std::make_unique<CallAST>();
            call->callee = name;
            call->args = std::move(args);
            return call;
        }

        if (pos < tokens.size() && tokens[pos].type == TokenType::LBRACKET) {
            pos++; // [
            auto idx = expression();
            if (pos < tokens.size() && tokens[pos].type == TokenType::RBRACKET) pos++;
            else std::cerr << "Missing ']' in array access at pos " << pos << "\n";
            return std::make_unique<ArrayAccessAST>(name, std::move(idx));
        }

        return std::make_unique<VariableAST>(name);
    }

    if (tok.type == TokenType::LPAREN) {
        pos++;
        auto expr = expression();
        if (pos < tokens.size() && tokens[pos].type == TokenType::RPAREN) pos++;
        else std::cerr << "Missing ')' in parenthesized expression at pos " << pos << "\n";
        return expr;
    }

    // Unary minus and logical not
    if (tok.type == TokenType::MINUS || tok.type == TokenType::NOT) {
        std::string op = (tok.type == TokenType::MINUS) ? "-" : "!";
        pos++;
        auto operand = primary();
        if (!operand) return nullptr;
        auto u = std::make_unique<UnaryAST>();
        u->op = op;
        u->operand = std::move(operand);
        return u;
    }

    std::cerr << "Unexpected token in primary: " << static_cast<int>(tok.type)
              << " ('" << tok.lexeme << "') at position " << pos << "\n";
    pos++;
    return nullptr;
}

std::unique_ptr<AST> Parser::parseExpression(int minPrec) {
    auto lhs = primary();
    if (!lhs) return nullptr;

    while (pos < tokens.size()) {
        TokenType opType = tokens[pos].type;
        int prec = getPrecedence(opType);
        if (prec < minPrec) break;

        std::string opStr = tokens[pos++].lexeme;
        auto rhs = parseExpression(prec + 1);
        if (!rhs) break;

        if (opType == TokenType::AND || opType == TokenType::OR ||
            opType == TokenType::EQ || opType == TokenType::NEQ) {
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
    if (tokens[pos].type == TokenType::LBRACE) pos++; // {

    while (tokens[pos].type != TokenType::RBRACE && tokens[pos].type != TokenType::EOF_TOK) {
        // MUST call statement(), NOT expression()
        auto stmt = statement(); 
        if (stmt) b->statements.push_back(std::move(stmt));
    }

    if (tokens[pos].type == TokenType::RBRACE) pos++; // }
    return b;
}

std::unique_ptr<AST> Parser::assignmentExpr() {
    if (tokens[pos].type != TokenType::IDENT) return nullptr;

    std::string name = tokens[pos++].lexeme;
    if (pos < tokens.size() && tokens[pos].type == TokenType::ASSIGN) {
        pos++;
        auto rhs = parseExpression(0);
        return std::make_unique<AssignAST>(name, std::move(rhs));
    }
    pos--;
    return nullptr;
}

std::unique_ptr<AST> Parser::statement() {
    if (pos >= tokens.size()) return nullptr;
    const auto& tok = tokens[pos];

    if (tok.type == TokenType::LBRACE) {
        return block(); // Call your block() function which handles { ... }
    }

    if (tokens[pos].type == TokenType::IDENT) {
    if (pos + 1 < tokens.size() && tokens[pos + 1].type == TokenType::ASSIGN) {
        std::string name = tokens[pos].lexeme;
        pos += 2; // Move past 'name' and '='
        
        auto val = expression(); 
        
        if (tokens[pos].type == TokenType::SEMI) pos++;
        return std::make_unique<AssignAST>(name, std::move(val)); // MUST RETURN HERE
    }
}

    // int x;
    if (tok.type == TokenType::INT || tok.type == TokenType::FLOAT) {
        pos++; // consume 'int'
        if (tokens[pos].type != TokenType::IDENT) return nullptr;
        
        std::string name = tokens[pos++].lexeme; // consume 'name'

        // Handle Array or Simple Var
        if (tokens[pos].type == TokenType::LBRACKET) {
            pos++; // [
            int size = std::stoi(tokens[pos++].lexeme); 
            if (tokens[pos].type == TokenType::RBRACKET) pos++;
            if (tokens[pos].type == TokenType::SEMI) pos++;
            return std::make_unique<ArrayDeclAST>(name, size);
        }

        // Standard variable: Just check for ONE semicolon
        if (tokens[pos].type == TokenType::SEMI) {
            pos++;
        }
        return std::make_unique<VarDeclAST>(name);
    }
    // return expr;
    if (tok.type == TokenType::RETURN) {
        pos++;
        std::unique_ptr<AST> expr = nullptr;
        if (pos < tokens.size() && tokens[pos].type != TokenType::SEMI) {
            expr = expression();
        }
        if (tokens[pos].type == TokenType::SEMI) pos++;
        return std::make_unique<ReturnAST>(std::move(expr));
    }


    // if (cond) { ... } [else { ... }]
    if (tok.type == TokenType::IF) {
        pos++; // if
        if (pos >= tokens.size() || tokens[pos++].type != TokenType::LPAREN)
            std::cerr << "Expected '(' after if at pos " << pos << "\n";
        auto cond = expression();
        if (pos >= tokens.size() || tokens[pos++].type != TokenType::RPAREN)
            std::cerr << "Expected ')' after if condition at pos " << pos << "\n";
        auto thenB = block();
        std::unique_ptr<AST> elseB = nullptr;
        if (pos < tokens.size() && tokens[pos].type == TokenType::ELSE) {
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
        pos++; // while
        if (pos >= tokens.size() || tokens[pos].type != TokenType::LPAREN) {
            std::cerr << "Expected '(' after 'while' at pos " << pos << "\n";
            return nullptr;
        }
        pos++;
        auto cond = expression();
        if (!cond) return nullptr;
        if (pos >= tokens.size() || tokens[pos].type != TokenType::RPAREN) {
            std::cerr << "Expected ')' after while condition at pos " << pos << "\n";
            return nullptr;
        }
        pos++;
        auto body = block();
        if (!body) return nullptr;
        auto node = std::make_unique<WhileAST>();
        node->cond = std::move(cond);
        node->body = std::move(body);
        return node;
    }

        // for (init; cond; incr) { body }
    if (tok.type == TokenType::FOR) {
        
        pos++; // consume 'for'
        if (tokens[pos++].type != TokenType::LPAREN) std::cerr << "Expected '(' after for\n";

        std::unique_ptr<AST> init = nullptr;
    
        // Check if init is an assignment: IDENT = EXPR
        if (tokens[pos].type == TokenType::IDENT && tokens[pos+1].type == TokenType::ASSIGN) {
            std::string name = tokens[pos].lexeme;
            pos += 2; // consume name and '='
            auto val = expression();
            init = std::make_unique<AssignAST>(name, std::move(val));
        } else if (tokens[pos].type != TokenType::SEMI) {
            init = expression(); // Fallback for simple expressions
        }

        // Now correctly consume the ;
        if (tokens[pos].type == TokenType::SEMI) {
            pos++; 
        } else {
            std::cerr << "Expected ';' after for init\n";
            return nullptr;
        }

        // Condition
        auto cond = expression();
        if (tokens[pos++].type != TokenType::SEMI) std::cerr << "Expected ';' after for cond\n";

        auto inc = expression();
        if (tokens[pos++].type != TokenType::RPAREN) std::cerr << "Expected )\n";
        
        auto body = block();
        return std::make_unique<ForAST>(std::move(init), std::move(cond), std::move(inc), std::move(body));

        /*std::unique_ptr<AST> inc = nullptr;
        if (tokens[pos].type != TokenType::RPAREN) {
            inc = expression();
        }

        // Consume separator ; after condition
        if (pos < tokens.size() && tokens[pos].type == TokenType::SEMI) {
            pos++; // consume ;
        } else {
            std::cerr << "Expected ';' after for condition at pos " << pos 
                      << " (got " << (int)tokens[pos].type << " '" << tokens[pos].lexeme << "')\n";
            return nullptr;
        }

        // Increment (optional) — NO ; after it!
        std::unique_ptr<AST> incr = nullptr;
        if (pos < tokens.size() && tokens[pos].type != TokenType::RPAREN) {
            incr = assignmentExpr(); // consumes "i = i + 1" — no ; expected
            if (!incr) {
                incr = expression();
            }
        }

        // Closing )
        if (pos < tokens.size() && tokens[pos].type == TokenType::RPAREN) {
            pos++;
        } else {
            std::cerr << "Expected ')' after for header at pos " << pos 
                      << " (got " << (int)tokens[pos].type << " '" << tokens[pos].lexeme << "')\n";
            return nullptr;
        }

        // Body
        auto body = block();
        if (!body) return nullptr;

        auto f = std::make_unique<ForAST>();
        f->init = std::move(init);
        f->cond = std::move(cond);
        f->inc  = std::move(inc);
        f->body = std::move(body);
        return f;*/
    }

    

    // break;
    // Check by Type OR by the literal string "break"
    if (tok.type == TokenType::BREAK || (tok.lexeme == "break")) {
        pos++; // consume 'break'

        // Robust semicolon check
        if (pos < tokens.size() && tokens[pos].type == TokenType::SEMI) {
            pos++; // consume ';'
        } else {
            // Log it, but don't crash, to keep the tree building
            std::cerr << "[PARSER ERROR] Expected ';' after break at pos " << pos << "\n";
        }

        return std::make_unique<BreakAST>();
    }

    if (tok.type == TokenType::CONTINUE) {
        pos++;
        if (pos < tokens.size() && tokens[pos].type == TokenType::SEMI) pos++;
        else std::cerr << "Expected ';' after continue\n";
        return std::make_unique<ContinueAST>();
    }

    // Assignment / array assignment / expression statement
    /*if (tok.type == TokenType::IDENT) {
        std::string name = tok.lexeme;
        pos++;

            // array assign
        if (pos < tokens.size() && tokens[pos].type == TokenType::LBRACKET) {
            pos++;
            auto idx = expression();
            if (pos < tokens.size() && tokens[pos].type == TokenType::RBRACKET) pos++;
            if (pos < tokens.size() && tokens[pos].type == TokenType::ASSIGN) pos++;
            auto val = expression();
            if (pos < tokens.size() && tokens[pos].type == TokenType::SEMI) pos++;
            else std::cerr << "Expected ';' after array assignment at pos " << pos << "\n";
            return std::make_unique<ArrayAssignAST>(name, std::move(idx), std::move(val));
        }

        // simple assign
        if (pos < tokens.size() && tokens[pos].type == TokenType::ASSIGN) {
            pos++; // =
            auto rhs = parseExpression(0);
            if (!rhs) {
                std::cerr << "Failed to parse RHS of assignment at pos " << pos << "\n";
                return nullptr;
            }
            auto assign = std::make_unique<AssignAST>(name, std::move(rhs));
            if (pos < tokens.size() && tokens[pos].type == TokenType::SEMI) pos++;
            else std::cerr << "Expected ';' after assignment at pos " << pos << "\n";
            return assign;
        }

        // not assignment → rewind and try expression statement
        pos--;
    }*/


    // Expression statement fallback
    auto expr = expression();
    if (tokens[pos].type == TokenType::SEMI) pos++;
    return expr;
}

std::unique_ptr<FunctionAST> Parser::function() {
    if (pos >= tokens.size() || tokens[pos].type != TokenType::INT) {
        std::cerr << "Expected 'int' return type at pos " << pos << "\n";
        return nullptr;
    }
    pos++; // int

    if (pos >= tokens.size() || tokens[pos].type != TokenType::IDENT) {
        std::cerr << "Expected function name at pos " << pos << "\n";
        return nullptr;
    }
    std::string name = tokens[pos++].lexeme;

    if (pos >= tokens.size() || tokens[pos++].type != TokenType::LPAREN) {
        std::cerr << "Expected '(' after function name at pos " << pos << "\n";
        return nullptr;
    }

    std::vector<std::string> args;
    while (pos < tokens.size() && tokens[pos].type != TokenType::RPAREN) {
        if (tokens[pos].type == TokenType::INT) pos++;
        if (tokens[pos].type == TokenType::IDENT) {
            args.push_back(tokens[pos++].lexeme);
        }
        if (tokens[pos].type == TokenType::COMMA) pos++;
    }
    if (pos < tokens.size() && tokens[pos].type == TokenType::RPAREN) pos++;

    auto proto = std::make_unique<PrototypeAST>();
    proto->name = name;
    proto->args = std::move(args);

    auto body = block();
    return std::make_unique<FunctionAST>(std::move(proto), std::move(body));
}

std::unique_ptr<AST> Parser::expression() {
    return parseExpression(0);
}

std::unique_ptr<AST> Parser::parse() {
    Logger::log(Stage::PARSER, "Parsing started");
    auto program = std::make_unique<ProgramAST>();

    while (pos < tokens.size() && tokens[pos].type != TokenType::EOF_TOK) {
        size_t startPos = pos;
        auto fn = function();
        if (fn) {
            program->functions.push_back(std::move(fn));
        } else {
            // Panic mode: try to recover by skipping to next possible function start
            // or end of current block
            if (pos < tokens.size() && tokens[pos].type == TokenType::RBRACE) {
                pos++; // consume stray }
                continue;
            }
            // Skip until next 'int' or EOF
            while (pos < tokens.size() && tokens[pos].type != TokenType::INT &&
                   tokens[pos].type != TokenType::EOF_TOK) {
                pos++;
            }
            if (pos == startPos) {
                // Prevent infinite loop if stuck
                pos++;
            }
        }
    }
    return program;
}