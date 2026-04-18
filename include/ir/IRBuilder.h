#pragma once
// ============================================================
//  IRBuilder.h  —  Quail Compiler
//
//  Walks the AST and emits three-address code into an IRModule.
//  Simultaneously builds a Value DAG (Directed Acyclic Graph)
//  per basic block to enable local common-subexpression
//  detection during lowering.
//
//  FIX v2: build() now returns a std::unique_ptr<IRModule>
//  (transferred ownership) instead of a raw pointer, so the
//  caller owns the module and there is no double-free.
// ============================================================
#include "ir/IRCFG.h"
#include "parser/AST.h"
#include <unordered_map>
#include <functional>
#include <sstream>

// ─────────────────────────────────────────────────────────────
//  DAG key for local CSE during IR building
// ─────────────────────────────────────────────────────────────
struct DAGKey {
    IROp        op;
    std::string l;
    std::string r;
    bool operator==(const DAGKey& o) const {
        return op == o.op && l == o.l && r == o.r;
    }
};
struct DAGKeyHash {
    size_t operator()(const DAGKey& k) const {
        size_t h = std::hash<int>{}((int)k.op);
        h ^= std::hash<std::string>{}(k.l) << 1;
        h ^= std::hash<std::string>{}(k.r) << 2;
        return h;
    }
};

// ─────────────────────────────────────────────────────────────
//  IRBuilder
// ─────────────────────────────────────────────────────────────
class IRBuilder {
public:
    explicit IRBuilder(const std::string& moduleName = "quail");

    // Entry point: build IR from a parsed AST.
    // Returns ownership of the IRModule via unique_ptr.
    // The IRBuilder must NOT be used after this call.
    std::unique_ptr<IRModule> build(AST* root);

    const std::vector<std::string>& getErrors() const { return errors; }
    bool hasErrors() const { return !errors.empty(); }

private:
    std::unique_ptr<IRModule> module;
    IRFunction*  currentFn  = nullptr;
    BasicBlock*  currentBB  = nullptr;
    int          tempCount  = 0;
    int          labelCount = 0;
    std::vector<std::string> errors;

    // Local variable → current IRValue (temp or var) in this function
    std::unordered_map<std::string, IRValue> varMap;

    // Per-block DAG: canonical (op,l,r) → result temp
    std::unordered_map<DAGKey, IRValue, DAGKeyHash> dag;

    // Break/continue target stacks
    std::vector<std::string> breakTargets;
    std::vector<std::string> continueTargets;

    // ── Name generation ───────────────────────────────────────
    IRValue newTemp(bool fp = false) {
        return IRValue::makeTemp("t" + std::to_string(tempCount++), fp);
    }
    std::string newLabel(const std::string& hint = "L") {
        return hint + std::to_string(labelCount++);
    }

    // ── Emission ──────────────────────────────────────────────
    void emit(const IRInstruction& ins) {
        if (currentBB) currentBB->instrs.push_back(ins);
    }

    BasicBlock* newBlock(const std::string& hint = "bb") {
        std::string lbl = newLabel(hint);
        return currentFn->addBlock(lbl);
    }

    // Switch to a different basic block (clears local DAG)
    void switchTo(BasicBlock* bb) {
        currentBB = bb;
        dag.clear();
    }

    // ── DAG-based local CSE ───────────────────────────────────
    IRValue emitBinopDag(IROp op, const IRValue& l, const IRValue& r,
                          bool fp = false) {
        DAGKey key{op, l.toString(), r.toString()};
        auto it = dag.find(key);
        if (it != dag.end()) return it->second;

        if (isCommutative(op)) {
            DAGKey ck{op, r.toString(), l.toString()};
            auto ci = dag.find(ck);
            if (ci != dag.end()) return ci->second;
        }

        IRValue dst = newTemp(fp);
        emit(IRInstruction::makeBinop(op, dst, l, r));
        dag[key] = dst;
        return dst;
    }

    // ── Code generation ───────────────────────────────────────
    IRValue genExpr(AST* node);
    void    genStmt(AST* node);
    void    genBlock(BlockAST* blk);
    void    genFunction(FunctionAST* fn);
    void    genClass(ClassDeclAST* cls);

    // ── Operator mapping ──────────────────────────────────────
    IROp binopFor(const std::string& op, bool fp) const {
        if (!fp) {
            if (op == "+")  return IROp::ADD;  if (op == "-")  return IROp::SUB;
            if (op == "*")  return IROp::MUL;  if (op == "/")  return IROp::DIV;
            if (op == "%")  return IROp::MOD;
            if (op == "==") return IROp::EQ;   if (op == "!=") return IROp::NEQ;
            if (op == "<")  return IROp::LT;   if (op == ">")  return IROp::GT;
            if (op == "<=") return IROp::LEQ;  if (op == ">=") return IROp::GEQ;
            if (op == "&&") return IROp::LAND; if (op == "||") return IROp::LOR;
        } else {
            if (op == "+")  return IROp::FADD; if (op == "-")  return IROp::FSUB;
            if (op == "*")  return IROp::FMUL; if (op == "/")  return IROp::FDIV;
            if (op == "==") return IROp::FEQ;  if (op == "!=") return IROp::FNEQ;
            if (op == "<")  return IROp::FLT;  if (op == ">")  return IROp::FGT;
            if (op == "<=") return IROp::FLEQ; if (op == ">=") return IROp::FGEQ;
        }
        return IROp::ADD;
    }

    void addError(const std::string& m) { errors.push_back(m); }
};