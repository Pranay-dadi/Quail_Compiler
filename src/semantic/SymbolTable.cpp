#include "semantic/SymbolTable.h"
#include <stdexcept>

// ── Constructor ────────────────────────────────────────────────
SymbolTable::SymbolTable() {
    enterScope(); // global scope always present
}

// ── Scope management ──────────────────────────────────────────
void SymbolTable::enterScope() {
    scopes.emplace_back();
}

void SymbolTable::exitScope() {
    if (scopes.size() <= 1)
        throw std::runtime_error("[SymbolTable] Cannot exit global scope");
    scopes.pop_back();
}

// ── Insert variable / array ────────────────────────────────────
void SymbolTable::insert(const std::string& name,
                         ValueType          type,
                         SymbolKind         kind,
                         llvm::Value*       value,
                         int                arraySize)
{
    if (scopes.empty())
        throw std::runtime_error("[SymbolTable] No active scope");

    auto& top = scopes.back();
    if (top.count(name))
        throw std::runtime_error(
            "Redeclaration of '" + name + "' in the same scope");

    Symbol sym;
    sym.name           = name;
    sym.kind           = kind;
    sym.type           = type;
    sym.value          = value;
    sym.arraySize      = arraySize;
    sym.definedAtDepth = currentDepth();
    top[name] = sym;
}

// ── Insert function signature ──────────────────────────────────
void SymbolTable::insertFunction(const std::string&         name,
                                 ValueType                  returnType,
                                 const std::vector<ValueType>& paramTypes,
                                 llvm::Value*               value)
{
    if (scopes.empty())
        throw std::runtime_error("[SymbolTable] No active scope");

    // Functions are always inserted into the global (outermost) scope
    auto& global = scopes.front();
    if (global.count(name))
        throw std::runtime_error(
            "Redefinition of function '" + name + "'");

    Symbol sym;
    sym.name        = name;
    sym.kind        = SymbolKind::Function;
    sym.type        = returnType;
    sym.returnType  = returnType;
    sym.paramTypes  = paramTypes;
    sym.value       = value;
    sym.definedAtDepth = 0;
    global[name] = sym;
}

// ── Lookup (outer → inner search, inner shadows outer) ────────
Symbol* SymbolTable::lookup(const std::string& name) {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end())
            return &found->second;
    }
    return nullptr;
}

const Symbol* SymbolTable::lookup(const std::string& name) const {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end())
            return &found->second;
    }
    return nullptr;
}

// ── Current-scope-only lookup ─────────────────────────────────
const Symbol* SymbolTable::lookupCurrentScope(const std::string& name) const {
    if (scopes.empty()) return nullptr;
    auto found = scopes.back().find(name);
    return (found != scopes.back().end()) ? &found->second : nullptr;
}

// ── Convenience: LLVM value ───────────────────────────────────
llvm::Value* SymbolTable::lookupValue(const std::string& name) const {
    const Symbol* s = lookup(name);
    return s ? s->value : nullptr;
}

// ── Convenience: declared type ────────────────────────────────
ValueType SymbolTable::lookupType(const std::string& name) const {
    const Symbol* s = lookup(name);
    return s ? s->type : ValueType::Unknown;
}

// ── Update LLVM value (e.g. after alloca is created) ─────────
void SymbolTable::updateValue(const std::string& name, llvm::Value* value) {
    Symbol* s = lookup(name);
    if (s) s->value = value;
}

// ── Type name helper ──────────────────────────────────────────
std::string SymbolTable::typeName(ValueType t) {
    switch (t) {
        case ValueType::Int:     return "int";
        case ValueType::Float:   return "float";
        case ValueType::Void:    return "void";
        case ValueType::Unknown: return "<unknown>";
    }
    return "<unknown>";
}