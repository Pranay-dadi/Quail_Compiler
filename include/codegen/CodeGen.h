#pragma once
#include "parser/AST.h"
#include "semantic/SymbolTable.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <map>
#include <unordered_map>
#include <memory>
#include <vector>
#include <string>

// ── Optimization levels ───────────────────────────────────────────────────────
enum class OptLevel {
    O0,   // No optimization
    O1,   // Basic optimizations (mem2reg, instcombine, simplifyCFG)
    O2,   // Standard optimizations (default LLVM O2 pipeline)
    O3    // Aggressive optimizations (O3 + vectorization)
};

// ── Optimization statistics ───────────────────────────────────────────────────
struct OptStats {
    struct FuncStat {
        std::string name;
        size_t instrBefore = 0;
        size_t instrAfter  = 0;
        size_t blocksBefore = 0;
        size_t blocksAfter  = 0;
    };
    std::vector<FuncStat> functions;
    size_t totalInstrBefore = 0;
    size_t totalInstrAfter  = 0;
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

    // Run the optimizer at the requested level; populates optStats
    void         optimize(OptLevel level = OptLevel::O2);

    // Capture raw IR text (before or after opt) for display/diff
    std::string  getIRString() const;

    llvm::Value* toBool(llvm::Value* val);
    void         dumpToFile(const std::string& filename);
    void         dump();

    bool hasErrors() const { return !errors.empty(); }
    const std::vector<CodeGenError>& getErrors() const { return errors; }
    const OptStats& getOptStats() const { return optStats; }

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
};