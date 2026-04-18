#pragma once
// ============================================================
//  IRValue.h  —  Quail Compiler IR Layer
//
//  Represents operands in three-address code instructions.
//  Every IR value is one of:
//    Temporary  : t0, t1, t2, ...  (compiler-generated)
//    Variable   : user-declared named variables
//    IntConst   : integer literal
//    FloatConst : float literal
//    Label      : jump target (basic block entry)
//    Undefined  : placeholder / uninitialized
// ============================================================
#include <string>
#include <variant>
#include <iostream>

enum class IRValueKind {
    Temporary,
    Variable,
    IntConst,
    FloatConst,
    Label,
    Undefined
};

struct IRValue {
    IRValueKind kind = IRValueKind::Undefined;
    std::string name;       // for Temporary, Variable, Label
    int         ival = 0;   // for IntConst
    double      fval = 0.0; // for FloatConst
    bool        isFloat = false; // type tag

    // ── Factory methods ───────────────────────────────────────
    static IRValue makeTemp(const std::string& n, bool fp = false) {
        IRValue v; v.kind = IRValueKind::Temporary;
        v.name = n; v.isFloat = fp; return v;
    }
    static IRValue makeVar(const std::string& n, bool fp = false) {
        IRValue v; v.kind = IRValueKind::Variable;
        v.name = n; v.isFloat = fp; return v;
    }
    static IRValue makeInt(int i) {
        IRValue v; v.kind = IRValueKind::IntConst;
        v.ival = i; return v;
    }
    static IRValue makeFloat(double f) {
        IRValue v; v.kind = IRValueKind::FloatConst;
        v.fval = f; v.isFloat = true; return v;
    }
    static IRValue makeLabel(const std::string& n) {
        IRValue v; v.kind = IRValueKind::Label;
        v.name = n; return v;
    }
    static IRValue makeUndef() { return IRValue{}; }

    // ── Predicates ────────────────────────────────────────────
    bool isConst()     const { return kind == IRValueKind::IntConst
                                   || kind == IRValueKind::FloatConst; }
    bool isTemp()      const { return kind == IRValueKind::Temporary; }
    bool isVar()       const { return kind == IRValueKind::Variable; }
    bool isLabel()     const { return kind == IRValueKind::Label; }
    bool isUndef()     const { return kind == IRValueKind::Undefined; }
    bool isTempOrVar() const { return isTemp() || isVar(); }

    bool isZero() const {
        if (kind == IRValueKind::IntConst)   return ival == 0;
        if (kind == IRValueKind::FloatConst) return fval == 0.0;
        return false;
    }
    bool isOne() const {
        if (kind == IRValueKind::IntConst)   return ival == 1;
        if (kind == IRValueKind::FloatConst) return fval == 1.0;
        return false;
    }

    // ── String representation ─────────────────────────────────
    std::string toString() const {
        switch (kind) {
            case IRValueKind::Temporary:  return name;
            case IRValueKind::Variable:   return name;
            case IRValueKind::IntConst:   return std::to_string(ival);
            case IRValueKind::FloatConst: {
                std::string s = std::to_string(fval);
                // trim trailing zeros
                if (s.find('.') != std::string::npos) {
                    s.erase(s.find_last_not_of('0') + 1);
                    if (s.back() == '.') s += '0';
                }
                return s;
            }
            case IRValueKind::Label:      return name;
            default:                      return "<undef>";
        }
    }

    bool operator==(const IRValue& o) const {
        if (kind != o.kind) return false;
        switch (kind) {
            case IRValueKind::IntConst:   return ival == o.ival;
            case IRValueKind::FloatConst: return fval == o.fval;
            default:                      return name == o.name;
        }
    }
    bool operator!=(const IRValue& o) const { return !(*this == o); }
};
