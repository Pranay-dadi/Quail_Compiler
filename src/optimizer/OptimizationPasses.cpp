#include "optimizer/OptimizationPasses.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>

// ═════════════════════════════════════════════════════════════
//  1. CONSTANT FOLDING
// ═════════════════════════════════════════════════════════════

IRValue ConstantFoldingPass::foldBinary(IROp op,
                                         const IRValue& l,
                                         const IRValue& r) const {
    // Integer operations
    if (l.kind == IRValueKind::IntConst && r.kind == IRValueKind::IntConst) {
        int a = l.ival, b = r.ival;
        switch (op) {
            case IROp::ADD:  return IRValue::makeInt(a + b);
            case IROp::SUB:  return IRValue::makeInt(a - b);
            case IROp::MUL:  return IRValue::makeInt(a * b);
            case IROp::DIV:  return (b != 0) ? IRValue::makeInt(a / b) : IRValue::makeUndef();
            case IROp::MOD:  return (b != 0) ? IRValue::makeInt(a % b) : IRValue::makeUndef();
            case IROp::AND:  return IRValue::makeInt(a & b);
            case IROp::OR:   return IRValue::makeInt(a | b);
            case IROp::XOR:  return IRValue::makeInt(a ^ b);
            case IROp::SHL:  return IRValue::makeInt(a << b);
            case IROp::SHR:  return IRValue::makeInt(a >> b);
            case IROp::EQ:   return IRValue::makeInt(a == b ? 1 : 0);
            case IROp::NEQ:  return IRValue::makeInt(a != b ? 1 : 0);
            case IROp::LT:   return IRValue::makeInt(a <  b ? 1 : 0);
            case IROp::GT:   return IRValue::makeInt(a >  b ? 1 : 0);
            case IROp::LEQ:  return IRValue::makeInt(a <= b ? 1 : 0);
            case IROp::GEQ:  return IRValue::makeInt(a >= b ? 1 : 0);
            case IROp::LAND: return IRValue::makeInt((a && b) ? 1 : 0);
            case IROp::LOR:  return IRValue::makeInt((a || b) ? 1 : 0);
            default: break;
        }
    }
    // Float operations
    double fa = 0, fb = 0;
    bool hasFloat = false;
    if (l.kind == IRValueKind::FloatConst) { fa = l.fval; hasFloat = true; }
    else if (l.kind == IRValueKind::IntConst) { fa = l.ival; }
    else return IRValue::makeUndef();
    if (r.kind == IRValueKind::FloatConst) { fb = r.fval; hasFloat = true; }
    else if (r.kind == IRValueKind::IntConst) { fb = r.ival; }
    else return IRValue::makeUndef();

    if (!hasFloat && !isFloatOp(op)) return IRValue::makeUndef();

    switch (op) {
        case IROp::FADD: return IRValue::makeFloat(fa + fb);
        case IROp::FSUB: return IRValue::makeFloat(fa - fb);
        case IROp::FMUL: return IRValue::makeFloat(fa * fb);
        case IROp::FDIV: return (fb != 0.0) ? IRValue::makeFloat(fa / fb) : IRValue::makeUndef();
        case IROp::FEQ:  return IRValue::makeInt(fa == fb ? 1 : 0);
        case IROp::FNEQ: return IRValue::makeInt(fa != fb ? 1 : 0);
        case IROp::FLT:  return IRValue::makeInt(fa <  fb ? 1 : 0);
        case IROp::FGT:  return IRValue::makeInt(fa >  fb ? 1 : 0);
        case IROp::FLEQ: return IRValue::makeInt(fa <= fb ? 1 : 0);
        case IROp::FGEQ: return IRValue::makeInt(fa >= fb ? 1 : 0);
        default: break;
    }
    return IRValue::makeUndef();
}

IRValue ConstantFoldingPass::foldUnary(IROp op, const IRValue& v) const {
    if (v.kind == IRValueKind::IntConst) {
        switch (op) {
            case IROp::NEG:  return IRValue::makeInt(-v.ival);
            case IROp::NOT:  return IRValue::makeInt(~v.ival);
            case IROp::LNOT: return IRValue::makeInt(!v.ival ? 1 : 0);
            case IROp::INT_TO_FLOAT: return IRValue::makeFloat((double)v.ival);
            default: break;
        }
    }
    if (v.kind == IRValueKind::FloatConst) {
        switch (op) {
            case IROp::FNEG: return IRValue::makeFloat(-v.fval);
            case IROp::FLOAT_TO_INT: return IRValue::makeInt((int)v.fval);
            default: break;
        }
    }
    return IRValue::makeUndef();
}

IRValue ConstantFoldingPass::tryFold(const IRInstruction& ins) const {
    if (isBinaryOp(ins.op) && ins.arg1.isConst() && ins.arg2.isConst())
        return foldBinary(ins.op, ins.arg1, ins.arg2);
    if (isUnaryOp(ins.op) && ins.arg1.isConst())
        return foldUnary(ins.op, ins.arg1);
    if ((ins.op == IROp::ASSIGN || ins.op == IROp::COPY) && ins.arg1.isConst())
        return ins.arg1;
    return IRValue::makeUndef();
}

