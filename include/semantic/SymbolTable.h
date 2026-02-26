#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <stdexcept>
#include <llvm/IR/Value.h>
#include <llvm/IR/Type.h>

// ── Symbol kinds ──────────────────────────────────────────────
enum class SymbolKind {
    Variable,   // int x; float x;
    Array,      // int x[N]; float x[N];
    Function,   // int foo(int a, int b)
    Parameter   // function parameter
};

// ── Value types the language supports ─────────────────────────
enum class ValueType {
    Int,        // int  (i32)
    Float,      // float (double)
    Void,       // void (for function return)
    Unknown
};

// ── A single symbol entry ─────────────────────────────────────
struct Symbol {
    std::string  name;
    SymbolKind   kind;
    ValueType    type;          // declared type
    llvm::Value* value;         // LLVM alloca or function ptr
    int          arraySize = 0; // >0 only for arrays
    int          definedAtDepth = 0; // scope depth where declared

    // For functions: parameter types (in order)
    std::vector<ValueType> paramTypes;
    ValueType              returnType = ValueType::Int;
};

// ── Scoped symbol table ────────────────────────────────────────
//
//  Design:
//   - enterScope / exitScope bracket every block and function body
//   - insert() rejects redeclarations within the SAME scope
//   - lookup() walks scopes outward (inner shadows outer)
//   - lookupCurrentScope() checks only the innermost scope
//   - All errors throw std::runtime_error so the caller (CodeGen)
//     can catch them and emit a CodeGenError without crashing.
//
class SymbolTable {
public:
    SymbolTable();

    // ── Scope management ──────────────────────────────────────
    void enterScope();
    void exitScope();
    int  currentDepth() const { return (int)scopes.size() - 1; }

    // ── Insertion ─────────────────────────────────────────────
    // Throws std::runtime_error on redeclaration in same scope.
    void insert(const std::string& name,
                ValueType          type,
                SymbolKind         kind,
                llvm::Value*       value,
                int                arraySize = 0);

    // Insert a function signature (no LLVM value needed at declaration point)
    void insertFunction(const std::string&        name,
                        ValueType                 returnType,
                        const std::vector<ValueType>& paramTypes,
                        llvm::Value*              value = nullptr);

    // ── Lookup ────────────────────────────────────────────────
    // Returns nullptr if not found (never throws).
    const Symbol* lookup(const std::string& name) const;
    Symbol*       lookup(const std::string& name);

    // Only searches innermost scope.
    const Symbol* lookupCurrentScope(const std::string& name) const;

    // Convenience: get just the LLVM value (returns nullptr if missing)
    llvm::Value*  lookupValue(const std::string& name) const;

    // Convenience: get declared type (returns Unknown if missing)
    ValueType     lookupType(const std::string& name) const;

    // ── Utilities ─────────────────────────────────────────────
    bool isDeclared(const std::string& name) const { return lookup(name) != nullptr; }
    bool isDeclaredInCurrentScope(const std::string& name) const {
        return lookupCurrentScope(name) != nullptr;
    }

    // Update the LLVM value of an existing symbol (needed after alloca)
    void updateValue(const std::string& name, llvm::Value* value);

    // Human-readable type name
    static std::string typeName(ValueType t);

private:
    using Scope = std::unordered_map<std::string, Symbol>;
    std::vector<Scope> scopes;
};