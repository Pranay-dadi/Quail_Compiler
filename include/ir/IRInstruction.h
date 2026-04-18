#pragma once
// ============================================================
//  IRInstruction.h  —  Quail Compiler IR Layer
//
//  Three-address code instructions in quadruple form:
//    (op, arg1, arg2, result)
//
//  Every instruction has at most two source operands and
//  one destination.  Control flow uses explicit label
//  operands.  This is the canonical form used by all
//  optimization passes.
//
//  Instruction categories:
//    ASSIGN      : result = arg1
//    BINOP       : result = arg1 op arg2
//    UNOP        : result = op arg1
//    COPY        : result = arg1              (explicit copy)
//    LOAD        : result = *arg1             (pointer load)
//    STORE       : *arg1 = arg2              (pointer store)
//    ARRAY_LOAD  : result = arg1[arg2]
//    ARRAY_STORE : arg1[arg2] = result (src in result field)
//    LABEL       : arg1 is the label name
//    JUMP        : unconditional goto arg1
//    CJUMP       : if arg1 relop arg2 goto result-label
//    CALL        : result = call arg1(args in extra list)
//    RETURN      : return arg1
//    PARAM       : push arg1 as call parameter
//    ALLOC       : result = alloc(arg1 bytes)
//    FREE        : free(arg1)
//    PHI         : SSA phi node
//    NOP         : no-op (dead instruction placeholder)
// ============================================================
#include "IRValue.h"
#include <string>
#include <vector>

enum class IROp {
    // ── Data movement ─────────────────────────────────────────
    ASSIGN,         // result = arg1
    COPY,           // result = arg1  (copy-propagation candidate)

    // ── Arithmetic ────────────────────────────────────────────
    ADD, SUB, MUL, DIV, MOD,
    FADD, FSUB, FMUL, FDIV,
    NEG, FNEG,

    // ── Bitwise / shift (future use) ──────────────────────────
    AND, OR, XOR, NOT, SHL, SHR,

    // ── Comparison (result is bool-as-int) ────────────────────
    EQ, NEQ, LT, GT, LEQ, GEQ,
    FEQ, FNEQ, FLT, FGT, FLEQ, FGEQ,

    // ── Logical ───────────────────────────────────────────────
    LAND, LOR, LNOT,

    // ── Type conversions ─────────────────────────────────────
    INT_TO_FLOAT,
    FLOAT_TO_INT,

    // ── Memory ───────────────────────────────────────────────
    LOAD,           // result = *arg1
    STORE,          // *arg1 = arg2
    ADDR_OF,        // result = &arg1
    ARRAY_LOAD,     // result = arg1[arg2]
    ARRAY_STORE,    // arg1[arg2] = src (src in 'result' field)

    // ── Control flow ─────────────────────────────────────────
    LABEL,          // label declaration
    JUMP,           // goto label
    CJUMP,          // if arg1 relop arg2 goto label (label in result)
    CJUMP_TRUE,     // if arg1 goto label
    CJUMP_FALSE,    // if !arg1 goto label

    // ── Functions ────────────────────────────────────────────
    PARAM,          // push param
    CALL,           // result = call func(params)
    CALL_VOID,      // call func(params)  — no return value
    RETURN,         // return arg1
    RETURN_VOID,    // return

    // ── Heap ─────────────────────────────────────────────────
    ALLOC,          // result = malloc(arg1)
    FREE,           // free(arg1)

    // ── SSA ──────────────────────────────────────────────────
    PHI,            // result = phi(arg1_from_bb1, arg2_from_bb2, ...)

    // ── Misc ─────────────────────────────────────────────────
    NOP,            // no-op (dead instruction marker)
    COMMENT         // human-readable annotation (not emitted)
};