void ConstantFoldingPass::run(IRFunction& fn) {
    stats = {};
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto& bb : fn.blocks) {
            for (auto& ins : bb->instrs) {
                if (ins.isDead || !ins.definesValue()) continue;
                
                if ((ins.op == IROp::ASSIGN || ins.op == IROp::COPY)
                        && ins.arg1.isConst())
                    continue;
                IRValue folded = tryFold(ins);
                if (!folded.isUndef()) {
                    // Replace instruction with assignment of constant
                    ins.op   = IROp::ASSIGN;
                    ins.arg1 = folded;
                    ins.arg2 = IRValue::makeUndef();
                    stats.instructionsChanged++;
                    changed = true;
                }
            }
        }
    }
}

// ═════════════════════════════════════════════════════════════
//  2. CONSTANT PROPAGATION
// ═════════════════════════════════════════════════════════════

void ConstantPropagationPass::buildConstantMap(IRFunction& fn) {
    constants.clear();
    // Count definitions per variable
    std::unordered_map<std::string, int> defCount;
    std::unordered_map<std::string, IRValue> lastDef;

    fn.forEachInstruction([&](const BasicBlock&, const IRInstruction& ins) {
        if (!ins.definesValue() || !ins.result.isTempOrVar()) return;
        defCount[ins.result.name]++;
        if ((ins.op == IROp::ASSIGN || ins.op == IROp::COPY) && ins.arg1.isConst())
            lastDef[ins.result.name] = ins.arg1;
        else
            lastDef[ins.result.name] = IRValue::makeUndef();
    });

    // A variable is constant iff it has exactly one definition
    // and that definition is an assignment from a constant
    for (auto& [name, count] : defCount) {
        if (count == 1) {
            auto it = lastDef.find(name);
            if (it != lastDef.end() && it->second.isConst())
                constants[name] = it->second;
        }
    }
}

IRValue ConstantPropagationPass::substitute(const IRValue& v) const {
    if (!v.isTempOrVar()) return v;
    auto it = constants.find(v.name);
    return (it != constants.end()) ? it->second : v;
}

void ConstantPropagationPass::rewriteUses(IRFunction& fn) {
    for (auto& bb : fn.blocks) {
        for (auto& ins : bb->instrs) {
            if (ins.op == IROp::PHI) continue; // don't rewrite phi sources here
            IRValue newArg1 = substitute(ins.arg1);
            IRValue newArg2 = substitute(ins.arg2);
            if (newArg1 != ins.arg1) { ins.arg1 = newArg1; stats.instructionsChanged++; }
            if (newArg2 != ins.arg2) { ins.arg2 = newArg2; stats.instructionsChanged++; }
            for (auto& ca : ins.callArgs) {
                IRValue nc = substitute(ca);
                if (nc != ca) { ca = nc; stats.instructionsChanged++; }
            }
        }
    }
}

void ConstantPropagationPass::run(IRFunction& fn) {
    stats = {};
    bool changed = true;
    while (changed) {
        int before = stats.instructionsChanged;
        buildConstantMap(fn);
        rewriteUses(fn);
        changed = (stats.instructionsChanged != before);
    }
}

// ═════════════════════════════════════════════════════════════
//  3. COMMON SUBEXPRESSION ELIMINATION (Global)
// ═════════════════════════════════════════════════════════════

CSEPass::ExprKey CSEPass::makeKey(const IRInstruction& ins) const {
    std::string a1 = ins.arg1.toString();
    std::string a2 = ins.arg2.toString();
    // Normalize commutative ops: always put smaller string first
    if (isCommutative(ins.op) && a1 > a2) std::swap(a1, a2);
    return {ins.op, a1, a2};
}

void CSEPass::invalidate(const std::string& defName) {
    // Remove any expression that uses defName as an operand
    for (auto it = exprMap.begin(); it != exprMap.end(); ) {
        if (it->first.arg1 == defName || it->first.arg2 == defName)
            it = exprMap.erase(it);
        else
            ++it;
    }
}

void CSEPass::run(IRFunction& fn) {
    stats  = {};
    exprMap.clear();

    // For global CSE we do a single forward pass; a full
    // availability analysis would require dominator tree traversal.
    for (auto& bb : fn.blocks) {
        exprMap.clear(); // reset per-block for safety (conservative)
        for (auto& ins : bb->instrs) {
            if (ins.isDead || !ins.definesValue()) {
                // Calls / stores invalidate the whole map
                if (ins.hasSideEffects()) exprMap.clear();
                continue;
            }

            if (!isBinaryOp(ins.op)) {
                if (ins.definesValue() && ins.result.isTempOrVar())
                    invalidate(ins.result.name);
                continue;
            }

            ExprKey key = makeKey(ins);
            auto it = exprMap.find(key);
            if (it != exprMap.end()) {
                // CSE hit: replace with a copy from the earlier result
                ins.op   = IROp::COPY;
                ins.arg1 = IRValue::makeTemp(it->second);
                ins.arg2 = IRValue::makeUndef();
                stats.instructionsChanged++;
            } else {
                if (ins.result.isTempOrVar())
                    exprMap[key] = ins.result.name;
            }

            // Invalidate expressions that use this definition
            if (ins.result.isTempOrVar())
                invalidate(ins.result.name);
        }
    }
}

