#ifndef AST_H
#define AST_H
#include <memory>
#include <string>
#include <vector>
#include <iostream>

struct AST {
    virtual ~AST() = default;
    virtual void print(int indent = 0) = 0;
};

struct NumberAST : AST {
    int val;
    NumberAST(int v) : val(v) {}
    void print(int indent) override {
        std::cout << std::string(indent, ' ') << "Number: " << val << "\n";
    }
};

struct VariableAST : AST {
    std::string name;
    VariableAST(std::string n) : name(n) {}
    void print(int indent) override {
        std::cout << std::string(indent, ' ') << "Variable: " << name << "\n";
    }
};

struct BinaryAST : AST {
    std::string op;
    std::unique_ptr<AST> lhs, rhs;
    void print(int indent) override {
        std::cout << std::string(indent, ' ') << "BinaryOp: " << op << "\n";
        lhs->print(indent + 4);
        rhs->print(indent + 4);
    }
};

struct ReturnAST : AST {
    std::unique_ptr<AST> expr;
    ReturnAST(std::unique_ptr<AST> e) : expr(std::move(e)) {}
    void print(int indent) override {
        std::cout << std::string(indent, ' ') << "ReturnStmt\n";
        if (expr) expr->print(indent + 4);
    }
};

struct VarDeclAST : AST {
    std::string name;
    VarDeclAST(const std::string& n) : name(n) {}
    void print(int indent) override {
        std::cout << std::string(indent, ' ') << "VarDecl: " << name << "\n";
    }
};

struct AssignAST : AST {
    std::string name;
    std::unique_ptr<AST> expr;
    AssignAST(std::string n, std::unique_ptr<AST> v) : name(n), expr(std::move(v)) {}
    void print(int indent) override {
        std::cout << std::string(indent, ' ') << "Assignment: " << name << " =\n";
        if(expr) expr->print(indent + 4);
    }
};

struct BlockAST : AST {
    std::vector<std::unique_ptr<AST>> statements;
    void print(int indent) override {
        std::cout << std::string(indent, ' ') << "Block:\n";
        for (auto& stmt : statements) {
            stmt->print(indent + 2);
        }
    }
};

struct IfAST : AST {
    std::unique_ptr<AST> cond, thenBlock, elseBlock;
    void print(int indent) override {
        std::string space(indent, ' ');
        std::cout << space << "IfStatement\n";
        
        // Label the Condition branch
        std::cout << space << "  Condition:\n";
        if (cond) cond->print(indent + 4);
        
        // Label the Then branch
        std::cout << space << "  Then:\n";
        if (thenBlock) thenBlock->print(indent + 4);
        
        // Label the Else branch (only if it exists)
        if (elseBlock) {
            std::cout << space << "  Else:\n";
            elseBlock->print(indent + 4);
    }
}
};

struct WhileAST : AST {
    std::unique_ptr<AST> cond, body;
    void print(int indent) override {
        std::cout << std::string(indent, ' ') << "WhileLoop\n";
        if (cond) cond->print(indent + 2);
        if (body) body->print(indent + 2);
    }
};


struct PrototypeAST : AST {
    std::string name;
    std::vector<std::string> args;
    void print(int indent) override {
        std::cout << std::string(indent, ' ') << "FunctionPrototype: " << name << "(";
        for (const auto& arg : args) std::cout << arg << " ";
        std::cout << ")\n";
    }
};

struct FunctionAST : AST {
    std::unique_ptr<PrototypeAST> proto;
    std::unique_ptr<BlockAST> body;

    FunctionAST(std::unique_ptr<PrototypeAST> p,
                std::unique_ptr<BlockAST> b)
        : proto(std::move(p)), body(std::move(b)) {}
    
    void print(int indent) override {
        std::cout << std::string(indent, ' ') << "FunctionDefinition\n";
        if (proto) proto->print(indent + 2);
        if (body) body->print(indent + 2);
    }
};

struct CallAST : AST {
    std::string callee;
    std::vector<std::unique_ptr<AST>> args;
    void print(int indent) override {
        std::cout << std::string(indent, ' ') << "FunctionCall: " << callee << "\n";
        for (auto& arg : args) arg->print(indent + 4);
    }
};

struct ArrayDeclAST : AST {
    std::string name;
    int size;

    ArrayDeclAST(const std::string& n, int s)
        : name(n), size(s) {}
    void print(int indent) override {
        std::cout << std::string(indent, ' ') << "ArrayDecl: " << name << "[" << size << "]\n";
    }
};

struct ArrayAccessAST : AST {
    std::string name;
    std::unique_ptr<AST> index;

    ArrayAccessAST(const std::string& n, std::unique_ptr<AST> idx)
        : name(n), index(std::move(idx)) {}
    void print(int indent) override {
        std::cout << std::string(indent, ' ') << "ArrayAccess: " << name << "\n";
        index->print(indent + 4);
    }
};

struct ProgramAST : AST {
    std::vector<std::unique_ptr<FunctionAST>> functions;
    void print(int indent) override {
        std::cout << "--- [SYNTACTIC VALIDATION: AST TREE] ---\n";
        for (auto& e : functions) e->print(indent);
    }
};

struct ForAST : AST {
    std::unique_ptr<AST> init, cond, inc, body;

    ForAST(std::unique_ptr<AST> i, std::unique_ptr<AST> c, 
           std::unique_ptr<AST> in, std::unique_ptr<BlockAST> b)
        : init(std::move(i)), cond(std::move(c)), inc(std::move(in)), body(std::move(b)) {}

    void print(int indent) override {
        std::cout << std::string(indent, ' ') << "ForLoop\n";
        if (init) init->print(indent + 2);
        if (cond) cond->print(indent + 2);
        if (inc) inc->print(indent + 2);
        if (body) body->print(indent + 2);
    }
};

struct BreakAST : AST {
    void print(int indent) override {
        std::cout << std::string(indent, ' ') << "Break\n";
    }
};
struct ContinueAST : AST {
    void print(int indent) override {
        std::cout << std::string(indent, ' ') << "Continue\n";
    }
};

struct LogicalAST : AST {
    std::string op;
    std::unique_ptr<AST> lhs, rhs;
    void print(int indent) override {
        std::cout << std::string(indent, ' ') << "LogicalOp: " << op << "\n";
        if (lhs) lhs->print(indent + 2);
        if (rhs) rhs->print(indent + 2);
    }
};

struct UnaryAST : AST {
    std::string op;
    std::unique_ptr<AST> operand;
    void print(int indent) override {
        std::cout << std::string(indent, ' ') << "UnaryOp: " << op << "\n";
        operand->print(indent + 4);
    }
};

struct ArrayAssignAST : AST {
    std::string name;
    std::unique_ptr<AST> index;
    std::unique_ptr<AST> expr;

    ArrayAssignAST(const std::string& n,
                   std::unique_ptr<AST> idx,
                   std::unique_ptr<AST> e)
        : name(n), index(std::move(idx)), expr(std::move(e)) {}
};

#endif