// ── Human-readable opcode name ────────────────────────────────
inline std::string opName(IROp op) {
    switch (op) {
        case IROp::ASSIGN:        return "=";
        case IROp::COPY:          return "copy";
        case IROp::ADD:           return "+";
        case IROp::SUB:           return "-";
        case IROp::MUL:           return "*";
        case IROp::DIV:           return "/";
        case IROp::MOD:           return "%";
        case IROp::FADD:          return "f+";
        case IROp::FSUB:          return "f-";
        case IROp::FMUL:          return "f*";
        case IROp::FDIV:          return "f/";
        case IROp::NEG:           return "neg";
        case IROp::FNEG:          return "fneg";
        case IROp::AND:           return "&";
        case IROp::OR:            return "|";
        case IROp::XOR:           return "^";
        case IROp::NOT:           return "~";
        case IROp::SHL:           return "<<";
        case IROp::SHR:           return ">>";
        case IROp::EQ:            return "==";
        case IROp::NEQ:           return "!=";
        case IROp::LT:            return "<";
        case IROp::GT:            return ">";
        case IROp::LEQ:           return "<=";
        case IROp::GEQ:           return ">=";
        case IROp::FEQ:           return "f==";
        case IROp::FNEQ:          return "f!=";
        case IROp::FLT:           return "f<";
        case IROp::FGT:           return "f>";
        case IROp::FLEQ:          return "f<=";
        case IROp::FGEQ:          return "f>=";
        case IROp::LAND:          return "&&";
        case IROp::LOR:           return "||";
        case IROp::LNOT:          return "!";
        case IROp::INT_TO_FLOAT:  return "(float)";
        case IROp::FLOAT_TO_INT:  return "(int)";
        case IROp::LOAD:          return "load";
        case IROp::STORE:         return "store";
        case IROp::ADDR_OF:       return "addr";
        case IROp::ARRAY_LOAD:    return "aload";
        case IROp::ARRAY_STORE:   return "astore";
        case IROp::LABEL:         return "label";
        case IROp::JUMP:          return "goto";
        case IROp::CJUMP:         return "if";
        case IROp::CJUMP_TRUE:    return "iftrue";
        case IROp::CJUMP_FALSE:   return "iffalse";
        case IROp::PARAM:         return "param";
        case IROp::CALL:          return "call";
        case IROp::CALL_VOID:     return "call_void";
        case IROp::RETURN:        return "return";
        case IROp::RETURN_VOID:   return "return_void";
        case IROp::ALLOC:         return "alloc";
        case IROp::FREE:          return "free";
        case IROp::PHI:           return "phi";
        case IROp::NOP:           return "nop";
        case IROp::COMMENT:       return "//";
        default:                  return "?";
    }
}

// ── Is this a binary arithmetic or comparison op? ─────────────
inline bool isBinaryOp(IROp op) {
    switch (op) {
        case IROp::ADD: case IROp::SUB: case IROp::MUL:
        case IROp::DIV: case IROp::MOD:
        case IROp::FADD: case IROp::FSUB: case IROp::FMUL: case IROp::FDIV:
        case IROp::AND: case IROp::OR: case IROp::XOR:
        case IROp::SHL: case IROp::SHR:
        case IROp::EQ: case IROp::NEQ: case IROp::LT:
        case IROp::GT: case IROp::LEQ: case IROp::GEQ:
        case IROp::FEQ: case IROp::FNEQ: case IROp::FLT:
        case IROp::FGT: case IROp::FLEQ: case IROp::FGEQ:
        case IROp::LAND: case IROp::LOR:
            return true;
        default: return false;
    }
}

inline bool isUnaryOp(IROp op) {
    return op == IROp::NEG  || op == IROp::FNEG ||
           op == IROp::NOT  || op == IROp::LNOT ||
           op == IROp::INT_TO_FLOAT || op == IROp::FLOAT_TO_INT;
}

inline bool isCommutative(IROp op) {
    return op == IROp::ADD  || op == IROp::FADD ||
           op == IROp::MUL  || op == IROp::FMUL ||
           op == IROp::AND  || op == IROp::OR   ||
           op == IROp::XOR  || op == IROp::EQ   ||
           op == IROp::NEQ  || op == IROp::FEQ  || op == IROp::FNEQ;
}

inline bool isComparisonOp(IROp op) {
    switch (op) {
        case IROp::EQ: case IROp::NEQ: case IROp::LT:
        case IROp::GT: case IROp::LEQ: case IROp::GEQ:
        case IROp::FEQ: case IROp::FNEQ: case IROp::FLT:
        case IROp::FGT: case IROp::FLEQ: case IROp::FGEQ:
            return true;
        default: return false;
    }
}