// ═════════════════════════════════════════════════════════════
//  4. COPY PROPAGATION
// ═════════════════════════════════════════════════════════════

IRValue CopyPropagationPass::resolve(const IRValue& v) const {
    if (!v.isTempOrVar()) return v;
    auto it = copyOf.find(v.name);
    if (it == copyOf.end()) return v;
    // Chase the chain transitively
    IRValue cur = it->second;
    for (int depth = 0; depth < 16; depth++) {
        if (!cur.isTempOrVar()) break;
        auto jt = copyOf.find(cur.name);
        if (jt == copyOf.end()) break;
        cur = jt->second;
    }
    return cur;
}

void CopyPropagationPass::killCopiesOf(const std::string& name) {
    // Remove any copy whose source is 'name' (the source was redefined)
    for (auto it = copyOf.begin(); it != copyOf.end(); ) {
        if (it->second.isTempOrVar() && it->second.name == name)
            it = copyOf.erase(it);
        else
            ++it;
    }
    copyOf.erase(name);
}

void CopyPropagationPass::run(IRFunction& fn) {
    stats  = {};
    bool changed = true;
    while (changed) {
        changed = false;
        copyOf.clear();

        for (auto& bb : fn.blocks) {
            for (auto& ins : bb->instrs) {
                // Record copies
                if ((ins.op == IROp::ASSIGN || ins.op == IROp::COPY) &&
                    ins.result.isTempOrVar() && ins.arg1.isTempOrVar()) {
                    copyOf[ins.result.name] = ins.arg1;
                }

                // Substitute uses
                if (ins.op != IROp::PHI) {
                    IRValue newA1 = resolve(ins.arg1);
                    IRValue newA2 = resolve(ins.arg2);
                    if (newA1 != ins.arg1) { ins.arg1 = newA1; stats.instructionsChanged++; changed = true; }
                    if (newA2 != ins.arg2) { ins.arg2 = newA2; stats.instructionsChanged++; changed = true; }
                    for (auto& ca : ins.callArgs) {
                        IRValue nc = resolve(ca);
                        if (nc != ca) { ca = nc; stats.instructionsChanged++; changed = true; }
                    }
                }

                // Kill copies of redefined names
                if (ins.definesValue() && ins.result.isTempOrVar()) {
                    killCopiesOf(ins.result.name);
                    // If this instruction itself is not a pure copy, also
                    // remove it from the copy map
                    if (ins.op != IROp::ASSIGN && ins.op != IROp::COPY)
                        copyOf.erase(ins.result.name);
                }
            }
        }
    }
}

// ═════════════════════════════════════════════════════════════
//  5. DEAD CODE ELIMINATION
// ═════════════════════════════════════════════════════════════

void DeadCodeEliminationPass::markLive(const IRValue& v) {
    if (v.isTempOrVar()) live.insert(v.name);
}

bool DeadCodeEliminationPass::isLive(const IRValue& v) const {
    if (!v.isTempOrVar()) return true; // constants are always "live"
    return live.count(v.name) > 0;
}

bool DeadCodeEliminationPass::canEliminate(const IRInstruction& ins) const {
    if (ins.hasSideEffects()) return false;
    if (ins.isTerminator())   return false;
    if (ins.op == IROp::LABEL || ins.op == IROp::COMMENT) return false;
    if (!ins.definesValue())  return false;
    return !isLive(ins.result);
}

void DeadCodeEliminationPass::run(IRFunction& fn) {
    stats = {};
    bool changed = true;
    while (changed) {
        changed = false;
        live.clear();

        // Seed: terminator operands and call arguments are always live
        for (auto& bb : fn.blocks) {
            for (auto& ins : bb->instrs) {
                if (ins.isTerminator() || ins.hasSideEffects()) {
                    markLive(ins.arg1);
                    markLive(ins.arg2);
                    markLive(ins.result);
                    for (auto& ca : ins.callArgs) markLive(ca);
                }
                // Phi sources are live
                if (ins.op == IROp::PHI) {
                    for (auto& [lbl, v] : ins.phiSources) markLive(v);
                    markLive(ins.result);
                }
            }
        }

        // Backwards propagation: if result is live, mark uses live
        bool liveChanged = true;
        while (liveChanged) {
            liveChanged = false;
            for (auto& bb : fn.blocks) {
                for (auto& ins : bb->instrs) {
                    if (!ins.definesValue()) continue;
                    if (!isLive(ins.result)) continue;
                    auto before = live.size();
                    markLive(ins.arg1);
                    markLive(ins.arg2);
                    for (auto& ca : ins.callArgs) markLive(ca);
                    if (live.size() != before) liveChanged = true;
                }
            }
        }

        // Remove dead instructions
        for (auto& bb : fn.blocks) {
            auto& instrs = bb->instrs;
            size_t before = instrs.size();
            instrs.erase(
                std::remove_if(instrs.begin(), instrs.end(),
                    [&](const IRInstruction& ins) {
                        return canEliminate(ins);
                    }),
                instrs.end());
            int removed = (int)(before - instrs.size());
            stats.instructionsRemoved += removed;
            if (removed > 0) changed = true;
        }
    }
}

