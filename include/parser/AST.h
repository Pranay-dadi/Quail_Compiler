#ifndef AST_H
#define AST_H
#include <memory>
#include <string>
#include <vector>
#include <iostream>

// ── Language value types ──────────────────────────────────────
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

struct VarDeclAST : AST {
    std::string name;
    ASTType     type;
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

// Combined declare-and-initialize in the CURRENT scope (no child scope).
// Replaces the old pattern of wrapping VarDecl+Assign in a BlockAST,
// which erroneously destroyed the variable at the end of the inner block.
// Example:  int result = obj.method();
struct VarDeclInitAST : AST {
    std::string          name;
    ASTType              type;
    std::unique_ptr<AST> init;
    VarDeclInitAST(const std::string& n, ASTType t, std::unique_ptr<AST> e)
        : name(n), type(t), init(std::move(e)) {}
    void print(int indent) const override {
        std::cout << std::string(indent, ' ')
                  << "VarDeclInit: " << astTypeName(type) << " " << name << " =\n";
        if (init) init->print(indent + 4);
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
//  Arrays
// ─────────────────────────────────────────────────────────────

struct ArrayDeclAST : AST {
    std::string name;
    int         size;
    ASTType     type;
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
//  Functions
// ─────────────────────────────────────────────────────────────

struct PrototypeAST : AST {
    std::string              name;
    std::vector<std::string> args;
    std::vector<ASTType>     argTypes;
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
//  OOP — Class declaration
// ─────────────────────────────────────────────────────────────

// A single field inside a class body
struct ClassField {
    std::string name;
    ASTType     type;
};

// class Foo { int x; float y; int getX() { ... } }
struct ClassDeclAST : AST {
    std::string                               name;
    std::vector<ClassField>                   fields;
    std::vector<std::unique_ptr<FunctionAST>> methods;

    void print(int indent) const override {
        std::string sp(indent, ' ');
        std::cout << sp << "ClassDecl: " << name << "\n";
        for (auto& f : fields)
            std::cout << sp << "  Field: " << astTypeName(f.type) << " " << f.name << "\n";
        for (auto& m : methods)
            m->print(indent + 2);
    }
};

// ─────────────────────────────────────────────────────────────
//  OOP — Object declaration  (ClassName varName;)
// ─────────────────────────────────────────────────────────────
struct ObjectDeclAST : AST {
    std::string className;
    std::string varName;
    ObjectDeclAST(const std::string& cn, const std::string& vn)
        : className(cn), varName(vn) {}
    void print(int indent) const override {
        std::cout << std::string(indent, ' ')
                  << "ObjectDecl: " << className << " " << varName << "\n";
    }
};

// ─────────────────────────────────────────────────────────────
//  OOP — Member access  (obj.field  in expression)
// ─────────────────────────────────────────────────────────────
struct MemberAccessAST : AST {
    std::string objName;
    std::string memberName;
    MemberAccessAST(const std::string& obj, const std::string& mem)
        : objName(obj), memberName(mem) {}
    void print(int indent) const override {
        std::cout << std::string(indent, ' ')
                  << "MemberAccess: " << objName << "." << memberName << "\n";
    }
};

// ─────────────────────────────────────────────────────────────
//  OOP — Member assign  (obj.field = expr;)
// ─────────────────────────────────────────────────────────────
struct MemberAssignAST : AST {
    std::string          objName;
    std::string          memberName;
    std::unique_ptr<AST> expr;
    MemberAssignAST(const std::string& obj, const std::string& mem,
                    std::unique_ptr<AST> e)
        : objName(obj), memberName(mem), expr(std::move(e)) {}
    void print(int indent) const override {
        std::cout << std::string(indent, ' ')
                  << "MemberAssign: " << objName << "." << memberName << " =\n";
        if (expr) expr->print(indent + 4);
    }
};

// ─────────────────────────────────────────────────────────────
//  OOP — Method call  (obj.method(args)  or  this.method(args))
//         objName == "this" → call on implicit this pointer
// ─────────────────────────────────────────────────────────────
struct MethodCallAST : AST {
    std::string                       objName;
    std::string                       methodName;
    std::vector<std::unique_ptr<AST>> args;
    MethodCallAST(const std::string& obj, const std::string& meth,
                  std::vector<std::unique_ptr<AST>> a)
        : objName(obj), methodName(meth), args(std::move(a)) {}
    void print(int indent) const override {
        std::cout << std::string(indent, ' ')
                  << "MethodCall: " << objName << "." << methodName << "()\n";
        for (const auto& a : args) a->print(indent + 4);
    }
};

// ─────────────────────────────────────────────────────────────
//  OOP — this.field access (inside a method)
// ─────────────────────────────────────────────────────────────
struct ThisAccessAST : AST {
    std::string memberName;
    explicit ThisAccessAST(const std::string& m) : memberName(m) {}
    void print(int indent) const override {
        std::cout << std::string(indent, ' ')
                  << "ThisAccess: this." << memberName << "\n";
    }
};

// ─────────────────────────────────────────────────────────────
//  OOP — this.field = expr (inside a method)
// ─────────────────────────────────────────────────────────────
struct ThisAssignAST : AST {
    std::string          memberName;
    std::unique_ptr<AST> expr;
    ThisAssignAST(const std::string& m, std::unique_ptr<AST> e)
        : memberName(m), expr(std::move(e)) {}
    void print(int indent) const override {
        std::cout << std::string(indent, ' ')
                  << "ThisAssign: this." << memberName << " =\n";
        if (expr) expr->print(indent + 4);
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
