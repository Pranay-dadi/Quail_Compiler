#pragma once
#include <string>
#include <unordered_map>
#include <llvm/IR/Value.h>

struct Symbol {
    llvm::Value* value;
};

class SymbolTable {
public:
    void insert(const std::string& name, llvm::Value* value);
    llvm::Value* lookup(const std::string& name);
private:
    std::unordered_map<std::string, Symbol> table;
};
