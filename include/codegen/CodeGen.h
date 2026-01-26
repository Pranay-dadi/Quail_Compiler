#pragma once

#include "parser/AST.h"
#include "semantic/SymbolTable.h"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

#include <map>
#include <unordered_map>
#include <memory>

class CodeGen {
public:
    CodeGen();

    llvm::Value* generate(AST* node);
    void optimize();
    llvm::Value* toBool(llvm::Value* val);
    void dumpToFile(const std::string &filename);
    void dump();

private:
    llvm::LLVMContext context;
    llvm::IRBuilder<> builder;
    std::unique_ptr<llvm::Module> module;

    SymbolTable symbols;
    std::unordered_map<std::string, llvm::Value*> namedValues;
    std::vector<llvm::BasicBlock*> breakStack;
    std::vector<llvm::BasicBlock*> continueStack;

};