inline bool isFloatOp(IROp op) {
    return op == IROp::FADD || op == IROp::FSUB ||
           op == IROp::FMUL || op == IROp::FDIV ||
           op == IROp::FNEG ||
           op == IROp::FEQ  || op == IROp::FNEQ ||
           op == IROp::FLT  || op == IROp::FGT  ||
           op == IROp::FLEQ || op == IROp::FGEQ;
}

// ─────────────────────────────────────────────────────────────
//  IRInstruction  —  single quadruple  (op, arg1, arg2, result)
// ─────────────────────────────────────────────────────────────
struct IRInstruction {
    IROp      op     = IROp::NOP;
    IRValue   arg1;          // left operand / source
    IRValue   arg2;          // right operand (binary ops)
    IRValue   result;        // destination / label target
    std::string comment;     // optional human-readable note
    int       lineHint = 0;  // source line (for diagnostics)

    // For CALL instructions: list of argument values
    std::vector<IRValue> callArgs;

    // For PHI nodes: (predecessor-block-label, value) pairs
    std::vector<std::pair<std::string, IRValue>> phiSources;

    // Dead-code marker — set by DCE pass, removed by peephole
    bool isDead = false;

    // ── Factory helpers ───────────────────────────────────────
    static IRInstruction makeAssign(const IRValue& dst, const IRValue& src) {
        IRInstruction i; i.op = IROp::ASSIGN;
        i.result = dst; i.arg1 = src; return i;
    }
    static IRInstruction makeBinop(IROp op,
                                    const IRValue& dst,
                                    const IRValue& l,
                                    const IRValue& r) {
        IRInstruction i; i.op = op;
        i.result = dst; i.arg1 = l; i.arg2 = r; return i;
    }
    static IRInstruction makeUnop(IROp op,
                                   const IRValue& dst,
                                   const IRValue& src) {
        IRInstruction i; i.op = op;
        i.result = dst; i.arg1 = src; return i;
    }
    static IRInstruction makeLabel(const std::string& name) {
        IRInstruction i; i.op = IROp::LABEL;
        i.arg1 = IRValue::makeLabel(name); return i;
    }
    static IRInstruction makeJump(const std::string& target) {
        IRInstruction i; i.op = IROp::JUMP;
        i.result = IRValue::makeLabel(target); return i;
    }
    static IRInstruction makeCJump(const IRValue& cond,
                                    const std::string& trueLabel,
                                    const std::string& falseLabel) {
        IRInstruction i; i.op = IROp::CJUMP;
        i.arg1 = cond;
        i.result = IRValue::makeLabel(trueLabel);
        i.arg2   = IRValue::makeLabel(falseLabel);
        return i;
    }
    static IRInstruction makeReturn(const IRValue& val) {
        IRInstruction i; i.op = IROp::RETURN;
        i.arg1 = val; return i;
    }
    static IRInstruction makeReturnVoid() {
        IRInstruction i; i.op = IROp::RETURN_VOID; return i;
    }
    static IRInstruction makeCall(const IRValue& dst,
                                   const std::string& fn,
                                   const std::vector<IRValue>& args) {
        IRInstruction i; i.op = IROp::CALL;
        i.result = dst; i.arg1 = IRValue::makeLabel(fn);
        i.callArgs = args; return i;
    }
    static IRInstruction makeCallVoid(const std::string& fn,
                                       const std::vector<IRValue>& args) {
        IRInstruction i; i.op = IROp::CALL_VOID;
        i.arg1 = IRValue::makeLabel(fn);
        i.callArgs = args; return i;
    }
    static IRInstruction makeNop() {
        IRInstruction i; i.op = IROp::NOP; return i;
    }
    static IRInstruction makePhi(const IRValue& dst,
                                  const std::vector<std::pair<std::string,IRValue>>& srcs) {
        IRInstruction i; i.op = IROp::PHI;
        i.result = dst; i.phiSources = srcs; return i;
    }

