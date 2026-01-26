#include "semantic/SymbolTable.h"

void SymbolTable::enterScope() {
    scopes.emplace_back();
}

void SymbolTable::exitScope() {
    scopes.pop_back();
}

void SymbolTable::insert(const std::string& name, llvm::Value* value) {
    scopes.back()[name] = {value};
}

llvm::Value* SymbolTable::lookup(const std::string& name) {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        if (it->count(name))
            return (*it)[name].value;
    }
    return nullptr;
}