// ═════════════════════════════════════════════════════════════
//  6. STRENGTH REDUCTION
// ═════════════════════════════════════════════════════════════

bool StrengthReductionPass::isPowerOfTwo(int n, int& exp) const {
    if (n <= 0) return false;
    if ((n & (n-1)) != 0) return false;
    exp = 0;
    while (n > 1) { n >>= 1; exp++; }
    return true;
}

bool StrengthReductionPass::tryReduce(IRInstruction& ins) {
    if (!isBinaryOp(ins.op)) return false;

    const IRValue& l = ins.arg1;
    const IRValue& r = ins.arg2;

    // x + 0 → x
    if (ins.op == IROp::ADD && r.isZero()) {
        ins.op = IROp::ASSIGN; ins.arg1 = l; ins.arg2 = IRValue::makeUndef();
        return true;
    }
    if (ins.op == IROp::FADD && r.isZero()) {
        ins.op = IROp::ASSIGN; ins.arg1 = l; ins.arg2 = IRValue::makeUndef();
        return true;
    }
    // 0 + x → x
    if (ins.op == IROp::ADD && l.isZero()) {
        ins.op = IROp::ASSIGN; ins.arg1 = r; ins.arg2 = IRValue::makeUndef();
        return true;
    }

    // x - 0 → x
    if ((ins.op == IROp::SUB || ins.op == IROp::FSUB) && r.isZero()) {
        ins.op = IROp::ASSIGN; ins.arg1 = l; ins.arg2 = IRValue::makeUndef();
        return true;
    }

    // x * 1 → x
    if ((ins.op == IROp::MUL || ins.op == IROp::FMUL) && r.isOne()) {
        ins.op = IROp::ASSIGN; ins.arg1 = l; ins.arg2 = IRValue::makeUndef();
        return true;
    }
    // 1 * x → x
    if ((ins.op == IROp::MUL || ins.op == IROp::FMUL) && l.isOne()) {
        ins.op = IROp::ASSIGN; ins.arg1 = r; ins.arg2 = IRValue::makeUndef();
        return true;
    }

    // x * 0 → 0
    if (ins.op == IROp::MUL && (l.isZero() || r.isZero())) {
        ins.op = IROp::ASSIGN; ins.arg1 = IRValue::makeInt(0);
        ins.arg2 = IRValue::makeUndef();
        return true;
    }

    // x / 1 → x
    if ((ins.op == IROp::DIV || ins.op == IROp::FDIV) && r.isOne()) {
        ins.op = IROp::ASSIGN; ins.arg1 = l; ins.arg2 = IRValue::makeUndef();
        return true;
    }

    // x * 2^n → x << n  (integer only)
    if (ins.op == IROp::MUL && r.kind == IRValueKind::IntConst) {
        int exp;
        if (isPowerOfTwo(r.ival, exp)) {
            ins.op   = IROp::SHL;
            ins.arg2 = IRValue::makeInt(exp);
            return true;
        }
    }
    if (ins.op == IROp::MUL && l.kind == IRValueKind::IntConst) {
        int exp;
        if (isPowerOfTwo(l.ival, exp)) {
            ins.op   = IROp::SHL;
            ins.arg1 = r;
            ins.arg2 = IRValue::makeInt(exp);
            return true;
        }
    }

    // x / 2^n → x >> n
    if (ins.op == IROp::DIV && r.kind == IRValueKind::IntConst && r.ival > 0) {
        int exp;
        if (isPowerOfTwo(r.ival, exp)) {
            ins.op   = IROp::SHR;
            ins.arg2 = IRValue::makeInt(exp);
            return true;
        }
    }

    // x - x → 0
    if ((ins.op == IROp::SUB) && l == r) {
        ins.op = IROp::ASSIGN; ins.arg1 = IRValue::makeInt(0);
        ins.arg2 = IRValue::makeUndef();
        return true;
    }

    // x == x → 1
    if (ins.op == IROp::EQ && l == r) {
        ins.op = IROp::ASSIGN; ins.arg1 = IRValue::makeInt(1);
        ins.arg2 = IRValue::makeUndef();
        return true;
    }

    // x != x → 0
    if (ins.op == IROp::NEQ && l == r) {
        ins.op = IROp::ASSIGN; ins.arg1 = IRValue::makeInt(0);
        ins.arg2 = IRValue::makeUndef();
        return true;
    }

    // x * 2 → x + x
    if (ins.op == IROp::MUL && r.kind == IRValueKind::IntConst && r.ival == 2) {
        ins.op = IROp::ADD; ins.arg2 = l;  // arg1 stays l
        return true;
    }

    return false;
}

