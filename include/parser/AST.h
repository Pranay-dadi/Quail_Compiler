#ifndef AST_H
#define AST_H
#include <memory>
#include <string>
#include <vector>
#include <iostream>

// ── Language value types (mirrored from SymbolTable for AST use) ──
// We redeclare here so AST.h has no dependency on LLVM headers.
enum class ASTType {
    Int,
    Float,
    Void,
    Unknown
};

static inline std::string astTypeName(ASTType t) {
    switch (t) {
        case ASTType::Int:     return "int";
        case ASTType::Float:   return "float";
        case ASTType::Void:    return "void";
        case ASTType::Unknown: return "<unknown>";
    }
    return "<unknown>";
}

// ── Base ──────────────────────────────────────────────────────
struct AST {
    virtual ~AST() = default;
    virtual void print(int indent = 0) const = 0;
    virtual bool isNoOp() const { return false; }
};

// ─────────────────────────────────────────────────────────────
//  COMMENTS
// ─────────────────────────────────────────────────────────────

struct LineCommentAST : AST {
    std::string text;
    explicit LineCommentAST(std::string t) : text(std::move(t)) {}
    bool isNoOp() const override { return true; }
    void print(int indent) const override {
        std::cout << std::string(indent, ' ') << "// " << text << "\n";
    }
};

struct BlockCommentAST : AST {
    std::string text;
    explicit BlockCommentAST(std::string t) : text(std::move(t)) {}
    bool isNoOp() const override { return true; }
    void print(int indent) const override {
        std::cout << std::string(indent, ' ') << "/* " << text << " */\n";
    }
};

// ─────────────────────────────────────────────────────────────
//  Literals
// ─────────────────────────────────────────────────────────────

struct NumberAST : AST {
    int val;
    explicit NumberAST(int v) : val(v) {}
    void print(int indent) const override {
        std::cout << std::string(indent, ' ') << "Number(int): " << val << "\n";
    }
};

struct FloatAST : AST {
    double val;
    explicit FloatAST(double v) : val(v) {}
    void print(int indent) const override {
        std::cout << std::string(indent, ' ') << "Number(float): " << val << "\n";
    }
};

struct VariableAST : AST {
    std::string name;
    explicit VariableAST(std::string n) : name(std::move(n)) {}
    void print(int indent) const override {
        std::cout << std::string(indent, ' ') << "Variable: " << name << "\n";
    }
};

// ─────────────────────────────────────────────────────────────
//  Expressions
// ─────────────────────────────────────────────────────────────

struct BinaryAST : AST {
    std::string op;
    std::unique_ptr<AST> lhs, rhs;
    void print(int indent) const override {
        std::cout << std::string(indent, ' ') << "BinaryOp: " << op << "\n";
        lhs->print(indent + 4);
        rhs->print(indent + 4);
    }
};

struct LogicalAST : AST {
    std::string op;
    std::unique_ptr<AST> lhs, rhs;
    void print(int indent) const override {
        std::cout << std::string(indent, ' ') << "LogicalOp: " << op << "\n";
        if (lhs) lhs->print(indent + 2);
        if (rhs) rhs->print(indent + 2);
    }
};

struct UnaryAST : AST {
    std::string op;
    std::unique_ptr<AST> operand;
    void print(int indent) const override {
        std::cout << std::string(indent, ' ') << "UnaryOp: " << op << "\n";
        operand->print(indent + 4);
    }
};

struct PostIncAST : AST {
    std::string name;
    explicit PostIncAST(std::string n) : name(std::move(n)) {}
    void print(int indent) const override {
        std::cout << std::string(indent, ' ') << "PostIncrement: " << name << "++\n";
    }
};

// ─────────────────────────────────────────────────────────────
//  Statements
// ─────────────────────────────────────────────────────────────

struct ReturnAST : AST {
    std::unique_ptr<AST> expr;
    explicit ReturnAST(std::unique_ptr<AST> e) : expr(std::move(e)) {}
    void print(int indent) const override {
        std::cout << std::string(indent, ' ') << "ReturnStmt\n";
        if (expr) expr->print(indent + 4);
    }
};

// ── VarDeclAST now carries its declared type ──────────────────
struct VarDeclAST : AST {
    std::string name;
    ASTType     type;   // int or float

    explicit VarDeclAST(const std::string& n, ASTType t = ASTType::Int)
        : name(n), type(t) {}

    void print(int indent) const override {
        std::cout << std::string(indent, ' ')
                  << "VarDecl: " << astTypeName(type) << " " << name << "\n";
    }
};

struct AssignAST : AST {
    std::string name;
    std::unique_ptr<AST> expr;
    AssignAST(std::string n, std::unique_ptr<AST> v)
        : name(std::move(n)), expr(std::move(v)) {}
    void print(int indent) const override {
        std::cout << std::string(indent, ' ') << "Assignment: " << name << " =\n";
        if (expr) expr->print(indent + 4);
    }
};

struct BlockAST : AST {
    std::vector<std::unique_ptr<AST>> statements;
    void print(int indent) const override {
        std::cout << std::string(indent, ' ') << "Block:\n";
        for (const auto& stmt : statements) stmt->print(indent + 2);
    }
};

