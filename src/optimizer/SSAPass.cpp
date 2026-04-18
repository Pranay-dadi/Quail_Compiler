#include "optimizer/SSAPass.h"
#include <algorithm>
#include <iostream>

// ─────────────────────────────────────────────────────────────
//  Compute dominance frontiers
//  df[b] = { y | ∃ predecessor p of y such that b dom p
//                 but b does not strictly dominate y }
// ─────────────────────────────────────────────────────────────
std::unordered_map<int, std::set<int>>
SSAPass::computeDomFrontier(IRFunction& fn) {
    fn.computeDominators();

    std::unordered_map<int, std::set<int>> df;
    for (auto& bb : fn.blocks) df[bb->id] = {};

    // Classic algorithm: for each join point y with ≥2 preds,
    // walk up the dominator tree from each pred until we reach
    // idom(y), adding y to df[node] along the way.
    for (auto& bb : fn.blocks) {
        if (bb->predecessors.size() < 2) continue;
        for (auto& predLbl : bb->predecessors) {
            BasicBlock* pred = fn.getBlock(predLbl);
            if (!pred) continue;
            int runner = pred->id;
            while (runner != bb->idomId && runner != -1 && runner >= 0) {
                df[runner].insert(bb->id);
                int next = fn.blocks[runner]->idomId;
                if (next == runner) break;  // ← break on self-reference
                runner = next;
            }
        }
    }
    return df;
}

// ─────────────────────────────────────────────────────────────
//  Collect all variable/temp names that are defined in fn
// ─────────────────────────────────────────────────────────────
std::unordered_set<std::string> SSAPass::collectDefs(IRFunction& fn) {
    std::unordered_set<std::string> defs;
    fn.forEachInstruction([&](const BasicBlock&, const IRInstruction& ins) {
        if (ins.definesValue() && ins.result.isTempOrVar())
            defs.insert(ins.result.name);
    });
    return defs;
}

// ─────────────────────────────────────────────────────────────
//  For each variable, which block IDs assign to it?
// ─────────────────────────────────────────────────────────────
std::unordered_map<std::string, std::set<int>>
SSAPass::defBlocks(IRFunction& fn,
                   const std::unordered_set<std::string>& vars) {
    std::unordered_map<std::string, std::set<int>> db;
    for (auto& v : vars) db[v] = {};
    for (auto& bb : fn.blocks) {
        for (auto& ins : bb->instrs) {
            if (ins.definesValue() && ins.result.isTempOrVar())
                if (db.count(ins.result.name))
                    db[ins.result.name].insert(bb->id);
        }
    }
    return db;
}

