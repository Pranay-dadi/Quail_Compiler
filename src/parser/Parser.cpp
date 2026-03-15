#include "parser/Parser.h"
#include <iostream>
#include <sstream>

Parser::Parser(const std::vector<Token>& t) : tokens(t), pos(0) {}

// ── Helpers ────────────────────────────────────────────────────

bool Parser::isComment(TokenType t) const {
    return t == TokenType::LINE_COMMENT || t == TokenType::BLOCK_COMMENT;
}

bool Parser::isTypeKeyword(TokenType t) const {
    return t == TokenType::INT || t == TokenType::FLOAT || t == TokenType::VOID;
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

static ASTType tokenToASTType(TokenType t) {
    if (t == TokenType::FLOAT) return ASTType::Float;
    if (t == TokenType::VOID)  return ASTType::Void;
    return ASTType::Int;
}

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
        if (isTypeKeyword(tokens[pos].type) &&
            tokens[pos+1].type == TokenType::IDENT &&
            tokens[pos+2].type == TokenType::LPAREN)
            return;
        if (tokens[pos].type == TokenType::CLASS) return;
        if (tokens[pos].type == TokenType::EOF_TOK) return;
        pos++;
    }
    while (pos < tokens.size() && tokens[pos].type != TokenType::EOF_TOK) pos++;
}

// ── Operator precedence ────────────────────────────────────────

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

// ── Argument list parser (LPAREN already consumed) ─────────────
std::vector<std::unique_ptr<AST>> Parser::parseArgList() {
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
    return args;
}

// ── Primary ────────────────────────────────────────────────────

std::unique_ptr<AST> Parser::primary() {
    if (pos >= tokens.size()) return nullptr;

    while (pos < tokens.size() && isComment(tokens[pos].type)) pos++;
    if (pos >= tokens.size()) return nullptr;

    const Token& tok = tokens[pos];

    // ── this.field  /  this.method(args) ──────────────────────
    if (tok.type == TokenType::THIS) {
        pos++;
        if (pos >= tokens.size() || tokens[pos].type != TokenType::DOT) {
            addError("Expected '.' after 'this'");
            return nullptr;
        }
        pos++; // consume DOT
        if (pos >= tokens.size() || tokens[pos].type != TokenType::IDENT) {
            addError("Expected member name after 'this.'");
            return nullptr;
        }
        std::string member = tokens[pos++].lexeme;
        // this.method(args)?
        if (pos < tokens.size() && tokens[pos].type == TokenType::LPAREN) {
            pos++;
            auto args = parseArgList();
            if (pos < tokens.size() && tokens[pos].type == TokenType::RPAREN) pos++;
            else addError("Missing ')' in this." + member + "()");
            return std::make_unique<MethodCallAST>("this", member, std::move(args));
        }
        return std::make_unique<ThisAccessAST>(member);
    }

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

        // Post-increment  x++
        if (pos < tokens.size() && tokens[pos].type == TokenType::INC) {
            pos++; return std::make_unique<PostIncAST>(name);
        }

        // Member access / method call  obj.field  obj.method(args)
        if (pos < tokens.size() && tokens[pos].type == TokenType::DOT) {
            pos++; // consume DOT
            if (pos >= tokens.size() || tokens[pos].type != TokenType::IDENT) {
                addError("Expected member name after '.' on '" + name + "'");
                return nullptr;
            }
            std::string member = tokens[pos++].lexeme;
            if (pos < tokens.size() && tokens[pos].type == TokenType::LPAREN) {
                pos++;
                auto args = parseArgList();
                if (pos < tokens.size() && tokens[pos].type == TokenType::RPAREN) pos++;
                else addError("Missing ')' in " + name + "." + member + "()");
                return std::make_unique<MethodCallAST>(name, member, std::move(args));
            }
            return std::make_unique<MemberAccessAST>(name, member);
        }

        // Function call  name(args)
        if (pos < tokens.size() && tokens[pos].type == TokenType::LPAREN) {
            pos++;
            auto args = parseArgList();
            if (pos < tokens.size() && tokens[pos].type == TokenType::RPAREN) pos++;
            else addError("Missing ')' in call to '" + name + "'");
            auto call = std::make_unique<CallAST>();
            call->callee = name; call->args = std::move(args);
            return call;
        }

        // Array access  name[idx]
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

// ── Pratt expression parser ────────────────────────────────────

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

// ── Block ──────────────────────────────────────────────────────

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
        if (stmt) b->statements.push_back(std::move(stmt));
        else      syncStatement();
    }

    if (pos < tokens.size() && tokens[pos].type == TokenType::RBRACE) pos++;
    else addError("Missing '}' at end of block");

    return b;
}