struct IfAST : AST {
    std::unique_ptr<AST> cond, thenBlock, elseBlock;
    void print(int indent) const override {
        std::string sp(indent, ' ');
        std::cout << sp << "IfStatement\n" << sp << "  Condition:\n";
        if (cond)      cond->print(indent + 4);
        std::cout << sp << "  Then:\n";
        if (thenBlock) thenBlock->print(indent + 4);
        if (elseBlock) {
            std::cout << sp << "  Else:\n";
            elseBlock->print(indent + 4);
        }
    }
};

struct WhileAST : AST {
    std::unique_ptr<AST> cond, body;
    void print(int indent) const override {
        std::cout << std::string(indent, ' ') << "WhileLoop\n";
        if (cond) cond->print(indent + 2);
        if (body) body->print(indent + 2);
    }
};

struct ForAST : AST {
    std::unique_ptr<AST> init, cond, inc, body;
    ForAST(std::unique_ptr<AST> i, std::unique_ptr<AST> c,
           std::unique_ptr<AST> in, std::unique_ptr<BlockAST> b)
        : init(std::move(i)), cond(std::move(c)),
          inc(std::move(in)), body(std::move(b)) {}
    void print(int indent) const override {
        std::cout << std::string(indent, ' ') << "ForLoop\n";
        if (init) init->print(indent + 2);
        if (cond) cond->print(indent + 2);
        if (inc)  inc->print(indent + 2);
        if (body) body->print(indent + 2);
    }
};

struct BreakAST : AST {
    void print(int indent) const override {
        std::cout << std::string(indent, ' ') << "Break\n";
    }
};

struct ContinueAST : AST {
    void print(int indent) const override {
        std::cout << std::string(indent, ' ') << "Continue\n";
    }
};

// ─────────────────────────────────────────────────────────────
//  Array declarations — now carry element type
// ─────────────────────────────────────────────────────────────

struct ArrayDeclAST : AST {
    std::string name;
    int         size;
    ASTType     type;   // int or float

    ArrayDeclAST(const std::string& n, int s, ASTType t = ASTType::Int)
        : name(n), size(s), type(t) {}

    void print(int indent) const override {
        std::cout << std::string(indent, ' ')
                  << "ArrayDecl: " << astTypeName(type)
                  << " " << name << "[" << size << "]\n";
    }
};

struct ArrayAccessAST : AST {
    std::string name;
    std::unique_ptr<AST> index;
    ArrayAccessAST(const std::string& n, std::unique_ptr<AST> idx)
        : name(n), index(std::move(idx)) {}
    void print(int indent) const override {
        std::cout << std::string(indent, ' ') << "ArrayAccess: " << name << "\n";
        index->print(indent + 4);
    }
};

struct ArrayAssignAST : AST {
    std::string name;
    std::unique_ptr<AST> index, expr;
    ArrayAssignAST(const std::string& n,
                   std::unique_ptr<AST> idx,
                   std::unique_ptr<AST> e)
        : name(n), index(std::move(idx)), expr(std::move(e)) {}
    void print(int indent) const override {
        std::cout << std::string(indent, ' ')
                  << "ArrayAssign: " << name << "[...] =\n";
        if (index) index->print(indent + 4);
        if (expr)  expr->print(indent + 4);
    }
};

// ─────────────────────────────────────────────────────────────
//  Functions — now carry return type and parameter types
// ─────────────────────────────────────────────────────────────

struct PrototypeAST : AST {
    std::string              name;
    std::vector<std::string> args;
    std::vector<ASTType>     argTypes;   // parallel to args
    ASTType                  returnType = ASTType::Int;

    void print(int indent) const override {
        std::cout << std::string(indent, ' ')
                  << "FunctionPrototype: "
                  << astTypeName(returnType) << " " << name << "(";
        for (size_t i = 0; i < args.size(); ++i) {
            if (i) std::cout << ", ";
            std::cout << astTypeName(i < argTypes.size() ? argTypes[i] : ASTType::Int)
                      << " " << args[i];
        }
        std::cout << ")\n";
    }
};

struct FunctionAST : AST {
    std::unique_ptr<PrototypeAST> proto;
    std::unique_ptr<BlockAST>     body;
    FunctionAST(std::unique_ptr<PrototypeAST> p, std::unique_ptr<BlockAST> b)
        : proto(std::move(p)), body(std::move(b)) {}
    void print(int indent) const override {
        std::cout << std::string(indent, ' ') << "FunctionDefinition\n";
        if (proto) proto->print(indent + 2);
        if (body)  body->print(indent + 2);
    }
};

struct CallAST : AST {
    std::string callee;
    std::vector<std::unique_ptr<AST>> args;
    void print(int indent) const override {
        std::cout << std::string(indent, ' ')
                  << "FunctionCall: " << callee << "\n";
        for (const auto& a : args) a->print(indent + 4);
    }
};

// ─────────────────────────────────────────────────────────────
//  Program root
// ─────────────────────────────────────────────────────────────

struct ProgramAST : AST {
    std::vector<std::unique_ptr<AST>> topLevel;
    void print(int indent) const override {
        std::cout << "--- [SYNTACTIC VALIDATION: AST TREE] ---\n";
        for (const auto& e : topLevel) e->print(indent);
    }
};

#endif