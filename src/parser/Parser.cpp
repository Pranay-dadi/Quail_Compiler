#include "parser/Parser.h"
#include <iostream>
#include <sstream>

Parser::Parser(const std::vector<Token>& t) : tokens(t), pos(0) {}

// ── Helpers ────────────────────────────────────────────────────────────────────

bool Parser::isComment(TokenType t) const {
    return t == TokenType::LINE_COMMENT || t == TokenType::BLOCK_COMMENT;
}

int Parser::currentLine() const {
    if (pos < tokens.size()) return tokens[pos].line;
    return tokens.empty() ? 0 : tokens.back().line;
}

void Parser::addError(const std::string& msg) {
    int ln = currentLine();
    for (auto& e : errors)
        if (e.line == ln && e.message == msg) return;
    errors.push_back({ln, msg});
}

// ── Helper: convert token keyword to ASTType ──────────────────
static ASTType tokenToASTType(TokenType t) {
    if (t == TokenType::FLOAT) return ASTType::Float;
    return ASTType::Int;   // INT and any other keyword → int
}

// ── Drain comment tokens ──────────────────────────────────────
std::vector<std::unique_ptr<AST>> Parser::drainComments() {
    std::vector<std::unique_ptr<AST>> out;
    while (pos < tokens.size() && isComment(tokens[pos].type)) {
        const Token& tok = tokens[pos++];
        if (tok.type == TokenType::LINE_COMMENT)
            out.push_back(std::make_unique<LineCommentAST>(tok.lexeme));
        else
            out.push_back(std::make_unique<BlockCommentAST>(tok.lexeme));
    }
    return out;
}

// ── Recovery ──────────────────────────────────────────────────

void Parser::syncStatement() {
    while (pos < tokens.size()) {
        TokenType t = tokens[pos].type;
        if (t == TokenType::SEMI)    { pos++; return; }
        if (t == TokenType::RBRACE)  return;
        if (t == TokenType::EOF_TOK) return;
        pos++;
    }
}

void Parser::syncFunction() {
    while (pos + 2 < tokens.size()) {
        if ((tokens[pos].type   == TokenType::INT  ||
             tokens[pos].type   == TokenType::FLOAT) &&
            tokens[pos+1].type == TokenType::IDENT &&
            tokens[pos+2].type == TokenType::LPAREN)
            return;
        if (tokens[pos].type == TokenType::EOF_TOK) return;
        pos++;
    }
    while (pos < tokens.size() && tokens[pos].type != TokenType::EOF_TOK) pos++;
}

// ── Operator precedence ────────────────────────────────────────────────────────

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

// ── Primary ────────────────────────────────────────────────────────────────────

