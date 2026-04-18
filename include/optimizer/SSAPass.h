#pragma once
// ============================================================
//  SSAPass.h  —  Quail Compiler
//
//  Converts the IR from stack-variable form into SSA form
//  using the classic Cytron et al. algorithm:
//
//    1. Compute dominance frontiers
//    2. Insert phi functions at merge points where a variable
//       has multiple reaching definitions
//    3. Rename variables so each definition is unique
//       (x → x.0, x.1, x.2, ...)
//
//  After this pass every variable is defined exactly once
//  (SSA property), enabling more aggressive optimization.
//
//  An inverse pass (fromSSA) removes phi nodes by inserting
//  copy instructions on predecessor edges, preparing for
//  register allocation or LLVM lowering.
// ============================================================
#include "ir/IRCFG.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <stack>
#include <set>

class SSAPass {
public:
    // Convert fn to SSA form in-place.
    void toSSA(IRFunction& fn);

    // Convert fn back from SSA (remove phi nodes).
    void fromSSA(IRFunction& fn);

    struct Stats {
        int phiInserted  = 0;
        int renames      = 0;
        int phiRemoved   = 0;
    };
    const Stats& getStats() const { return stats; }

private:
    Stats stats;

    // ── Step 1: Dominance frontier computation ────────────────
    // df[b] = set of block ids where b's dominance ends
    std::unordered_map<int, std::set<int>> computeDomFrontier(IRFunction& fn);

    // ── Step 2: Phi placement ────────────────────────────────
    void placePhi(IRFunction& fn,
                  const std::unordered_map<int, std::set<int>>& df);

    // Collect all variables that are assigned in the function
    std::unordered_set<std::string> collectDefs(IRFunction& fn);

    // For each variable, which blocks assign it?
    std::unordered_map<std::string, std::set<int>>
    defBlocks(IRFunction& fn,
              const std::unordered_set<std::string>& vars);

    // ── Step 3: Renaming ─────────────────────────────────────
    void rename(IRFunction& fn);

    // Rename within one block, recursing into dominator children
    void renameBlock(IRFunction& fn, BasicBlock& bb,
                     std::unordered_map<std::string, std::stack<std::string>>& stacks,
                     std::unordered_map<std::string, int>& counters);

    std::string newName(const std::string& base, int version) const {
        return base + "." + std::to_string(version);
    }

    // ── fromSSA helpers ───────────────────────────────────────
    // Insert copies on each predecessor edge for a phi node
    void insertCopiesForPhi(IRFunction& fn, BasicBlock& bb,
                             const IRInstruction& phi);
};
