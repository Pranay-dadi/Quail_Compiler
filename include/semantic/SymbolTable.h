#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <llvm/IR/Value.h>

struct Symbol {
    llvm::Value* value;
};

class SymbolTable {
public:
    SymbolTable() { enterScope(); }

    void enterScope();
    void exitScope();

    void insert(const std::string& name, llvm::Value* value);
    llvm::Value* lookup(const std::string& name);

private:
    std::vector<std::unordered_map<std::string, Symbol>> scopes;
};