std::unique_ptr<AST> Parser::primary() {
    if (pos >= tokens.size()) return nullptr;

    while (pos < tokens.size() && isComment(tokens[pos].type)) pos++;

    const Token& tok = tokens[pos];

    if (tok.type == TokenType::ASSIGN) {
        addError("Unexpected '=' in expression"); pos++; return nullptr;
    }

    if (tok.type == TokenType::NUMBER) {
        int val = std::stoi(tok.lexeme); pos++;
        return std::make_unique<NumberAST>(val);
    }

    if (tok.type == TokenType::FLOAT_VAL) {
        double val = std::stod(tok.lexeme); pos++;
        return std::make_unique<FloatAST>(val);
    }

    if (tok.type == TokenType::IDENT) {
        std::string name = tok.lexeme; pos++;

        // Post-increment
        if (pos < tokens.size() && tokens[pos].type == TokenType::INC) {
            pos++; return std::make_unique<PostIncAST>(name);
        }
        // Function call
        if (pos < tokens.size() && tokens[pos].type == TokenType::LPAREN) {
            pos++;
            std::vector<std::unique_ptr<AST>> args;
            while (pos < tokens.size()
                   && tokens[pos].type != TokenType::RPAREN
                   && tokens[pos].type != TokenType::EOF_TOK) {
                while (pos < tokens.size() && isComment(tokens[pos].type)) pos++;
                if (pos < tokens.size() && tokens[pos].type == TokenType::RPAREN) break;
                auto arg = expression();
                if (arg) args.push_back(std::move(arg));
                if (pos < tokens.size() && tokens[pos].type == TokenType::COMMA) pos++;
            }
            if (pos < tokens.size() && tokens[pos].type == TokenType::RPAREN) pos++;
            else addError("Missing ')' in call to '" + name + "'");
            auto call = std::make_unique<CallAST>();
            call->callee = name; call->args = std::move(args);
            return call;
        }
        // Array access
        if (pos < tokens.size() && tokens[pos].type == TokenType::LBRACKET) {
            pos++;
            auto idx = expression();
            if (!idx) { addError("Invalid index for '" + name + "'"); syncStatement(); return nullptr; }
            if (pos < tokens.size() && tokens[pos].type == TokenType::RBRACKET) pos++;
            else addError("Missing ']' in array access '" + name + "[...]'");
            return std::make_unique<ArrayAccessAST>(name, std::move(idx));
        }
        return std::make_unique<VariableAST>(name);
    }

    if (tok.type == TokenType::LPAREN) {
        pos++;
        auto expr = expression();
        if (pos < tokens.size() && tokens[pos].type == TokenType::RPAREN) pos++;
        else addError("Missing ')' in parenthesised expression");
        return expr;
    }

    if (tok.type == TokenType::MINUS || tok.type == TokenType::NOT) {
        std::string op = (tok.type == TokenType::MINUS) ? "-" : "!";
        pos++;
        auto operand = primary();
        if (!operand) { addError("Expected expression after unary '" + op + "'"); return nullptr; }
        auto u = std::make_unique<UnaryAST>();
        u->op = op; u->operand = std::move(operand);
        return u;
    }

    addError("Unexpected token '" + tok.lexeme + "' in expression");
    pos++;
    return nullptr;
}

// ── Pratt expression parser ────────────────────────────────────────────────────

std::unique_ptr<AST> Parser::parseExpression(int minPrec) {
    auto lhs = primary();
    if (!lhs) return nullptr;

    while (pos < tokens.size()) {
        while (pos < tokens.size() && isComment(tokens[pos].type)) pos++;
        if (pos >= tokens.size()) break;

        TokenType opType = tokens[pos].type;
        int prec = getPrecedence(opType);
        if (prec < minPrec) break;

        std::string opStr = tokens[pos++].lexeme;
        auto rhs = parseExpression(prec + 1);
        if (!rhs) { addError("Expected right-hand side after '" + opStr + "'"); break; }

        if (opType == TokenType::AND || opType == TokenType::OR ||
            opType == TokenType::EQ  || opType == TokenType::NEQ) {
            auto log = std::make_unique<LogicalAST>();
            log->op = opStr; log->lhs = std::move(lhs); log->rhs = std::move(rhs);
            lhs = std::move(log);
        } else {
            auto bin = std::make_unique<BinaryAST>();
            bin->op = opStr; bin->lhs = std::move(lhs); bin->rhs = std::move(rhs);
            lhs = std::move(bin);
        }
    }
    return lhs;
}

std::unique_ptr<AST> Parser::expression() { return parseExpression(0); }

// ── Block ──────────────────────────────────────────────────────────────────────

std::unique_ptr<BlockAST> Parser::block() {
    if (pos < tokens.size() && tokens[pos].type == TokenType::LBRACE) pos++;
    else addError("Expected '{' to open block");

    auto b = std::make_unique<BlockAST>();

    while (pos < tokens.size()
           && tokens[pos].type != TokenType::RBRACE
           && tokens[pos].type != TokenType::EOF_TOK)
    {
        for (auto& c : drainComments())
            b->statements.push_back(std::move(c));

        if (pos >= tokens.size()
            || tokens[pos].type == TokenType::RBRACE
            || tokens[pos].type == TokenType::EOF_TOK) break;

        auto stmt = statement();
        if (stmt) {
            b->statements.push_back(std::move(stmt));
        } else {
            syncStatement();
        }
    }

    if (pos < tokens.size() && tokens[pos].type == TokenType::RBRACE) pos++;
    else addError("Missing '}' at end of block");

    return b;
}