void StrengthReductionPass::run(IRFunction& fn) {
    stats = {};
    for (auto& bb : fn.blocks) {
        for (auto& ins : bb->instrs) {
            if (tryReduce(ins)) stats.instructionsChanged++;
        }
    }
}

// ═════════════════════════════════════════════════════════════
//  7. LOOP INVARIANT CODE MOTION (LICM)
// ═════════════════════════════════════════════════════════════

std::set<std::string> LICMPass::collectLoopDefs(
    const IRFunction& fn,
    const IRFunction::LoopInfo& loop) const
{
    std::set<std::string> defs;
    for (auto& lbl : loop.body) {
        const BasicBlock* bb = fn.getBlock(lbl);
        if (!bb) continue;
        for (auto& ins : bb->instrs)
            if (ins.definesValue() && ins.result.isTempOrVar())
                defs.insert(ins.result.name);
    }
    return defs;
}

bool LICMPass::isInvariant(const IRInstruction& ins,
                             const std::set<std::string>& loopDefs,
                             const std::set<std::string>& loopBlocks) const {
    if (!ins.definesValue()) return false;
    if (ins.hasSideEffects()) return false;
    if (ins.isTerminator()) return false;
    if (ins.op == IROp::LOAD || ins.op == IROp::ARRAY_LOAD) return false;
    if (ins.op == IROp::PHI) return false;

    // Check arg1
    if (ins.arg1.isTempOrVar() && loopDefs.count(ins.arg1.name)) return false;
    // Check arg2
    if (ins.arg2.isTempOrVar() && loopDefs.count(ins.arg2.name)) return false;
    // Check callArgs
    for (auto& ca : ins.callArgs)
        if (ca.isTempOrVar() && loopDefs.count(ca.name)) return false;

    return true;
}

BasicBlock* LICMPass::getOrCreatePreHeader(IRFunction& fn,
                                             const IRFunction::LoopInfo& loop) {
    // Find the loop header's predecessors that are outside the loop
    BasicBlock* header = fn.getBlock(loop.headerLabel);
    if (!header) return nullptr;

    std::string preHeaderLbl = loop.headerLabel + "_preheader";
    if (fn.getBlock(preHeaderLbl)) return fn.getBlock(preHeaderLbl);

    BasicBlock* preHeader = fn.addBlock(preHeaderLbl);
    // Redirect outside predecessors to the pre-header
    for (auto& predLbl : header->predecessors) {
        if (loop.body.count(predLbl)) continue; // skip back-edges
        BasicBlock* pred = fn.getBlock(predLbl);
        if (!pred) continue;
        // Rewrite pred's jump to header → pre-header
        for (auto& ins : pred->instrs) {
            if (ins.op == IROp::JUMP && ins.result.name == loop.headerLabel)
                ins.result.name = preHeaderLbl;
            if (ins.op == IROp::CJUMP) {
                if (ins.result.name == loop.headerLabel) ins.result.name = preHeaderLbl;
                if (ins.arg2.name   == loop.headerLabel) ins.arg2.name   = preHeaderLbl;
            }
        }
    }
    preHeader->instrs.push_back(IRInstruction::makeJump(loop.headerLabel));
    fn.rebuildEdges();
    return preHeader;
}

void LICMPass::run(IRFunction& fn) {
    stats = {};
    fn.rebuildEdges();
    fn.detectLoops();

    // FIX: snapshot loops before any structural CFG changes
    auto loopsCopy = fn.loops;

    for (auto& loop : loopsCopy) {
        auto loopDefs = collectLoopDefs(fn, loop);
        BasicBlock* preHeader = getOrCreatePreHeader(fn, loop);
        if (!preHeader) continue;

        // FIX: cap inner iterations
        const int MAX_LICM_ITER = 32;
        int iter = 0;
        bool changed = true;
        while (changed && iter++ < MAX_LICM_ITER) {
            changed = false;
            for (auto& lbl : loop.body) {
                BasicBlock* bb = fn.getBlock(lbl);
                if (!bb) continue;
                for (size_t i = 0; i < bb->instrs.size(); ) {
                    auto& ins = bb->instrs[i];
                    if (isInvariant(ins, loopDefs, loop.body)) {
                        auto& ph = preHeader->instrs;
                        ph.insert(ph.end() - 1, ins);
                        bb->instrs.erase(bb->instrs.begin() + i);
                        loopDefs.erase(ins.result.name);
                        stats.instructionsChanged++;
                        changed = true;
                    } else {
                        i++;
                    }
                }
            }
        }
    }
}

// ═════════════════════════════════════════════════════════════
//  8. INDUCTION VARIABLE ELIMINATION
// ═════════════════════════════════════════════════════════════