// ── Statement ──────────────────────────────────────────────────

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

    // ── this.field = expr;  (inside method) ───────────────────
    if (tok.type == TokenType::THIS
        && pos+1 < tokens.size() && tokens[pos+1].type == TokenType::DOT)
    {
        size_t savedPos = pos;
        pos += 2; // skip this + .
        if (pos < tokens.size() && tokens[pos].type == TokenType::IDENT) {
            std::string field = tokens[pos++].lexeme;
            if (pos < tokens.size() && tokens[pos].type == TokenType::ASSIGN) {
                pos++;
                auto val = expression();
                if (!val) { addError("Invalid rhs in 'this." + field + " = ...'"); syncStatement(); return nullptr; }
                if (pos < tokens.size() && tokens[pos].type == TokenType::SEMI) pos++;
                else addError("Missing ';' after this." + field + " assignment");
                return std::make_unique<ThisAssignAST>(field, std::move(val));
            }
        }
        // Not an assignment — fall through to expression statement
        pos = savedPos;
    }

    // ── obj.field = expr;  (member assignment) ────────────────
    if (tok.type == TokenType::IDENT
        && pos+1 < tokens.size() && tokens[pos+1].type == TokenType::DOT)
    {
        // Look ahead: IDENT DOT IDENT ASSIGN
        size_t savedPos = pos;
        std::string objName = tokens[pos].lexeme; pos += 2; // skip obj + .
        if (pos < tokens.size() && tokens[pos].type == TokenType::IDENT) {
            std::string member = tokens[pos++].lexeme;
            if (pos < tokens.size() && tokens[pos].type == TokenType::ASSIGN) {
                pos++;
                auto val = expression();
                if (!val) { addError("Invalid rhs in '" + objName + "." + member + " = ...'"); syncStatement(); return nullptr; }
                if (pos < tokens.size() && tokens[pos].type == TokenType::SEMI) pos++;
                else addError("Missing ';' after " + objName + "." + member + " assignment");
                return std::make_unique<MemberAssignAST>(objName, member, std::move(val));
            }
        }
        // Not a member assignment — restore and fall through
        pos = savedPos;
    }

    // ── Object declaration  ClassName varName; ────────────────
    if (tok.type == TokenType::IDENT
        && classNames.count(tok.lexeme)
        && pos+1 < tokens.size() && tokens[pos+1].type == TokenType::IDENT)
    {
        std::string className = tokens[pos].lexeme;
        std::string varName   = tokens[pos+1].lexeme;
        pos += 2;
        if (pos < tokens.size() && tokens[pos].type == TokenType::SEMI) pos++;
        else addError("Missing ';' after object declaration '" + className + " " + varName + "'");
        return std::make_unique<ObjectDeclAST>(className, varName);
    }

    // ── Array assignment  name[idx] = expr; ───────────────────
    if (tok.type == TokenType::IDENT
        && pos+1 < tokens.size()
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

    // ── Simple assignment  name = expr; ───────────────────────
    if (tok.type == TokenType::IDENT
        && pos+1 < tokens.size()
        && tokens[pos+1].type == TokenType::ASSIGN)
    {
        std::string name = tokens[pos].lexeme; pos += 2;
        auto val = expression();
        if (!val) { addError("Invalid expression in assignment to '" + name + "'"); syncStatement(); return nullptr; }
        if (pos < tokens.size() && tokens[pos].type == TokenType::SEMI) pos++;
        else addError("Missing ';' after assignment to '" + name + "'");
        return std::make_unique<AssignAST>(name, std::move(val));
    }

    // ── Variable / array declaration ──────────────────────────
    if (isTypeKeyword(tok.type)) {
        ASTType declType = tokenToASTType(tok.type);
        pos++;
        if (pos >= tokens.size() || tokens[pos].type != TokenType::IDENT) {
            addError("Expected variable name after type keyword"); syncStatement(); return nullptr; }
        std::string name = tokens[pos++].lexeme;

        // Array declaration  type name[size];
        if (pos < tokens.size() && tokens[pos].type == TokenType::LBRACKET) {
            pos++;
            if (pos >= tokens.size() || tokens[pos].type != TokenType::NUMBER) {
                addError("Expected size in array declaration '" + name + "[...]'"); syncStatement(); return nullptr; }
            int size = std::stoi(tokens[pos++].lexeme);
            if (size <= 0) { addError("Array size must be positive"); syncStatement(); return nullptr; }
            if (pos < tokens.size() && tokens[pos].type == TokenType::RBRACKET) pos++;
            else addError("Missing ']' in array declaration '" + name + "[...]'");
            if (pos < tokens.size() && tokens[pos].type == TokenType::SEMI) pos++;
            else addError("Missing ';' after array declaration '" + name + "[...]'");
            return std::make_unique<ArrayDeclAST>(name, size, declType);
        }

        // Declaration with initialiser  type name = expr;
        if (pos < tokens.size() && tokens[pos].type == TokenType::ASSIGN) {
            pos++;
            auto initExpr = expression();
            if (!initExpr) { addError("Expected initializer for '" + name + "'"); syncStatement(); return nullptr; }
            if (pos < tokens.size() && tokens[pos].type == TokenType::SEMI) pos++;
            else addError("Missing ';' after declaration of '" + name + "'");
            // Use VarDeclInitAST so the variable stays in the CURRENT scope,
            // not a child scope (the old BlockAST wrapper caused "undeclared variable" errors).
            return std::make_unique<VarDeclInitAST>(name, declType, std::move(initExpr));
        }

        // Plain declaration  type name;
        if (pos < tokens.size() && tokens[pos].type == TokenType::SEMI) {
            pos++;
            return std::make_unique<VarDeclAST>(name, declType);
        }
        addError("Missing ';' after variable declaration '" + name + "'");
        syncStatement(); return nullptr;
    }

    // ── return ─────────────────────────────────────────────────
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

    // ── if / if-else ───────────────────────────────────────────
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

    // ── while ──────────────────────────────────────────────────
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

    // ── for ────────────────────────────────────────────────────
    if (tok.type == TokenType::FOR) {
        pos++;
        if (pos < tokens.size() && tokens[pos].type == TokenType::LPAREN) pos++;
        else addError("Expected '(' after 'for'");
        std::unique_ptr<AST> init;
        if (pos < tokens.size() && tokens[pos].type == TokenType::IDENT
            && pos+1 < tokens.size() && tokens[pos+1].type == TokenType::ASSIGN) {
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

    // ── break / continue ───────────────────────────────────────
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

    // ── Expression statement (inc. method calls, post-inc) ─────
    auto expr = expression();
    if (!expr) {
        addError("Unrecognised statement starting with '" + tok.lexeme + "'");
        syncStatement(); return nullptr;
    }
    if (pos < tokens.size() && tokens[pos].type == TokenType::SEMI) pos++;
    else addError("Missing ';' after expression statement");
    return expr;
}

// ── Function definition ────────────────────────────────────────

std::unique_ptr<FunctionAST> Parser::function() {
    if (pos >= tokens.size() || !isTypeKeyword(tokens[pos].type)) {
        addError("Expected return type (int/float/void) for function");
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
        if (isTypeKeyword(tokens[pos].type)) {
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

// ── Class definition ───────────────────────────────────────────

std::unique_ptr<ClassDeclAST> Parser::parseClass() {
    // consume 'class'
    pos++;

    if (pos >= tokens.size() || tokens[pos].type != TokenType::IDENT) {
        addError("Expected class name after 'class'");
        return nullptr;
    }
    std::string name = tokens[pos++].lexeme;
    classNames.insert(name);  // register so object-decl parsing works inside other classes

    if (pos >= tokens.size() || tokens[pos].type != TokenType::LBRACE) {
        addError("Expected '{' after class name '" + name + "'");
        return nullptr;
    }
    pos++; // consume '{'

    auto cls = std::make_unique<ClassDeclAST>();
    cls->name = name;

    while (pos < tokens.size()
           && tokens[pos].type != TokenType::RBRACE
           && tokens[pos].type != TokenType::EOF_TOK)
    {
        // Drain comments
        if (isComment(tokens[pos].type)) { pos++; continue; }

        // Skip access modifiers: public / private  (no enforcement)
        if (tokens[pos].type == TokenType::PUBLIC || tokens[pos].type == TokenType::PRIVATE) {
            pos++; continue;
        }

        // Member must start with a type keyword
        if (!isTypeKeyword(tokens[pos].type)) {
            addError("Expected type keyword in class '" + name + "' body");
            pos++; continue;
        }

        // Peek: type IDENT '(' → method;  type IDENT ';' → field
        size_t savedPos = pos;
        ASTType declType = tokenToASTType(tokens[pos].type);
        pos++;
        if (pos >= tokens.size() || tokens[pos].type != TokenType::IDENT) {
            addError("Expected member name in class '" + name + "'");
            syncStatement(); continue;
        }
        std::string memberName = tokens[pos++].lexeme;

        if (pos < tokens.size() && tokens[pos].type == TokenType::LPAREN) {
            // Method — back up to type token and call function()
            pos = savedPos;
            auto method = function();
            if (method) cls->methods.push_back(std::move(method));
        } else if (pos < tokens.size() && tokens[pos].type == TokenType::SEMI) {
            // Field
            pos++;
            cls->fields.push_back({memberName, declType});
        } else {
            addError("Expected ';' or '(' after member '" + memberName + "' in class '" + name + "'");
            syncStatement();
        }
    }

    if (pos < tokens.size() && tokens[pos].type == TokenType::RBRACE) pos++;
    else addError("Missing '}' at end of class '" + name + "'");

    return cls;
}

// ── Top-level parse ────────────────────────────────────────────

std::unique_ptr<AST> Parser::parse() {
    auto program = std::make_unique<ProgramAST>();

    while (pos < tokens.size() && tokens[pos].type != TokenType::EOF_TOK) {
        auto comments = drainComments();
        for (auto& c : comments)
            program->topLevel.push_back(std::move(c));

        if (pos >= tokens.size() || tokens[pos].type == TokenType::EOF_TOK) break;

        // Class definition
        if (tokens[pos].type == TokenType::CLASS) {
            auto cls = parseClass();
            if (cls) program->topLevel.push_back(std::move(cls));
            else syncFunction();
            continue;
        }

        // Function definition
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