// ── Statement ──────────────────────────────────────────────────────────────────

std::unique_ptr<AST> Parser::statement() {
    if (pos >= tokens.size()) return nullptr;

    if (isComment(tokens[pos].type)) {
        const Token& t = tokens[pos++];
        if (t.type == TokenType::LINE_COMMENT)
            return std::make_unique<LineCommentAST>(t.lexeme);
        return std::make_unique<BlockCommentAST>(t.lexeme);
    }

    const Token& tok = tokens[pos];

    if (tok.type == TokenType::LBRACE)
        return block();

    // Array assignment: name[idx] = expr;
    if (tok.type == TokenType::IDENT
        && pos + 1 < tokens.size()
        && tokens[pos+1].type == TokenType::LBRACKET)
    {
        std::string name = tokens[pos].lexeme; pos++; pos++;
        auto idx = expression();
        if (!idx) { addError("Invalid index in assignment to '" + name + "[...]'"); syncStatement(); return nullptr; }
        if (pos < tokens.size() && tokens[pos].type == TokenType::RBRACKET) pos++;
        else addError("Missing ']' in array assignment to '" + name + "'");
        if (pos >= tokens.size() || tokens[pos].type != TokenType::ASSIGN) {
            addError("Expected '=' after '" + name + "[...]'"); syncStatement(); return nullptr; }
        pos++;
        auto val = expression();
        if (!val) { addError("Invalid value in assignment to '" + name + "[...]'"); syncStatement(); return nullptr; }
        if (pos < tokens.size() && tokens[pos].type == TokenType::SEMI) pos++;
        else addError("Missing ';' after array assignment to '" + name + "[...]'");
        return std::make_unique<ArrayAssignAST>(name, std::move(idx), std::move(val));
    }

    // Simple assignment: name = expr;
    if (tok.type == TokenType::IDENT
        && pos + 1 < tokens.size()
        && tokens[pos+1].type == TokenType::ASSIGN)
    {
        std::string name = tokens[pos].lexeme; pos += 2;
        auto val = expression();
        if (!val) { addError("Invalid expression in assignment to '" + name + "'"); syncStatement(); return nullptr; }
        if (pos < tokens.size() && tokens[pos].type == TokenType::SEMI) pos++;
        else addError("Missing ';' after assignment to '" + name + "'");
        return std::make_unique<AssignAST>(name, std::move(val));
    }

    // ── Variable / array declaration  ← FIXED: now passes type to AST node ──
    if (tok.type == TokenType::INT || tok.type == TokenType::FLOAT) {
        ASTType declType = tokenToASTType(tok.type);
        pos++;
        if (pos >= tokens.size() || tokens[pos].type != TokenType::IDENT) {
            addError("Expected variable name after type keyword"); syncStatement(); return nullptr; }
        std::string name = tokens[pos++].lexeme;

        // Array declaration
        if (pos < tokens.size() && tokens[pos].type == TokenType::LBRACKET) {
            pos++;
            if (pos >= tokens.size() || tokens[pos].type != TokenType::NUMBER) {
                addError("Expected size in array declaration '" + name + "[...]'"); syncStatement(); return nullptr; }
            int size = std::stoi(tokens[pos++].lexeme);
            if (size <= 0) {
                addError("Array size must be positive in declaration '" + name + "[...]'");
                syncStatement(); return nullptr;
            }
            if (pos < tokens.size() && tokens[pos].type == TokenType::RBRACKET) pos++;
            else addError("Missing ']' in array declaration '" + name + "[...]'");
            if (pos < tokens.size() && tokens[pos].type == TokenType::SEMI) pos++;
            else addError("Missing ';' after array declaration '" + name + "[...]'");
            // Pass type to ArrayDeclAST
            return std::make_unique<ArrayDeclAST>(name, size, declType);
        }

        // Scalar declaration with optional initializer: int x = expr;
        if (pos < tokens.size() && tokens[pos].type == TokenType::ASSIGN) {
            pos++;
            auto initExpr = expression();
            if (!initExpr) {
                addError("Expected initializer expression for '" + name + "'");
                syncStatement(); return nullptr;
            }
            if (pos < tokens.size() && tokens[pos].type == TokenType::SEMI) pos++;
            else addError("Missing ';' after declaration of '" + name + "'");

            // Emit: VarDecl + Assign as a block
            auto blk = std::make_unique<BlockAST>();
            blk->statements.push_back(std::make_unique<VarDeclAST>(name, declType));
            blk->statements.push_back(std::make_unique<AssignAST>(name, std::move(initExpr)));
            return blk;
        }

        // Plain declaration: int x;
        if (pos < tokens.size() && tokens[pos].type == TokenType::SEMI) {
            pos++;
            return std::make_unique<VarDeclAST>(name, declType);
        }
        addError("Missing ';' after variable declaration '" + name + "'");
        syncStatement(); return nullptr;
    }

    // return
    if (tok.type == TokenType::RETURN) {
        pos++;
        while (pos < tokens.size() && isComment(tokens[pos].type)) pos++;
        std::unique_ptr<AST> expr;
        if (pos < tokens.size() && tokens[pos].type != TokenType::SEMI)
            expr = expression();
        if (pos < tokens.size() && tokens[pos].type == TokenType::SEMI) pos++;
        else addError("Missing ';' after return statement");
        return std::make_unique<ReturnAST>(std::move(expr));
    }

    // if / if-else
    if (tok.type == TokenType::IF) {
        pos++;
        if (pos < tokens.size() && tokens[pos].type == TokenType::LPAREN) pos++;
        else addError("Expected '(' after 'if'");
        auto cond = expression();
        if (!cond) { addError("Invalid 'if' condition"); syncStatement(); return nullptr; }
        if (pos < tokens.size() && tokens[pos].type == TokenType::RPAREN) pos++;
        else addError("Missing ')' after 'if' condition");
        auto thenB = block();
        std::unique_ptr<AST> elseB;
        while (pos < tokens.size() && isComment(tokens[pos].type)) pos++;
        if (pos < tokens.size() && tokens[pos].type == TokenType::ELSE) { pos++; elseB = block(); }
        auto node = std::make_unique<IfAST>();
        node->cond = std::move(cond); node->thenBlock = std::move(thenB); node->elseBlock = std::move(elseB);
        return node;
    }

    // while
    if (tok.type == TokenType::WHILE) {
        pos++;
        if (pos < tokens.size() && tokens[pos].type == TokenType::LPAREN) pos++;
        else addError("Expected '(' after 'while'");
        auto cond = expression();
        if (!cond) { addError("Invalid 'while' condition"); syncStatement(); return nullptr; }
        if (pos < tokens.size() && tokens[pos].type == TokenType::RPAREN) pos++;
        else addError("Missing ')' after 'while' condition");
        auto body = block();
        auto node = std::make_unique<WhileAST>();
        node->cond = std::move(cond); node->body = std::move(body);
        return node;
    }

    // for
    if (tok.type == TokenType::FOR) {
        pos++;
        if (pos < tokens.size() && tokens[pos].type == TokenType::LPAREN) pos++;
        else addError("Expected '(' after 'for'");

        std::unique_ptr<AST> init;
        if (pos < tokens.size() && tokens[pos].type == TokenType::IDENT
            && pos + 1 < tokens.size() && tokens[pos+1].type == TokenType::ASSIGN) {
            std::string name = tokens[pos].lexeme; pos += 2;
            auto val = expression();
            if (val) init = std::make_unique<AssignAST>(name, std::move(val));
            else addError("Invalid initializer in 'for' loop");
        } else if (pos < tokens.size() && tokens[pos].type != TokenType::SEMI) {
            init = expression();
        }
        if (pos < tokens.size() && tokens[pos].type == TokenType::SEMI) pos++;
        else addError("Missing ';' after 'for' initializer");

        auto cond = expression();
        if (pos < tokens.size() && tokens[pos].type == TokenType::SEMI) pos++;
        else addError("Missing ';' after 'for' condition");

        auto inc = expression();
        if (pos < tokens.size() && tokens[pos].type == TokenType::RPAREN) pos++;
        else addError("Missing ')' after 'for' increment");

        auto body = block();
        return std::make_unique<ForAST>(std::move(init), std::move(cond),
                                        std::move(inc), std::move(body));
    }

    // break / continue
    if (tok.type == TokenType::BREAK) {
        pos++;
        if (pos < tokens.size() && tokens[pos].type == TokenType::SEMI) pos++;
        else addError("Missing ';' after 'break'");
        return std::make_unique<BreakAST>();
    }
    if (tok.type == TokenType::CONTINUE) {
        pos++;
        if (pos < tokens.size() && tokens[pos].type == TokenType::SEMI) pos++;
        else addError("Missing ';' after 'continue'");
        return std::make_unique<ContinueAST>();
    }

    // Expression statement
    auto expr = expression();
    if (!expr) {
        addError("Unrecognised statement starting with '" + tok.lexeme + "'");
        syncStatement(); return nullptr;
    }
    if (pos < tokens.size() && tokens[pos].type == TokenType::SEMI) pos++;
    else addError("Missing ';' after expression statement");
    return expr;
}

