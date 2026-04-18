#pragma once
// ============================================================
//  OptimizationPasses.h  —  Quail Compiler
//
//  All optimization passes operate on the IRModule/IRFunction
//  in-place.  Each pass exposes:
//    • run(IRFunction&) — apply to one function
//    • stats            — instruction counts removed/changed
//
//  Passes implemented:
//    ConstantFoldingPass       — fold constant expressions
//    ConstantPropagationPass   — propagate known values
//    CSEPass                   — global common subexpression elim
//    CopyPropagationPass       — eliminate redundant copies
//    DeadCodeEliminationPass   — remove unused definitions
//    StrengthReductionPass     — replace expensive ops with cheap ones
//    LICMPass                  — loop invariant code motion
//    InductionVarPass          — induction variable elimination
//    PeepholePass              — local peephole window optimizations
//    BasicBlockOptPass         — merge/simplify basic blocks
// ============================================================
#include "ir/IRCFG.h"
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <cmath>

// ─────────────────────────────────────────────────────────────
//  Base class for all passes
// ─────────────────────────────────────────────────────────────
struct PassStats {
    int instructionsRemoved = 0;
    int instructionsChanged = 0;
    int blocksRemoved       = 0;
    int phiRemoved          = 0;
};

class IRPass {
public:
    virtual ~IRPass() = default;
    virtual void run(IRFunction& fn) = 0;
    const PassStats& getStats() const { return stats; }
    virtual std::string name() const = 0;
protected:
    PassStats stats;
};

// ─────────────────────────────────────────────────────────────
//  1. Constant Folding
//     Replace binary/unary operations on constants with
//     the computed result at compile time.
//     e.g.  t1 = 3 + 4  →  t1 = 7
//           t2 = !0      →  t2 = 1
// ─────────────────────────────────────────────────────────────
class ConstantFoldingPass : public IRPass {
public:
    std::string name() const override { return "ConstantFolding"; }
    void run(IRFunction& fn) override;

private:
    // Returns folded value, or makeUndef() if not foldable
    IRValue tryFold(const IRInstruction& ins) const;
    IRValue foldBinary(IROp op, const IRValue& l, const IRValue& r) const;
    IRValue foldUnary(IROp op, const IRValue& v) const;
};

// ─────────────────────────────────────────────────────────────
//  2. Constant Propagation
//     Track which temps/vars hold constant values and
//     substitute constants for uses of those names.
//     Uses a simple worklist over the whole function.
//     e.g.  x = 5; y = x + 2  →  y = 5 + 2  (then folded)
// ─────────────────────────────────────────────────────────────
class ConstantPropagationPass : public IRPass {
public:
    std::string name() const override { return "ConstantPropagation"; }
    void run(IRFunction& fn) override;

private:
    // name → constant value (for names with a single constant def)
    std::unordered_map<std::string, IRValue> constants;

    void buildConstantMap(IRFunction& fn);
    IRValue substitute(const IRValue& v) const;
    void rewriteUses(IRFunction& fn);
};

// ─────────────────────────────────────────────────────────────
//  3. Common Subexpression Elimination (Global CSE)
//     Detect computations that produce the same value on
//     all paths and replace later occurrences with copies
//     of the first occurrence.
//     Operates at function scope (not just per-block).
//     e.g.  t1 = a + b; ... t2 = a + b  →  t2 = t1
// ─────────────────────────────────────────────────────────────
class CSEPass : public IRPass {
public:
    std::string name() const override { return "CSE"; }
    void run(IRFunction& fn) override;

private:
    struct ExprKey {
        IROp        op;
        std::string arg1;
        std::string arg2;
        bool operator==(const ExprKey& o) const {
            return op == o.op && arg1 == o.arg1 && arg2 == o.arg2;
        }
    };
    struct ExprKeyHash {
        size_t operator()(const ExprKey& k) const {
            return std::hash<int>{}((int)k.op) ^
                   std::hash<std::string>{}(k.arg1) << 1 ^
                   std::hash<std::string>{}(k.arg2) << 2;
        }
    };

    // exprMap[key] = name of temp that already holds this value
    std::unordered_map<ExprKey, std::string, ExprKeyHash> exprMap;

    ExprKey makeKey(const IRInstruction& ins) const;
    void    invalidate(const std::string& defName);
};

// ─────────────────────────────────────────────────────────────
//  4. Copy Propagation
//     Replace uses of copies (t2 = t1) with the original
//     name (t1), then the dead copy can be removed by DCE.
//     e.g.  t2 = t1; x = t2 + 1  →  x = t1 + 1
// ─────────────────────────────────────────────────────────────
class CopyPropagationPass : public IRPass {
public:
    std::string name() const override { return "CopyPropagation"; }
    void run(IRFunction& fn) override;

private:
    // copyOf[t2] = t1  (t2 is a copy of t1)
    std::unordered_map<std::string, IRValue> copyOf;

    IRValue resolve(const IRValue& v) const;
    void    killCopiesOf(const std::string& name);
};

// ─────────────────────────────────────────────────────────────
//  5. Dead Code Elimination
//     Remove instructions whose results are never used
//     (pure operations with no side effects).
//     Uses a backwards liveness walk — an instruction is live
//     iff its result is used by a live instruction.
//     e.g.  t1 = a + b;  (t1 never used) → removed
// ─────────────────────────────────────────────────────────────
class DeadCodeEliminationPass : public IRPass {
public:
    std::string name() const override { return "DCE"; }
    void run(IRFunction& fn) override;

private:
    // live set: names that are currently needed
    std::unordered_set<std::string> live;