// ─────────────────────────────────────────────────────────────
//  Step 2: Phi placement
//  For each variable v, insert phi(v) at each block in DF+(v)
//  (the iterated dominance frontier).
// ─────────────────────────────────────────────────────────────
void SSAPass::placePhi(IRFunction& fn,
                        const std::unordered_map<int, std::set<int>>& df) {
    auto vars   = collectDefs(fn);
    auto db     = defBlocks(fn, vars);

    for (auto& varName : vars) {
        // Worklist = blocks that define varName
        std::vector<int> worklist(db[varName].begin(), db[varName].end());
        std::set<int> phiPlaced;

        while (!worklist.empty()) {
            int bid = worklist.back(); worklist.pop_back();
            auto it = df.find(bid);
            if (it == df.end()) continue;
            for (int y : it->second) {
                if (phiPlaced.count(y)) continue;
                phiPlaced.insert(y);

                // Insert phi(varName) at beginning of block y
                BasicBlock* ybb = fn.blocks[y].get();

                // Build phi sources: one (undef) per predecessor
                std::vector<std::pair<std::string, IRValue>> srcs;
                for (auto& p : ybb->predecessors)
                    srcs.push_back({p, IRValue::makeVar(varName)});

                IRValue phiDst = IRValue::makeVar(varName);
                IRInstruction phi = IRInstruction::makePhi(phiDst, srcs);

                // Insert at front (after any existing phis)
                size_t insertPos = 0;
                while (insertPos < ybb->instrs.size() &&
                       ybb->instrs[insertPos].op == IROp::PHI)
                    insertPos++;
                ybb->instrs.insert(ybb->instrs.begin() + insertPos, phi);

                stats.phiInserted++;
                if (!db[varName].count(y)) {
                    db[varName].insert(y);
                    worklist.push_back(y);
                }
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────
//  Step 3: Renaming
//  DFS over dominator tree, maintaining a stack of version
//  numbers per variable.
// ─────────────────────────────────────────────────────────────
void SSAPass::renameBlock(
    IRFunction& fn,
    BasicBlock& bb,
    std::unordered_map<std::string, std::stack<std::string>>& stacks,
    std::unordered_map<std::string, int>& counters)
{
    // Track which variables we pushed in this block (for cleanup)
    std::vector<std::string> pushed;

    for (auto& ins : bb.instrs) {
        // Rename uses (except phi sources — handled at successor boundary)
        if (ins.op != IROp::PHI) {
            // arg1
            if (ins.arg1.isTempOrVar() && stacks.count(ins.arg1.name) &&
                !stacks[ins.arg1.name].empty()) {
                ins.arg1.name = stacks[ins.arg1.name].top();
                stats.renames++;
            }
            // arg2
            if (ins.arg2.isTempOrVar() && stacks.count(ins.arg2.name) &&
                !stacks[ins.arg2.name].empty()) {
                ins.arg2.name = stacks[ins.arg2.name].top();
                stats.renames++;
            }
            // callArgs
            for (auto& ca : ins.callArgs) {
                if (ca.isTempOrVar() && stacks.count(ca.name) &&
                    !stacks[ca.name].empty()) {
                    ca.name = stacks[ca.name].top();
                    stats.renames++;
                }
            }
        }

        // Rename definition
        if (ins.definesValue() && ins.result.isTempOrVar()) {
            std::string base = ins.result.name;
            if (!counters.count(base)) counters[base] = 0;
            int ver = counters[base]++;
            std::string newN = newName(base, ver);
            stacks[base].push(newN);
            pushed.push_back(base);
            ins.result.name = newN;
            stats.renames++;
        }
    }

    // Fill phi sources in successors
    for (auto& succLbl : bb.successors) {
        BasicBlock* succ = fn.getBlock(succLbl);
        if (!succ) continue;
        for (auto& phi : succ->instrs) {
            if (phi.op != IROp::PHI) break;
            std::string base = phi.result.name;
            // Find and update the source for this predecessor
            for (auto& [predLbl, val] : phi.phiSources) {
                if (predLbl == bb.label) {
                    if (val.isTempOrVar() && stacks.count(val.name) &&
                        !stacks[val.name].empty()) {
                        val.name = stacks[val.name].top();
                    }
                }
            }
        }
    }

    // Recurse into dominator children
    for (auto& child : fn.blocks) {
        if (!child->isReachable) continue;
        if (child->idomId < 0) continue;
        if (child->idomId == bb.id && child.get() != &bb)
            renameBlock(fn, *child, stacks, counters);
    }

    // Pop all versions pushed in this block
    for (auto& name : pushed)
        if (!stacks[name].empty())
            stacks[name].pop();
}

void SSAPass::rename(IRFunction& fn) {
    std::unordered_map<std::string, std::stack<std::string>> stacks;
    std::unordered_map<std::string, int> counters;

    // Initialize stacks for all defined variables
    fn.forEachInstruction([&](const BasicBlock&, const IRInstruction& ins) {
        if (ins.definesValue() && ins.result.isTempOrVar())
            if (!stacks.count(ins.result.name))
                stacks[ins.result.name] = {};
    });
    // Also initialize from phi sources
    for (auto& bb : fn.blocks)
        for (auto& ins : bb->instrs)
            if (ins.op == IROp::PHI)
                for (auto& [lbl, val] : ins.phiSources)
                    if (val.isTempOrVar() && !stacks.count(val.name))
                        stacks[val.name] = {};

    if (!fn.blocks.empty())
        renameBlock(fn, *fn.blocks.front(), stacks, counters);
}

// ─────────────────────────────────────────────────────────────
//  toSSA — full pipeline
// ─────────────────────────────────────────────────────────────
void SSAPass::toSSA(IRFunction& fn) {
    fn.rebuildEdges();
    fn.markReachable();
    fn.computeDominators();
    auto df = computeDomFrontier(fn);
    placePhi(fn, df);
    rename(fn);
}

// ─────────────────────────────────────────────────────────────
//  fromSSA — remove phi nodes by inserting copies on pred edges
// ─────────────────────────────────────────────────────────────
void SSAPass::insertCopiesForPhi(IRFunction& fn,
                                  BasicBlock& bb,
                                  const IRInstruction& phi)
{
    for (auto& [predLbl, srcVal] : phi.phiSources) {
        BasicBlock* pred = fn.getBlock(predLbl);
        if (!pred) continue;
        // Insert: phi.result = srcVal   before the terminator of pred
        IRInstruction copy = IRInstruction::makeAssign(phi.result, srcVal);
        auto& instrs = pred->instrs;
        size_t pos = instrs.size();
        // Insert before the terminator
        while (pos > 0 && instrs[pos-1].isTerminator()) --pos;
        instrs.insert(instrs.begin() + pos, copy);
        stats.phiRemoved++;
    }
}

void SSAPass::fromSSA(IRFunction& fn) {
    for (auto& bb : fn.blocks) {
        // Collect all phis first, then remove them
        std::vector<IRInstruction> phis;
        std::vector<IRInstruction> rest;
        for (auto& ins : bb->instrs) {
            if (ins.op == IROp::PHI) phis.push_back(ins);
            else                      rest.push_back(ins);
        }
        // Insert copies on predecessor edges
        for (auto& phi : phis)
            insertCopiesForPhi(fn, *bb, phi);
        // Replace block instructions without phis
        bb->instrs = std::move(rest);
    }
}
