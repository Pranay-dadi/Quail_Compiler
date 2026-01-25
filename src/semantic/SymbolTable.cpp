#include "semantic/SymbolTable.h"

void SymbolTable::insert(const std::string& name, llvm::Value* value) {
    table[name] = {value};
}

llvm::Value* SymbolTable::lookup(const std::string& name) {
    if (table.count(name)) return table[name].value;
    return nullptr;
}