std::vector<InductionVarPass::InductionVar>
InductionVarPass::detectIVs(const IRFunction& fn,
                              const IRFunction::LoopInfo& loop) const {
    std::vector<InductionVar> ivs;

    // Find basic IVs: variable v where the only def in the loop is v = v + c
    for (auto& lbl : loop.body) {
        const BasicBlock* bb = fn.getBlock(lbl);
        if (!bb) continue;
        for (auto& ins : bb->instrs) {
            if (ins.op != IROp::ADD && ins.op != IROp::SUB) continue;
            if (!ins.result.isTempOrVar()) continue;
            const std::string& name = ins.result.name;
            // v = v + const  or  v = const + v
            bool selfRef = false; int step = 0;
            if (ins.arg1.isTempOrVar() && ins.arg1.name == name &&
                ins.arg2.kind == IRValueKind::IntConst) {
                selfRef = true;
                step = (ins.op == IROp::ADD) ? ins.arg2.ival : -ins.arg2.ival;
            } else if (ins.arg2.isTempOrVar() && ins.arg2.name == name &&
                       ins.arg1.kind == IRValueKind::IntConst) {
                selfRef = true;
                step = (ins.op == IROp::ADD) ? ins.arg1.ival : -ins.arg1.ival;
            }
            if (selfRef) {
                InductionVar iv;
                iv.name     = name;
                iv.step     = step;
                iv.init     = 0; // TODO: find init from pre-header
                iv.isBasic  = true;
                iv.isDerived= false;
                iv.base     = name;
                ivs.push_back(iv);
            }
        }
    }

    // Find derived IVs: j = a*i + b  where i is a basic IV
    std::set<std::string> basicNames;
    for (auto& iv : ivs) if (iv.isBasic) basicNames.insert(iv.name);

    for (auto& lbl : loop.body) {
        const BasicBlock* bb = fn.getBlock(lbl);
        if (!bb) continue;
        for (auto& ins : bb->instrs) {
            if (ins.op != IROp::MUL) continue;
            if (!ins.result.isTempOrVar()) continue;
            // j = a * i  where i is a basic IV
            std::string basicIV;
            int scale = 0;
            if (ins.arg1.isTempOrVar() && basicNames.count(ins.arg1.name) &&
                ins.arg2.kind == IRValueKind::IntConst) {
                basicIV = ins.arg1.name; scale = ins.arg2.ival;
            } else if (ins.arg2.isTempOrVar() && basicNames.count(ins.arg2.name) &&
                       ins.arg1.kind == IRValueKind::IntConst) {
                basicIV = ins.arg2.name; scale = ins.arg1.ival;
            }
            if (!basicIV.empty()) {
                InductionVar div;
                div.name      = ins.result.name;
                div.base      = basicIV;
                div.step      = scale; // step = scale * basicIV.step
                div.init      = 0;
                div.isBasic   = false;
                div.isDerived = true;
                ivs.push_back(div);
            }
        }
    }
    return ivs;
}

bool InductionVarPass::processLoop(IRFunction& fn,
                                    const IRFunction::LoopInfo& loop) {
    auto ivs = detectIVs(fn, loop);
    if (ivs.empty()) return false;

    bool changed = false;
    for (auto& div : ivs) {
        if (!div.isDerived) continue;

        // Find the basic IV this derives from
        InductionVar* basic = nullptr;
        for (auto& iv : ivs)
            if (iv.isBasic && iv.name == div.base) { basic = &iv; break; }
        if (!basic) continue;

        // Replace: j = scale * i  →  new IV j that increments by scale*step
        // In the loop body: remove the multiply, add j += scale*step
        for (auto& lbl : loop.body) {
            BasicBlock* bb = fn.getBlock(lbl);
            if (!bb) continue;
            for (auto& ins : bb->instrs) {
                if (ins.op == IROp::MUL &&
                    ins.result.isTempOrVar() && ins.result.name == div.name) {
                    // Replace multiply with increment
                    int incStep = div.step * basic->step;
                    ins.op   = IROp::ADD;
                    ins.arg1 = IRValue::makeVar(div.name);
                    ins.arg2 = IRValue::makeInt(incStep);
                    stats.instructionsChanged++;
                    changed = true;
                }
            }
        }
    }
    return changed;
}

void InductionVarPass::run(IRFunction& fn) {
    stats = {};
    fn.rebuildEdges();
    fn.detectLoops();
    for (auto& loop : fn.loops)
        processLoop(fn, loop);
}

// ═════════════════════════════════════════════════════════════
//  9. PEEPHOLE
// ═════════════════════════════════════════════════════════════

bool PeepholePass::patternNopRemoval(std::vector<IRInstruction>& ins, size_t i) {
    if (ins[i].op == IROp::NOP || ins[i].isDead) {
        ins.erase(ins.begin() + i);
        stats.instructionsRemoved++;
        return true;
    }
    return false;
}

