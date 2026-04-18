#pragma once
// ============================================================
//  RegisterAllocator.h  —  Quail Compiler
//
//  Graph-coloring register allocator for the Quail IR.
//
//  Algorithm: Chaitin-Briggs (simplified)
//    1. Build interference graph from liveness analysis
//    2. Simplify: repeatedly remove low-degree (<K) nodes
//    3. Spill: if no low-degree node, pick a spill candidate
//    4. Select: rebuild the graph assigning colours (registers)
//    5. Spill code: rewrite spilled temps with load/store pairs
//
//  This allocator targets an abstract machine with K registers
//  named r0..r(K-1).  The register assignment can be used
//  directly by a backend or just for analysis/statistics.
//
//  Special registers:
//    r0         — return value
//    r1..rK-3   — general purpose
//    rSP        — stack pointer (never allocated)
//    rFP        — frame pointer (never allocated)
// ============================================================
#include "ir/IRCFG.h"
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <vector>
#include <string>
#include <stack>

class RegisterAllocator {
public:
    explicit RegisterAllocator(int numRegisters = 8);

    // Run allocation on a function.
    // After this call, getAssignment() returns the register map,
    // and getSpills() returns the set of temporaries that were spilled.
    void allocate(IRFunction& fn);

    // Returns register name for a temp/var (empty string = spilled)
    std::string getRegister(const std::string& name) const {
        auto it = assignment.find(name);
        return (it != assignment.end()) ? it->second : "";
    }

    bool isSpilled(const std::string& name) const {
        return spilled.count(name) > 0;
    }

    const std::unordered_map<std::string, std::string>& getAssignment() const {
        return assignment;
    }
    const std::unordered_set<std::string>& getSpills() const {
        return spilled;
    }

    struct Stats {
        int totalTemps      = 0;
        int registersUsed   = 0;
        int spillCount      = 0;
        int spiltInstr      = 0; // load+store instructions added
        int interferenceEdges = 0;
    };
    const Stats& getStats() const { return stats; }

    // Print a human-readable register allocation report
    void printReport(const IRFunction& fn) const;

private:
    int K; // number of available registers
    std::unordered_map<std::string, std::string> assignment; // temp → register name
    std::unordered_set<std::string> spilled;
    Stats stats;

    // Interference graph: adjacency set per node
    std::unordered_map<std::string, std::unordered_set<std::string>> graph;

    // ── Phase 1: Build interference graph from liveness ──────
    void buildInterferenceGraph(IRFunction& fn);
    void addEdge(const std::string& a, const std::string& b);

    // ── Phase 2: Chaitin-Briggs simplification ────────────────
    // Returns ordered stack of nodes to color
    std::stack<std::string> simplify();

    // ── Phase 3: Spill selection ──────────────────────────────
    // Pick the best spill candidate (highest degree / lowest use count)
    std::string pickSpill(const std::unordered_set<std::string>& remaining,
                          const IRFunction& fn) const;

    // ── Phase 4: Coloring ─────────────────────────────────────
    void colorNodes(const std::stack<std::string>& order);

    // ── Phase 5: Spill code insertion ─────────────────────────
    void insertSpillCode(IRFunction& fn);

    // Return the register name for index i
    static std::string regName(int i) { return "r" + std::to_string(i); }

    // Collect all live names in the function
    std::unordered_set<std::string> collectAllTemps(const IRFunction& fn) const;

    // Use-count per variable (for spill cost heuristic)
    std::unordered_map<std::string, int> useCount;
    void countUses(const IRFunction& fn);
};
