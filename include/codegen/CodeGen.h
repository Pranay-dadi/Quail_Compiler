#pragma once
#include "parser/AST.h"
#include "semantic/SymbolTable.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <memory>
#include <vector>
#include <string>
#include <utility>
#include <unordered_map>

// ── Optimization levels ───────────────────────────────────────
enum class OptLevel { O0, O1, O2, O3 };

// ── Optimization statistics ───────────────────────────────────
struct OptStats {
    struct FuncStat {
        std::string name;
        size_t instrBefore  = 0;
        size_t instrAfter   = 0;
        size_t blocksBefore = 0;
        size_t blocksAfter  = 0;
    };
    std::vector<FuncStat> functions;
    size_t totalInstrBefore  = 0;
    size_t totalInstrAfter   = 0;
    size_t totalBlocksBefore = 0;
    size_t totalBlocksAfter  = 0;

    int instrReduction() const {
        if (totalInstrBefore == 0) return 0;
        return (int)(100.0 * (totalInstrBefore - totalInstrAfter) / totalInstrBefore);
    }
};

struct CodeGenError {
    std::string message;
};

// ── Class metadata stored during codegen ─────────────────────
struct ClassInfo {
    std::string name;
    std::vector<std::pair<std::string, ValueType>> fields;  // (fieldName, type) in order
    llvm::StructType* llvmType = nullptr;

    int fieldIndex(const std::string& fname) const {
        for (int i = 0; i < (int)fields.size(); i++)
            if (fields[i].first == fname) return i;
        return -1;
    }

    ValueType fieldType(const std::string& fname) const {
        for (auto& [n, t] : fields)
            if (n == fname) return t;
        return ValueType::Unknown;
    }
};

class CodeGen {
public:
    CodeGen();

    llvm::Value* generate(AST* node);
    void         optimize(OptLevel level = OptLevel::O2);
    std::string  getIRString() const;
    llvm::Value* toBool(llvm::Value* val);
    void         dumpToFile(const std::string& filename);
    void         dump();

    bool hasErrors() const { return !errors.empty(); }
    const std::vector<CodeGenError>&   getErrors()   const { return errors; }
    const OptStats&                    getOptStats() const { return optStats; }
    const std::vector<SymbolLogEntry>& getSymbolLog()const { return symbols.getLog(); }

    // Class registry — for debug/report
    const std::unordered_map<std::string, ClassInfo>& getClassInfos() const { return classInfos; }

private:
    llvm::LLVMContext              context;
    llvm::IRBuilder<>              builder;
    std::unique_ptr<llvm::Module>  module;
    SymbolTable                    symbols;
    std::vector<llvm::BasicBlock*> breakStack;
    std::vector<llvm::BasicBlock*> continueStack;
    std::vector<CodeGenError>      errors;
    OptStats                       optStats;

    // ── OOP state ─────────────────────────────────────────────
    std::unordered_map<std::string, llvm::StructType*> classTypes;   // class name → LLVM struct type
    std::unordered_map<std::string, ClassInfo>         classInfos;   // class name → metadata
    std::string   currentClassName;    // non-empty while generating a method
    llvm::Value*  currentThisAlloca;   // alloca of ClassName* inside current method

    // ── Helpers ───────────────────────────────────────────────
    void addError(const std::string& msg);
    void collectStats(OptStats::FuncStat& fs, llvm::Function& fn, bool before);

    llvm::Type*  llvmType(ASTType   t);
    llvm::Type*  llvmType(ValueType t);
    llvm::Value* coerce(llvm::Value* val, llvm::Type* targetTy);
    std::pair<llvm::Value*, llvm::Value*> promoteToCommon(llvm::Value* lhs, llvm::Value* rhs);

    // ── OOP helpers ───────────────────────────────────────────
    // Generate a class method (prepends implicit this* parameter)
    void generateMethod(FunctionAST* f,
                        const std::string& className,
                        llvm::StructType*  structTy);

    // Resolve a field GEP for an object symbol
    llvm::Value* fieldGEP(const Symbol* sym,
                          const std::string& fieldName,
                          const std::string& tag = "");

    // Resolve a field GEP using an explicit struct pointer value
    llvm::Value* fieldGEPFromPtr(llvm::StructType* structTy,
                                  llvm::Value*      objPtr,
                                  int               fieldIdx,
                                  const std::string& tag = "");
};