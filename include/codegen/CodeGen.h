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
    const std::vector<CodeGenError>&   getErrors()      const { return errors; }
    const OptStats&                    getOptStats()    const { return optStats; }

    // ── Symbol table access ───────────────────────────────────
    // Returns every symbol that was ever inserted (survives scope pops).
    const std::vector<SymbolLogEntry>& getSymbolLog()   const { return symbols.getLog(); }

private:
    llvm::LLVMContext              context;
    llvm::IRBuilder<>              builder;
    std::unique_ptr<llvm::Module>  module;
    SymbolTable                    symbols;
    std::vector<llvm::BasicBlock*> breakStack;
    std::vector<llvm::BasicBlock*> continueStack;
    std::vector<CodeGenError>      errors;
    OptStats                       optStats;

    void addError(const std::string& msg);
    void collectStats(OptStats::FuncStat& fs, llvm::Function& fn, bool before);

    llvm::Type*  llvmType(ASTType   t);
    llvm::Type*  llvmType(ValueType t);
    llvm::Value* coerce(llvm::Value* val, llvm::Type* targetTy);
    std::pair<llvm::Value*, llvm::Value*> promoteToCommon(llvm::Value* lhs, llvm::Value* rhs);
};