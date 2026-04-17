#pragma once
// ============================================================
//  TypeChecker.h  —  Quail Compiler
//
//  Pre-codegen AST walk that catches type errors with proper
//  diagnostics before LLVM ever sees the IR.
//
//  Checks:
//    • Binary operator type compatibility
//    • Void function result used as value
//    • Return type vs declared function return type
//    • Condition expressions must be numeric / bool
//    • Function call argument count matching
//    • Array index must be integer
//    • const must have initializer (parser enforces, checker confirms)
//    • Unary '!' on float
// ============================================================
#include "parser/AST.h"
#include "semantic/SymbolTable.h"
#include <vector>
#include <string>
#include <unordered_map>

struct TypeError {
    int         line;
    std::string message;
};

class TypeChecker {
public:
    TypeChecker();

    void check(AST* root);

    bool hasErrors() const { return !errors.empty(); }
    const std::vector<TypeError>& getErrors() const { return errors; }

private:
    std::vector<TypeError> errors;

    ASTType     currentReturnType   = ASTType::Void;
    std::string currentFunctionName;

    // Lightweight function registry populated during collectSignatures
    std::unordered_map<std::string, ASTType>              funcReturnTypes;
    std::unordered_map<std::string, std::vector<ASTType>> funcParamTypes;

    void addError(int line, const std::string& msg);

    // Returns the ASTType produced by an expression.
    ASTType typeOf(AST* node);

    void checkStmt(AST* node);
    void checkBlock(BlockAST* block);
    void checkFunction(FunctionAST* fn);
    void checkClass(ClassDeclAST* cls);
    void collectSignatures(AST* root);

    static bool isNumeric(ASTType t) {
        return t == ASTType::Int || t == ASTType::Float;
    }
    static bool compatible(ASTType lhs, ASTType rhs) {
        if (lhs == rhs) return true;
        if (isNumeric(lhs) && isNumeric(rhs)) return true;
        return false;
    }
    static ASTType wider(ASTType a, ASTType b) {
        if (a == ASTType::Float || b == ASTType::Float) return ASTType::Float;
        return ASTType::Int;
    }
};