    void markLive(const IRValue& v);
    bool isLive(const IRValue& v) const;
    // True if the instruction can be deleted when its result is dead
    bool canEliminate(const IRInstruction& ins) const;
};

// ─────────────────────────────────────────────────────────────
//  6. Strength Reduction
//     Replace expensive operations with cheaper equivalents:
//       x * 2    → x + x   (or x << 1 if targeting asm)
//       x * 1    → x       (identity)
//       x * 0    → 0       (zero)
//       x / 1    → x
//       x + 0    → x
//       x - 0    → x
//       x - x    → 0
//       x * 2^n  → x << n  (power-of-2 mult)
//       x / 2^n  → x >> n  (power-of-2 div, signed only)
//       x == x   → 1
//       x != x   → 0
// ─────────────────────────────────────────────────────────────
class StrengthReductionPass : public IRPass {
public:
    std::string name() const override { return "StrengthReduction"; }
    void run(IRFunction& fn) override;

private:
    // Returns a simplified instruction, or the original if no reduction applies
    bool tryReduce(IRInstruction& ins);
    bool isPowerOfTwo(int n, int& exp) const;
};

// ─────────────────────────────────────────────────────────────
//  7. Loop Invariant Code Motion (LICM)
//     Move instructions that compute the same value on every
//     iteration of a loop to the loop's pre-header block.
//
//     An instruction in loop L is invariant if all its operands
//     are either constants or are defined outside L.
//
//     e.g.  for(i=0;i<n;i++) { x = a+b; arr[i]=x*2; }
//           →  x = a+b; x2 = x*2; for(...) { arr[i] = x2; }
// ─────────────────────────────────────────────────────────────
class LICMPass : public IRPass {
public:
    std::string name() const override { return "LICM"; }
    void run(IRFunction& fn) override;

private:
    // Returns true if ins is loop-invariant in the given loop
    bool isInvariant(const IRInstruction& ins,
                     const std::set<std::string>& loopDefs,
                     const std::set<std::string>& loopBlocks) const;

    // Build set of all variable names defined inside the loop
    std::set<std::string> collectLoopDefs(
        const IRFunction& fn,
        const IRFunction::LoopInfo& loop) const;

    // Create or find a pre-header block for the loop
    BasicBlock* getOrCreatePreHeader(IRFunction& fn,
                                     const IRFunction::LoopInfo& loop);
};

// ─────────────────────────────────────────────────────────────
//  8. Induction Variable Elimination
//     Detect basic induction variables (i = i + c in a loop)
//     and derived induction variables (j = a*i + b).
//     Eliminate derived IVs by replacing them with a new
//     basic IV that is updated directly, removing the
//     expensive multiply.
//
//     e.g.  for(i=0;i<n;i++) { j=4*i; arr[j]=... }
//           →  for(i=0,j=0; i<n; i++,j+=4) { arr[j]=... }
// ─────────────────────────────────────────────────────────────
class InductionVarPass : public IRPass {
public:
    std::string name() const override { return "InductionVar"; }
    void run(IRFunction& fn) override;

private:
    struct InductionVar {
        std::string name;   // variable name
        std::string base;   // name of the basic IV it derives from
        int         step;   // additive step per iteration
        int         init;   // initial value
        bool        isBasic;
        bool        isDerived;
    };

    std::vector<InductionVar> detectIVs(
        const IRFunction& fn,
        const IRFunction::LoopInfo& loop) const;

    bool processLoop(IRFunction& fn, const IRFunction::LoopInfo& loop);
};

// ─────────────────────────────────────────────────────────────
//  9. Peephole Optimization
//     Apply a sliding window of 1–4 instructions and replace
//     recognizable patterns with shorter / faster sequences.
//
//     Patterns handled:
//       NOP removal
//       copy then immediate use → direct use
//       double negation: neg(neg(x)) → x
//       redundant jump to next block → fall-through
//       branch on constant: if 1 goto L1 else L2 → goto L1
//       add 0 / sub 0 / mul 1 / div 1 (missed by strength red.)
//       compare with constant + branch → simpler branch
// ─────────────────────────────────────────────────────────────
class PeepholePass : public IRPass {
public:
    std::string name() const override { return "Peephole"; }
    void run(IRFunction& fn) override;

private:
    bool runOnBlock(BasicBlock& bb);

    // Pattern matchers (return true if pattern was replaced)
    bool patternNopRemoval(std::vector<IRInstruction>& ins, size_t i);
    bool patternConstantBranch(std::vector<IRInstruction>& ins, size_t i);
    bool patternDoubleNeg(std::vector<IRInstruction>& ins, size_t i);
    bool patternRedundantCopy(std::vector<IRInstruction>& ins, size_t i);
    bool patternIdentityOps(std::vector<IRInstruction>& ins, size_t i);
    bool patternRedundantJump(BasicBlock& bb, IRFunction& fn, size_t i);
};

// ─────────────────────────────────────────────────────────────
//  10. Basic Block Optimization
//     Simplify the CFG at the basic block level:
//       • Remove unreachable blocks
//       • Merge a block with its unique predecessor if that
//         predecessor has only one successor
//       • Remove empty blocks (only a jump)
//       • Thread jumps: if B ends with jump to C and C has
//         only one predecessor, inline C into B
//       • Eliminate critical edges (for register allocation)
// ─────────────────────────────────────────────────────────────
class BasicBlockOptPass : public IRPass {
public:
    std::string name() const override { return "BasicBlockOpt"; }
    void run(IRFunction& fn) override;

private:
    bool removeUnreachable(IRFunction& fn);
    bool mergeBlocks(IRFunction& fn);
    bool removeEmptyBlocks(IRFunction& fn);
    bool threadJumps(IRFunction& fn);
};