bool PeepholePass::patternConstantBranch(std::vector<IRInstruction>& ins, size_t i) {
    auto& cur = ins[i];
    if (cur.op == IROp::CJUMP && cur.arg1.kind == IRValueKind::IntConst) {
        // if constant goto L1 else L2  →  goto (L1 or L2)
        std::string target = (cur.arg1.ival != 0)
            ? cur.result.name : cur.arg2.name;
        cur = IRInstruction::makeJump(target);
        stats.instructionsChanged++;
        return true;
    }
    if (cur.op == IROp::CJUMP_TRUE && cur.arg1.kind == IRValueKind::IntConst) {
        if (cur.arg1.ival != 0) { cur = IRInstruction::makeJump(cur.result.name); }
        else                    { cur.op = IROp::NOP; }
        stats.instructionsChanged++;
        return true;
    }
    if (cur.op == IROp::CJUMP_FALSE && cur.arg1.kind == IRValueKind::IntConst) {
        if (cur.arg1.ival == 0) { cur = IRInstruction::makeJump(cur.result.name); }
        else                    { cur.op = IROp::NOP; }
        stats.instructionsChanged++;
        return true;
    }
    return false;
}

bool PeepholePass::patternDoubleNeg(std::vector<IRInstruction>& ins, size_t i) {
    if (i + 1 >= ins.size()) return false;
    auto& a = ins[i];
    auto& b = ins[i+1];
    // t1 = neg x; t2 = neg t1  →  t2 = x
    if ((a.op == IROp::NEG || a.op == IROp::FNEG) &&
        (b.op == IROp::NEG || b.op == IROp::FNEG) &&
        b.arg1.isTempOrVar() && b.arg1.name == a.result.name) {
        b.op   = IROp::ASSIGN;
        b.arg1 = a.arg1;
        a.op   = IROp::NOP;
        stats.instructionsChanged++;
        return true;
    }
    // t1 = lnot x; t2 = lnot t1 → t2 = (x != 0)
    if (a.op == IROp::LNOT && b.op == IROp::LNOT &&
        b.arg1.isTempOrVar() && b.arg1.name == a.result.name) {
        // double-not is a cast to bool: x != 0
        b.op   = IROp::NEQ;
        b.arg1 = a.arg1;
        b.arg2 = IRValue::makeInt(0);
        a.op   = IROp::NOP;
        stats.instructionsChanged++;
        return true;
    }
    return false;
}

bool PeepholePass::patternRedundantCopy(std::vector<IRInstruction>& ins, size_t i) {
    if (i + 1 >= ins.size()) return false;
    auto& a = ins[i];
    auto& b = ins[i+1];
    // t1 = t2; t3 = t1  →  t3 = t2  (if t1 is only used once)
    if ((a.op == IROp::ASSIGN || a.op == IROp::COPY) &&
        a.result.isTempOrVar() && a.arg1.isTempOrVar() &&
        b.arg1.isTempOrVar() && b.arg1.name == a.result.name) {
        b.arg1 = a.arg1;
        a.op   = IROp::NOP;
        stats.instructionsChanged++;
        return true;
    }
    return false;
}

bool PeepholePass::patternIdentityOps(std::vector<IRInstruction>& ins, size_t i) {
    auto& cur = ins[i];
    // x = x (self-assign)
    if ((cur.op == IROp::ASSIGN || cur.op == IROp::COPY) &&
        cur.result == cur.arg1) {
        cur.op = IROp::NOP;
        stats.instructionsRemoved++;
        return true;
    }
    return false;
}

bool PeepholePass::patternRedundantJump(BasicBlock& bb, IRFunction& fn, size_t i) {
    auto& ins = bb.instrs;
    if (i + 1 != ins.size()) return false; // only last instruction
    if (ins[i].op != IROp::JUMP) return false;

    std::string target = ins[i].result.name;
    // Find the next block in layout order
    for (size_t bi = 0; bi + 1 < fn.blocks.size(); bi++) {
        if (fn.blocks[bi].get() == &bb) {
            if (fn.blocks[bi+1]->label == target) {
                // Jump to the immediately following block — remove it
                ins[i].op = IROp::NOP;
                stats.instructionsRemoved++;
                return true;
            }
        }
    }
    return false;
}

bool PeepholePass::runOnBlock(BasicBlock& bb) {
    // Not needed for block-level peephole; pattern methods operate on instrs vector
    return false;
}

void PeepholePass::run(IRFunction& fn) {
    stats = {};
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto& bb : fn.blocks) {
            auto& ins = bb->instrs;
            for (size_t i = 0; i < ins.size(); ) {
                bool did = patternNopRemoval(ins, i)        ||
                           patternConstantBranch(ins, i)    ||
                           patternDoubleNeg(ins, i)         ||
                           patternRedundantCopy(ins, i)     ||
                           patternIdentityOps(ins, i)       ||
                           patternRedundantJump(*bb, fn, i);
                if (did) { changed = true; /* don't advance */ }
                else     { i++; }
            }
        }
    }
}

// ═════════════════════════════════════════════════════════════
//  10. BASIC BLOCK OPTIMIZATION
// ═════════════════════════════════════════════════════════════

