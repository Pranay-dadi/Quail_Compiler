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
//  String literal
// ─────────────────────────────────────────────────────────────
struct StringAST : AST {
    std::string value;
    explicit StringAST(std::string v) : value(std::move(v)) {}
    void print(int indent) const override {
        std::cout << std::string(indent, ' ') << "String: \"" << value << "\"\n";
    }
};

// string variable declaration  (string s = "hello";)
struct StringVarDeclAST : AST {
    std::string          name;
    std::unique_ptr<AST> init;   // nullptr = uninitialized
    StringVarDeclAST(const std::string& n, std::unique_ptr<AST> e)
        : name(n), init(std::move(e)) {}
    void print(int indent) const override {
        std::cout << std::string(indent, ' ') << "StringVarDecl: string " << name;
        if (init) { std::cout << " =\n"; init->print(indent + 4); }
        else std::cout << "\n";
    }
};

// strlen(s) expression
struct StrLenAST : AST {
    std::string varName;
    explicit StrLenAST(std::string n) : varName(std::move(n)) {}
    void print(int indent) const override {
        std::cout << std::string(indent, ' ') << "StrLen: strlen(" << varName << ")\n";
    }
};

// strcat(dest, src)
struct StrCatAST : AST {
    std::string dest;
    std::string src;
    StrCatAST(std::string d, std::string s) : dest(std::move(d)), src(std::move(s)) {}
    void print(int indent) const override {
        std::cout << std::string(indent, ' ')
                  << "StrCat: " << dest << " += " << src << "\n";
    }
};

