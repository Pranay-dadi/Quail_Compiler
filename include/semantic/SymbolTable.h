#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <stdexcept>
#include <llvm/IR/Value.h>
#include <llvm/IR/Type.h>

// ── Symbol kinds ──────────────────────────────────────────────
enum class SymbolKind {
    Variable,
    Array,
    Function,
    Parameter,
    Object       // class instance (stack-allocated struct)
};

// ── Value types ───────────────────────────────────────────────
enum class ValueType {
    Int,
    Float,
    Void,
    Unknown
};

// ── A single symbol entry ─────────────────────────────────────
struct Symbol {
    std::string  name;
    SymbolKind   kind;
    ValueType    type;
    llvm::Value* value;
    int          arraySize      = 0;
    int          definedAtDepth = 0;
    std::string  ownerFunction;
    std::string  objectClass;    // for kind==Object: the class name

    std::vector<ValueType> paramTypes;
    ValueType              returnType = ValueType::Int;
};

// ── Flat log entry ────────────────────────────────────────────
struct SymbolLogEntry {
    std::string  name;
    SymbolKind   kind;
    ValueType    type;
    int          arraySize      = 0;
    int          scopeDepth     = 0;
    std::string  ownerFunction;
    std::string  objectClass;    // for kind==Object

    std::vector<ValueType> paramTypes;
    ValueType              returnType = ValueType::Int;
};

// ── Scoped symbol table ────────────────────────────────────────
class SymbolTable {
public:
    SymbolTable();

    // ── Scope management ──────────────────────────────────────
    void enterScope();
    void exitScope();
    int  currentDepth() const { return (int)scopes.size() - 1; }

    void setCurrentFunction(const std::string& fn) { currentFunction = fn; }
    void clearCurrentFunction()                     { currentFunction.clear(); }
    const std::string& getCurrentFunction() const   { return currentFunction; }

    // ── Insertion ─────────────────────────────────────────────
    void insert(const std::string& name,
                ValueType          type,
                SymbolKind         kind,
                llvm::Value*       value,
                int                arraySize   = 0,
                const std::string& objectClass = "");

    void insertFunction(const std::string&            name,
                        ValueType                     returnType,
                        const std::vector<ValueType>& paramTypes,
                        llvm::Value*                  value = nullptr);

    // ── Lookup ────────────────────────────────────────────────
    const Symbol* lookup(const std::string& name) const;
    Symbol*       lookup(const std::string& name);
    const Symbol* lookupCurrentScope(const std::string& name) const;
    llvm::Value*  lookupValue(const std::string& name) const;
    ValueType     lookupType(const std::string& name) const;

    // ── Utilities ─────────────────────────────────────────────
    bool isDeclared(const std::string& name) const { return lookup(name) != nullptr; }
    bool isDeclaredInCurrentScope(const std::string& name) const {
        return lookupCurrentScope(name) != nullptr;
    }
    void updateValue(const std::string& name, llvm::Value* value);

    static std::string typeName(ValueType t);
    static std::string kindName(SymbolKind k);

    const std::vector<SymbolLogEntry>& getLog() const { return log; }

private:
    using Scope = std::unordered_map<std::string, Symbol>;
    std::vector<Scope>         scopes;
    std::vector<SymbolLogEntry> log;
    std::string                currentFunction;

    void appendLog(const Symbol& sym);
};