    // ── Pretty print ──────────────────────────────────────────
    std::string toString() const {
        std::string s;
        switch (op) {
            case IROp::NOP:
                return "    nop";
            case IROp::COMMENT:
                return "    // " + comment;
            case IROp::LABEL:
                return arg1.toString() + ":";
            case IROp::JUMP:
                return "    goto " + result.toString();
            case IROp::CJUMP:
                return "    if " + arg1.toString()
                     + " goto " + result.toString()
                     + " else " + arg2.toString();
            case IROp::CJUMP_TRUE:
                return "    if " + arg1.toString()
                     + " goto " + result.toString();
            case IROp::CJUMP_FALSE:
                return "    ifnot " + arg1.toString()
                     + " goto " + result.toString();
            case IROp::RETURN:
                return "    return " + arg1.toString();
            case IROp::RETURN_VOID:
                return "    return";
            case IROp::PARAM:
                return "    param " + arg1.toString();
            case IROp::CALL: case IROp::CALL_VOID: {
                std::string a;
                for (size_t i = 0; i < callArgs.size(); i++) {
                    if (i) a += ", ";
                    a += callArgs[i].toString();
                }
                std::string lhs = (op == IROp::CALL)
                    ? result.toString() + " = " : "";
                return "    " + lhs + "call " + arg1.toString() + "(" + a + ")";
            }
            case IROp::ALLOC:
                return "    " + result.toString()
                     + " = alloc(" + arg1.toString() + ")";
            case IROp::FREE:
                return "    free(" + arg1.toString() + ")";
            case IROp::LOAD:
                return "    " + result.toString()
                     + " = *" + arg1.toString();
            case IROp::STORE:
                return "    *" + arg1.toString()
                     + " = " + arg2.toString();
            case IROp::ADDR_OF:
                return "    " + result.toString()
                     + " = &" + arg1.toString();
            case IROp::ARRAY_LOAD:
                return "    " + result.toString()
                     + " = " + arg1.toString()
                     + "[" + arg2.toString() + "]";
            case IROp::ARRAY_STORE:
                return "    " + arg1.toString()
                     + "[" + arg2.toString() + "] = "
                     + result.toString();
            case IROp::PHI: {
                std::string a;
                for (size_t i = 0; i < phiSources.size(); i++) {
                    if (i) a += ", ";
                    a += phiSources[i].second.toString()
                       + "<" + phiSources[i].first + ">";
                }
                return "    " + result.toString()
                     + " = phi(" + a + ")";
            }
            case IROp::ASSIGN: case IROp::COPY:
                return "    " + result.toString()
                     + " = " + arg1.toString();
            default:
                if (isBinaryOp(op))
                    return "    " + result.toString()
                         + " = " + arg1.toString()
                         + " " + opName(op) + " "
                         + arg2.toString();
                if (isUnaryOp(op))
                    return "    " + result.toString()
                         + " = " + opName(op)
                         + " " + arg1.toString();
                return "    ??? " + opName(op);
        }
    }

    // Does this instruction have a result (defines a value)?
    bool definesValue() const {
        return op != IROp::LABEL  && op != IROp::JUMP   &&
               op != IROp::CJUMP && op != IROp::CJUMP_TRUE &&
               op != IROp::CJUMP_FALSE &&
               op != IROp::RETURN && op != IROp::RETURN_VOID &&
               op != IROp::PARAM  && op != IROp::CALL_VOID  &&
               op != IROp::STORE  && op != IROp::ARRAY_STORE &&
               op != IROp::FREE   && op != IROp::NOP &&
               op != IROp::COMMENT;
    }

    bool isTerminator() const {
        return op == IROp::JUMP  || op == IROp::CJUMP ||
               op == IROp::CJUMP_TRUE || op == IROp::CJUMP_FALSE ||
               op == IROp::RETURN || op == IROp::RETURN_VOID;
    }

    bool hasSideEffects() const {
        return op == IROp::CALL || op == IROp::CALL_VOID ||
               op == IROp::STORE || op == IROp::ARRAY_STORE ||
               op == IROp::FREE;
    }
};