bool BasicBlockOptPass::removeUnreachable(IRFunction& fn) {
    fn.rebuildEdges();
    fn.markReachable();
    bool removed = false;
    auto& blocks = fn.blocks;
    for (auto it = blocks.begin(); it != blocks.end(); ) {
        if (!(*it)->isReachable && !(*it)->isEntry) {
            fn.blockMap.erase((*it)->label);
            it = blocks.erase(it);
            stats.blocksRemoved++;
            removed = true;
        } else {
            ++it;
        }
    }
    if (removed) fn.rebuildEdges();
    return removed;
}

bool BasicBlockOptPass::mergeBlocks(IRFunction& fn) {
    bool changed = false;
    for (auto& bb : fn.blocks) {
        // If bb has exactly one successor and that successor has exactly one predecessor
        if (bb->successors.size() != 1) continue;
        std::string succLbl = bb->successors[0];
        BasicBlock* succ = fn.getBlock(succLbl);
        if (!succ) continue;
        if (succ->predecessors.size() != 1) continue;
        if (succ->isEntry) continue;

        // Remove the jump from bb
        if (!bb->instrs.empty() && bb->instrs.back().op == IROp::JUMP)
            bb->instrs.pop_back();

        // Append succ's instructions to bb
        for (auto& ins : succ->instrs)
            bb->instrs.push_back(ins);

        // Transfer succ's successors
        bb->successors = succ->successors;

        // Update any blocks that had succ as a predecessor
        for (auto& s : bb->successors) {
            BasicBlock* sb = fn.getBlock(s);
            if (!sb) continue;
            for (auto& p : sb->predecessors)
                if (p == succLbl) p = bb->label;
        }

        // Remove succ
        fn.blockMap.erase(succLbl);
        fn.blocks.erase(std::remove_if(fn.blocks.begin(), fn.blocks.end(),
            [&](const std::unique_ptr<BasicBlock>& b) {
                return b.get() == succ;
            }), fn.blocks.end());

        stats.blocksRemoved++;
        changed = true;
        break; // restart after structural change
    }
    return changed;
}

bool BasicBlockOptPass::removeEmptyBlocks(IRFunction& fn) {
    bool changed = false;
    for (auto& bb : fn.blocks) {
        // An empty block has only a single unconditional jump
        if (bb->instrs.size() != 1) continue;
        if (bb->instrs[0].op != IROp::JUMP) continue;
        if (bb->isEntry || bb->isExit) continue;

        std::string target = bb->instrs[0].result.name;
        if (target == bb->label) continue; // self-loop

        // Redirect all predecessors to jump directly to target
        for (auto& predLbl : bb->predecessors) {
            BasicBlock* pred = fn.getBlock(predLbl);
            if (!pred) continue;
            for (auto& ins : pred->instrs) {
                if (ins.op == IROp::JUMP && ins.result.name == bb->label)
                    ins.result.name = target;
                if (ins.op == IROp::CJUMP) {
                    if (ins.result.name == bb->label) ins.result.name = target;
                    if (ins.arg2.name   == bb->label) ins.arg2.name   = target;
                }
            }
        }

        bb->isDead = true; // mark for removal
        changed = true;
    }

    if (changed) {
        fn.blocks.erase(std::remove_if(fn.blocks.begin(), fn.blocks.end(),
            [&](const std::unique_ptr<BasicBlock>& b) {
                if (b->isDead) {
                    fn.blockMap.erase(b->label);
                    stats.blocksRemoved++;
                    return true;
                }
                return false;
            }), fn.blocks.end());
        fn.rebuildEdges();
    }
    return changed;
}

bool BasicBlockOptPass::threadJumps(IRFunction& fn) {
    bool changed = false;
    for (auto& bb : fn.blocks) {
        auto* term = bb->terminator();
        if (!term) continue;

        auto tryThread = [&](std::string& targetLbl) {
            BasicBlock* tgt = fn.getBlock(targetLbl);
            if (!tgt || tgt == bb.get()) return;
            // If tgt contains only a jump, bypass it
            if (tgt->instrs.size() == 1 && tgt->instrs[0].op == IROp::JUMP) {
                targetLbl = tgt->instrs[0].result.name;
                stats.instructionsChanged++;
                changed = true;
            }
        };

        if (term->op == IROp::JUMP)
            tryThread(term->result.name);
        else if (term->op == IROp::CJUMP) {
            tryThread(term->result.name);
            tryThread(term->arg2.name);
        }
    }
    if (changed) fn.rebuildEdges();
    return changed;
}

void BasicBlockOptPass::run(IRFunction& fn) {
    stats = {};
    // FIX: cap iterations to prevent infinite oscillation
    const int MAX_ITER = 64;
    int iter = 0;
    bool changed = true;
    while (changed && iter++ < MAX_ITER) {
        changed  = removeUnreachable(fn);
        changed |= mergeBlocks(fn);
        changed |= removeEmptyBlocks(fn);
        changed |= threadJumps(fn);
    }
}