// ── Function definition ────────────────────────────────────────────────────────

std::unique_ptr<FunctionAST> Parser::function() {
    // Accept int or float as return type
    if (pos >= tokens.size() ||
        (tokens[pos].type != TokenType::INT && tokens[pos].type != TokenType::FLOAT)) {
        addError("Expected return type (int/float) for function definition");
        return nullptr;
    }
    ASTType retType = tokenToASTType(tokens[pos].type);
    pos++;

    if (pos >= tokens.size() || tokens[pos].type != TokenType::IDENT) {
        addError("Expected function name after return type"); return nullptr; }
    std::string name = tokens[pos++].lexeme;

    if (pos >= tokens.size() || tokens[pos].type != TokenType::LPAREN) {
        addError("Expected '(' after function name '" + name + "'"); return nullptr; }
    pos++;

    std::vector<std::string> args;
    std::vector<ASTType>     argTypes;

    while (pos < tokens.size()
           && tokens[pos].type != TokenType::RPAREN
           && tokens[pos].type != TokenType::EOF_TOK)
    {
        ASTType paramType = ASTType::Int;
        if (tokens[pos].type == TokenType::INT ||
            tokens[pos].type == TokenType::FLOAT) {
            paramType = tokenToASTType(tokens[pos].type);
            pos++;
        }
        if (pos < tokens.size() && tokens[pos].type == TokenType::IDENT) {
            args.push_back(tokens[pos++].lexeme);
            argTypes.push_back(paramType);
        } else {
            addError("Expected parameter name in '" + name + "'"); break;
        }
        if (pos < tokens.size() && tokens[pos].type == TokenType::COMMA) pos++;
    }

    if (pos >= tokens.size() || tokens[pos].type != TokenType::RPAREN)
        addError("Missing ')' in parameter list of '" + name + "'");
    else pos++;

    auto proto        = std::make_unique<PrototypeAST>();
    proto->name       = name;
    proto->args       = std::move(args);
    proto->argTypes   = std::move(argTypes);
    proto->returnType = retType;

    auto body = block();
    return std::make_unique<FunctionAST>(std::move(proto), std::move(body));
}

// ── Top-level parse ────────────────────────────────────────────────────────────

std::unique_ptr<AST> Parser::parse() {
    auto program = std::make_unique<ProgramAST>();

    while (pos < tokens.size() && tokens[pos].type != TokenType::EOF_TOK) {
        auto comments = drainComments();
        for (auto& c : comments)
            program->topLevel.push_back(std::move(c));

        if (pos >= tokens.size() || tokens[pos].type == TokenType::EOF_TOK) break;

        size_t before = pos;
        auto fn = function();
        if (fn) {
            program->topLevel.push_back(std::move(fn));
        } else {
            if (pos == before) pos++;
            syncFunction();
        }
    }
    return program;
}