// strcmp(a, b) → int
struct StrCmpAST : AST {
    std::unique_ptr<AST> lhs;
    std::unique_ptr<AST> rhs;
    StrCmpAST(std::unique_ptr<AST> l, std::unique_ptr<AST> r)
        : lhs(std::move(l)), rhs(std::move(r)) {}
    void print(int indent) const override {
        std::cout << std::string(indent, ' ') << "StrCmp:\n";
        if (lhs) lhs->print(indent + 4);
        if (rhs) rhs->print(indent + 4);
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
//  Pointer / Reference nodes
// ─────────────────────────────────────────────────────────────

// &varName — address-of
struct AddressOfAST : AST {
    std::string name;
    explicit AddressOfAST(std::string n) : name(std::move(n)) {}
    void print(int indent) const override {
        std::cout << std::string(indent, ' ') << "AddressOf: &" << name << "\n";
    }
};

// *expr — dereference
struct DerefAST : AST {
    std::unique_ptr<AST> operand;
    explicit DerefAST(std::unique_ptr<AST> e) : operand(std::move(e)) {}
    void print(int indent) const override {
        std::cout << std::string(indent, ' ') << "Deref: *\n";
        if (operand) operand->print(indent + 4);
    }
};

// *ptr = expr — dereference assignment
struct DerefAssignAST : AST {
    std::unique_ptr<AST> ptr;
    std::unique_ptr<AST> expr;
    DerefAssignAST(std::unique_ptr<AST> p, std::unique_ptr<AST> e)
        : ptr(std::move(p)), expr(std::move(e)) {}
    void print(int indent) const override {
        std::cout << std::string(indent, ' ') << "DerefAssign: *(...) =\n";
        if (ptr)  ptr->print(indent + 4);
        if (expr) expr->print(indent + 4);
    }
};

// ptr->field — pointer member access
struct ArrowAccessAST : AST {
    std::string objPtrName;
    std::string memberName;
    ArrowAccessAST(const std::string& o, const std::string& m)
        : objPtrName(o), memberName(m) {}
    void print(int indent) const override {
        std::cout << std::string(indent, ' ')
                  << "ArrowAccess: " << objPtrName << "->" << memberName << "\n";
    }
};

// ptr->field = expr — pointer member assignment
struct ArrowAssignAST : AST {
    std::string          objPtrName;
    std::string          memberName;
    std::unique_ptr<AST> expr;
    ArrowAssignAST(const std::string& o, const std::string& m, std::unique_ptr<AST> e)
        : objPtrName(o), memberName(m), expr(std::move(e)) {}
    void print(int indent) const override {
        std::cout << std::string(indent, ' ')
                  << "ArrowAssign: " << objPtrName << "->" << memberName << " =\n";
        if (expr) expr->print(indent + 4);
    }
};

// int* ptr;
struct PtrVarDeclAST : AST {
    std::string name;
    ASTType     baseType;
    explicit PtrVarDeclAST(const std::string& n, ASTType t) : name(n), baseType(t) {}
    void print(int indent) const override {
        std::cout << std::string(indent, ' ')
                  << "PtrVarDecl: " << astTypeName(baseType) << "* " << name << "\n";
    }
};

// int* p = &x;
struct PtrVarDeclInitAST : AST {
    std::string          name;
    ASTType              baseType;
    std::unique_ptr<AST> init;
    PtrVarDeclInitAST(const std::string& n, ASTType t, std::unique_ptr<AST> e)
        : name(n), baseType(t), init(std::move(e)) {}
    void print(int indent) const override {
        std::cout << std::string(indent, ' ')
                  << "PtrVarDeclInit: " << astTypeName(baseType) << "* " << name << " =\n";
        if (init) init->print(indent + 4);
    }
};

// ─────────────────────────────────────────────────────────────
//  Heap allocation nodes
// ─────────────────────────────────────────────────────────────

// new ClassName
struct NewExprAST : AST {
    std::string className;
    explicit NewExprAST(std::string cn) : className(std::move(cn)) {}
    void print(int indent) const override {
        std::cout << std::string(indent, ' ') << "NewExpr: new " << className << "\n";
    }
};

// new int / new float
struct NewScalarAST : AST {
    ASTType type;
    explicit NewScalarAST(ASTType t) : type(t) {}
    void print(int indent) const override {
        std::cout << std::string(indent, ' ')
                  << "NewScalar: new " << astTypeName(type) << "\n";
    }
};

// delete ptr
struct DeleteAST : AST {
    std::string ptrName;
    explicit DeleteAST(std::string n) : ptrName(std::move(n)) {}
    void print(int indent) const override {
        std::cout << std::string(indent, ' ') << "Delete: delete " << ptrName << "\n";
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

// const int x = 5;
struct ConstVarDeclInitAST : AST {
    std::string          name;
    ASTType              type;
    std::unique_ptr<AST> init;
    ConstVarDeclInitAST(const std::string& n, ASTType t, std::unique_ptr<AST> e)
        : name(n), type(t), init(std::move(e)) {}
    void print(int indent) const override {
        std::cout << std::string(indent, ' ')
                  << "ConstVarDeclInit: " << astTypeName(type) << " " << name << " =\n";
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
//  Switch statement
// ─────────────────────────────────────────────────────────────
struct CaseClause {
    std::unique_ptr<AST>              value;    // nullptr = default
    std::vector<std::unique_ptr<AST>> body;
    bool                              hasBreak = false;
};

struct SwitchAST : AST {
    std::unique_ptr<AST>    expr;
    std::vector<CaseClause> cases;
    void print(int indent) const override {
        std::string sp(indent, ' ');
        std::cout << sp << "SwitchStmt\n";
        if (expr) expr->print(indent + 4);
        for (auto& c : cases) {
            if (c.value) std::cout << sp << "  Case:\n";
            else         std::cout << sp << "  Default:\n";
            if (c.value) c.value->print(indent + 6);
            for (auto& s : c.body) s->print(indent + 6);
        }
    }
};

// ─────────────────────────────────────────────────────────────
//  I/O statements
// ─────────────────────────────────────────────────────────────

struct PrintAST : AST {
    std::vector<std::unique_ptr<AST>> exprs;
    bool newline;
    PrintAST(std::vector<std::unique_ptr<AST>> e, bool nl)
        : exprs(std::move(e)), newline(nl) {}
    void print(int indent) const override {
        std::cout << std::string(indent, ' ')
                  << (newline ? "Println" : "Print") << ":\n";
        for (const auto& e : exprs) e->print(indent + 4);
    }
};

struct ScanAST : AST {
    std::vector<std::string> varNames;
    explicit ScanAST(std::vector<std::string> v) : varNames(std::move(v)) {}
    void print(int indent) const override {
        std::cout << std::string(indent, ' ') << "Scan:";
        for (const auto& v : varNames) std::cout << " " << v;
        std::cout << "\n";
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

struct ClassField {
    std::string name;
    ASTType     type;
};

struct ClassDeclAST : AST {
    std::string                               name;
    std::string                               parentName;  // "" = no parent
    std::vector<ClassField>                   fields;
    std::vector<std::unique_ptr<FunctionAST>> methods;

    bool hasParent() const { return !parentName.empty(); }

    void print(int indent) const override {
        std::string sp(indent, ' ');
        std::cout << sp << "ClassDecl: " << name;
        if (hasParent()) std::cout << " extends " << parentName;
        std::cout << "\n";
        for (auto& f : fields)
            std::cout << sp << "  Field: " << astTypeName(f.type) << " " << f.name << "\n";
        for (auto& m : methods)
            m->print(indent + 2);
    }
};

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

struct ThisAccessAST : AST {
    std::string memberName;
    explicit ThisAccessAST(const std::string& m) : memberName(m) {}
    void print(int indent) const override {
        std::cout << std::string(indent, ' ')
                  << "ThisAccess: this." << memberName << "\n";
    }
};

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

// super.method(args) — call parent class method
struct SuperCallAST : AST {
    std::string                       methodName;
    std::vector<std::unique_ptr<AST>> args;
    SuperCallAST(const std::string& m, std::vector<std::unique_ptr<AST>> a)
        : methodName(m), args(std::move(a)) {}
    void print(int indent) const override {
        std::cout << std::string(indent, ' ')
                  << "SuperCall: super." << methodName << "()\n";
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