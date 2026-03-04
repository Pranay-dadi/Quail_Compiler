#include "semantic/SymbolTable.h"
#include <stdexcept>

SymbolTable::SymbolTable() {
    enterScope(); // global scope
}

void SymbolTable::enterScope() { scopes.emplace_back(); }

void SymbolTable::exitScope() {
    if (scopes.size() <= 1)
        throw std::runtime_error("[SymbolTable] Cannot exit global scope");
    scopes.pop_back();
}

void SymbolTable::appendLog(const Symbol& sym) {
    SymbolLogEntry e;
    e.name          = sym.name;
    e.kind          = sym.kind;
    e.type          = sym.type;
    e.arraySize     = sym.arraySize;
    e.scopeDepth    = sym.definedAtDepth;
    e.ownerFunction = currentFunction;
    e.objectClass   = sym.objectClass;
    e.paramTypes    = sym.paramTypes;
    e.returnType    = sym.returnType;
    log.push_back(e);
}

void SymbolTable::insert(const std::string& name,
                         ValueType          type,
                         SymbolKind         kind,
                         llvm::Value*       value,
                         int                arraySize,
                         const std::string& objectClass)
{
    if (scopes.empty())
        throw std::runtime_error("[SymbolTable] No active scope");

    auto& top = scopes.back();
    if (top.count(name))
        throw std::runtime_error("Redeclaration of '" + name + "' in the same scope");

    Symbol sym;
    sym.name           = name;
    sym.kind           = kind;
    sym.type           = type;
    sym.value          = value;
    sym.arraySize      = arraySize;
    sym.definedAtDepth = currentDepth();
    sym.ownerFunction  = currentFunction;
    sym.objectClass    = objectClass;
    top[name] = sym;

    appendLog(sym);
}

void SymbolTable::insertFunction(const std::string&            name,
                                 ValueType                     returnType,
                                 const std::vector<ValueType>& paramTypes,
                                 llvm::Value*                  value)
{
    if (scopes.empty())
        throw std::runtime_error("[SymbolTable] No active scope");

    auto& global = scopes.front();
    if (global.count(name))
        throw std::runtime_error("Redefinition of function '" + name + "'");

    Symbol sym;
    sym.name           = name;
    sym.kind           = SymbolKind::Function;
    sym.type           = returnType;
    sym.returnType     = returnType;
    sym.paramTypes     = paramTypes;
    sym.value          = value;
    sym.definedAtDepth = 0;
    sym.ownerFunction  = "";
    global[name] = sym;

    appendLog(sym);
}

Symbol* SymbolTable::lookup(const std::string& name) {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) return &found->second;
    }
    return nullptr;
}

const Symbol* SymbolTable::lookup(const std::string& name) const {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) return &found->second;
    }
    return nullptr;
}

const Symbol* SymbolTable::lookupCurrentScope(const std::string& name) const {
    if (scopes.empty()) return nullptr;
    auto found = scopes.back().find(name);
    return found != scopes.back().end() ? &found->second : nullptr;
}

llvm::Value* SymbolTable::lookupValue(const std::string& name) const {
    const Symbol* s = lookup(name);
    return s ? s->value : nullptr;
}

ValueType SymbolTable::lookupType(const std::string& name) const {
    const Symbol* s = lookup(name);
    return s ? s->type : ValueType::Unknown;
}

void SymbolTable::updateValue(const std::string& name, llvm::Value* value) {
    Symbol* s = lookup(name);
    if (s) s->value = value;
}

std::string SymbolTable::typeName(ValueType t) {
    switch (t) {
        case ValueType::Int:     return "int";
        case ValueType::Float:   return "float";
        case ValueType::Void:    return "void";
        case ValueType::Unknown: return "<unknown>";
    }
    return "<unknown>";
}

std::string SymbolTable::kindName(SymbolKind k) {
    switch (k) {
        case SymbolKind::Variable:  return "variable";
        case SymbolKind::Array:     return "array";
        case SymbolKind::Function:  return "function";
        case SymbolKind::Parameter: return "parameter";
        case SymbolKind::Object:    return "object";
    }
    return "?";
}