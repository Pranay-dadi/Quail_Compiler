#pragma once
#include <memory>
#include <string>
#include <vector>

struct AST {
    virtual ~AST() = default;
};

struct NumberAST : AST {
    int val;
    NumberAST(int v) : val(v) {}
};

struct VariableAST : AST {
    std::string name;
    VariableAST(std::string n) : name(n) {}
};

struct BinaryAST : AST {
    char op;
    std::unique_ptr<AST> lhs, rhs;
    BinaryAST(char o, std::unique_ptr<AST> l, std::unique_ptr<AST> r)
        : op(o), lhs(std::move(l)), rhs(std::move(r)) {}
};

struct ReturnAST : AST {
    std::unique_ptr<AST> expr;
    ReturnAST(std::unique_ptr<AST> e) : expr(std::move(e)) {}
};

struct VarDeclAST : AST {
    std::string name;
    VarDeclAST(const std::string& n) : name(n) {}
};

struct AssignAST : AST {
    std::string name;
    std::unique_ptr<AST> expr;
    AssignAST(std::string n, std::unique_ptr<AST> e)
        : name(n), expr(std::move(e)) {}
};

struct BlockAST : AST {
    std::vector<std::unique_ptr<AST>> statements;
};

struct IfAST : AST {
    std::unique_ptr<AST> cond, thenBlock, elseBlock;
};

struct WhileAST : AST {
    std::unique_ptr<AST> cond, body;
};


struct PrototypeAST : AST {
    std::string name;
    std::vector<std::string> args;
};

struct FunctionAST : AST {
    std::unique_ptr<PrototypeAST> proto;
    std::unique_ptr<BlockAST> body;

    FunctionAST(std::unique_ptr<PrototypeAST> p,
                std::unique_ptr<BlockAST> b)
        : proto(std::move(p)), body(std::move(b)) {}
};

struct CallAST : AST {
    std::string callee;
    std::vector<std::unique_ptr<AST>> args;
};

struct ArrayDeclAST : AST {
    std::string name;
    int size;

    ArrayDeclAST(const std::string& n, int s)
        : name(n), size(s) {}
};

struct ArrayAccessAST : AST {
    std::string name;
    std::unique_ptr<AST> index;

    ArrayAccessAST(const std::string& n, std::unique_ptr<AST> idx)
        : name(n), index(std::move(idx)) {}
};

