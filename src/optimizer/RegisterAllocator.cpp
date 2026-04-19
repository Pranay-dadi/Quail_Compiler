#include "optimizer/RegisterAllocator.h"
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <sstream>

RegisterAllocator::RegisterAllocator(int numRegisters)
    : K(numRegisters) {}

// ─────────────────────────────────────────────────────────────
//  collectAllTemps
// ─────────────────────────────────────────────────────────────
std::unordered_set<std::string>
RegisterAllocator::collectAllTemps(const IRFunction& fn) const {
    std::unordered_set<std::string> temps;
    fn.forEachInstruction([&](const BasicBlock&, const IRInstruction& ins) {
        if (ins.definesValue() && ins.result.isTemp())
            temps.insert(ins.result.name);
        if (ins.arg1.isTemp()) temps.insert(ins.arg1.name);
        if (ins.arg2.isTemp()) temps.insert(ins.arg2.name);
        for (auto& ca : ins.callArgs)
            if (ca.isTemp()) temps.insert(ca.name);
    });
    return temps;
}

// ─────────────────────────────────────────────────────────────
//  countUses — heuristic for spill cost
// ─────────────────────────────────────────────────────────────
void RegisterAllocator::countUses(const IRFunction& fn) {
    useCount.clear();
    fn.forEachInstruction([&](const BasicBlock& bb, const IRInstruction& ins) {
        int weight = bb.loopDepth > 0 ? (10 * bb.loopDepth) : 1;
        auto bump = [&](const IRValue& v) {
            if (v.isTemp()) useCount[v.name] += weight;
        };
        bump(ins.arg1); bump(ins.arg2); bump(ins.result);
        for (auto& ca : ins.callArgs) bump(ca);
    });
}

// ─────────────────────────────────────────────────────────────
//  buildInterferenceGraph
//
//  Two temporaries interfere if they are simultaneously live.
//  We compute per-block liveness (liveOut) and then for each
//  instruction in reverse, determine the live set at that point
//  and add edges between all simultaneously-live temps.
// ─────────────────────────────────────────────────────────────
void RegisterAllocator::addEdge(const std::string& a, const std::string& b) {
    if (a == b) return;
    graph[a].insert(b);
    graph[b].insert(a);
    stats.interferenceEdges++;
}

void RegisterAllocator::buildInterferenceGraph(IRFunction& fn) {
    graph.clear();
    const_cast<IRFunction&>(fn).computeLiveness();
    auto allTemps = collectAllTemps(fn);

    for (auto& bb : fn.blocks) {
        // Start with liveOut set
        std::unordered_set<std::string> live;
        for (auto& v : bb->liveOut)
            if (!v.empty() && allTemps.count(v)) live.insert(v);

        // Walk instructions in reverse
        for (int i = (int)bb->instrs.size() - 1; i >= 0; i--) {
            const auto& ins = bb->instrs[i];

            // The definition is live at this point — interferes with all others
            if (ins.definesValue() && ins.result.isTemp()) {
                for (auto& other : live)
                    addEdge(ins.result.name, other);
                live.erase(ins.result.name);
                graph[ins.result.name]; // ensure node exists
            }

            // Uses become live
            auto addLive = [&](const IRValue& v) {
                if (v.isTemp() && allTemps.count(v.name)) live.insert(v.name);
            };
            addLive(ins.arg1); addLive(ins.arg2);
            for (auto& ca : ins.callArgs) addLive(ca);
            if (ins.op == IROp::PHI)
                for (auto& [lbl, v] : ins.phiSources) addLive(v);
        }
    }
    stats.totalTemps = (int)graph.size();
}

// ─────────────────────────────────────────────────────────────
//  Simplify — Chaitin-Briggs
//  Repeatedly push nodes with degree < K onto the stack.
//  If no such node exists, we must spill.
// ─────────────────────────────────────────────────────────────
std::stack<std::string> RegisterAllocator::simplify() {
    std::stack<std::string> order;
    std::unordered_map<std::string, std::unordered_set<std::string>> g = graph;

    while (!g.empty()) {
        // Find a node with degree < K
        std::string chosen;
        for (auto& [node, neighbors] : g) {
            if ((int)neighbors.size() < K) {
                chosen = node;
                break;
            }
        }

        if (chosen.empty()) {
            // Spill: pick node with lowest spill cost (highest degree / fewest uses)
            int bestScore = -1;
            for (auto& [node, neighbors] : g) {
                int useFreq = useCount.count(node) ? useCount.at(node) : 1;
                int score = (int)neighbors.size() * 100 / (useFreq + 1);
                if (score > bestScore) { bestScore = score; chosen = node; }
            }
            if (chosen.empty()) break;
            spilled.insert(chosen);
            stats.spillCount++;
        }

        // Remove chosen from graph
        order.push(chosen);
        for (auto& nb : g[chosen])
            g[nb].erase(chosen);
        g.erase(chosen);
    }
    return order;
}

// ─────────────────────────────────────────────────────────────
//  colorNodes — assign registers by popping the stack
// ─────────────────────────────────────────────────────────────
void RegisterAllocator::colorNodes(const std::stack<std::string>& orderIn) {
    assignment.clear();
    std::stack<std::string> order = orderIn; // copy

    // Reverse the stack (we want to color in reverse simplify order)
    std::vector<std::string> nodes;
    while (!order.empty()) { nodes.push_back(order.top()); order.pop(); }

    for (int i = (int)nodes.size() - 1; i >= 0; i--) {
        const std::string& node = nodes[i];
        if (spilled.count(node)) continue;

        // Determine which registers are used by neighbors
        std::set<int> usedColors;
        auto it = graph.find(node);
        if (it != graph.end()) {
            for (auto& nb : it->second) {
                auto jt = assignment.find(nb);
                if (jt != assignment.end()) {
                    // Parse register number from "rN"
                    try {
                        int c = std::stoi(jt->second.substr(1));
                        usedColors.insert(c);
                    } catch (...) {}
                }
            }
        }

        // Assign the lowest available color
        bool assigned = false;
        for (int c = 0; c < K; c++) {
            if (!usedColors.count(c)) {
                assignment[node] = regName(c);
                assigned = true;
                break;
            }
        }

        if (!assigned) {
            // Could not color — spill this node
            spilled.insert(node);
            stats.spillCount++;
        }
    }

    // Count distinct registers used
    std::set<std::string> used;
    for (auto& [n, r] : assignment) used.insert(r);
    stats.registersUsed = (int)used.size();
}

// ─────────────────────────────────────────────────────────────
//  insertSpillCode
//  For each spilled temporary, insert:
//    - STORE to stack slot after each definition
//    - LOAD from stack slot before each use
// ─────────────────────────────────────────────────────────────
void RegisterAllocator::insertSpillCode(IRFunction& fn) {
    if (spilled.empty()) return;

    // Assign stack slot names
    std::unordered_map<std::string, std::string> slotName;
    int slotId = 0;
    for (auto& s : spilled)
        slotName[s] = "sp_" + std::to_string(slotId++);

    for (auto& bb : fn.blocks) {
        std::vector<IRInstruction> newInstrs;
        for (auto& ins : bb->instrs) {
            // Insert LOADs before uses of spilled temps
            auto insertLoad = [&](IRValue& v) {
                if (!v.isTemp() || !spilled.count(v.name)) return;
                std::string freshTemp = v.name + "_reload";
                IRInstruction load;
                load.op     = IROp::LOAD;
                load.result = IRValue::makeTemp(freshTemp);
                load.arg1   = IRValue::makeVar(slotName[v.name]);
                newInstrs.push_back(load);
                v.name = freshTemp; // redirect use to reloaded temp
                stats.spiltInstr++;
            };

            IRInstruction mutableIns = ins;
            insertLoad(mutableIns.arg1);
            insertLoad(mutableIns.arg2);
            for (auto& ca : mutableIns.callArgs) insertLoad(ca);

            newInstrs.push_back(mutableIns);

            // Insert STORE after definition of spilled temp
            if (ins.definesValue() && ins.result.isTemp() &&
                spilled.count(ins.result.name)) {
                IRInstruction store;
                store.op   = IROp::STORE;
                store.arg1 = IRValue::makeVar(slotName[ins.result.name]);
                store.arg2 = ins.result;
                newInstrs.push_back(store);
                stats.spiltInstr++;
            }
        }
        bb->instrs = std::move(newInstrs);
    }
}

// ─────────────────────────────────────────────────────────────
//  allocate — full pipeline
// ─────────────────────────────────────────────────────────────
void RegisterAllocator::allocate(IRFunction& fn) {
    stats   = {};
    spilled.clear();
    assignment.clear();

    fn.rebuildEdges();
    countUses(fn);
    buildInterferenceGraph(fn);

    // Iteratively allocate until no new spills
    // (spill code generates new temps that need allocation)
    int maxIter = 5;
    for (int iter = 0; iter < maxIter; iter++) {
        std::stack<std::string> order = simplify();
        colorNodes(order);
        if (spilled.empty()) break;
        insertSpillCode(fn);
        // Re-analyse after spill code insertion
        buildInterferenceGraph(fn);
    }
    fnAssignment[fn.name] = assignment;
    fnSpilled[fn.name]    = spilled;
    fnStats[fn.name]      = stats;
}

// ─────────────────────────────────────────────────────────────
//  printReport
// ─────────────────────────────────────────────────────────────
void RegisterAllocator::printReport(const IRFunction& fn) const {
    static const char* BOLD  = "\033[1m";
    static const char* GREEN = "\033[1;32m";
    static const char* RED   = "\033[1;31m";
    static const char* CYAN  = "\033[1;36m";
    static const char* DIM   = "\033[2m";
    static const char* RESET = "\033[0m";
    const auto& asgn = fnAssignment.count(fn.name) ? fnAssignment.at(fn.name) : assignment;
    const auto& spll = fnSpilled.count(fn.name) ? fnSpilled.at(fn.name)    : spilled;
    const Stats& st  = fnStats.count(fn.name) ? fnStats.at(fn.name)      : stats;

    std::cout << "\n" << BOLD << CYAN
              << "── Register Allocation: " << fn.name << "() ─────────────\n"
              << RESET;
    std::cout << "  Temporaries : " << BOLD << stats.totalTemps     << RESET << "\n"
              << "  Registers K : " << BOLD << K                    << RESET << "\n"
              << "  Colors used : " << BOLD << stats.registersUsed  << RESET << "\n"
              << "  Spilled     : " << (stats.spillCount > 0 ? RED : GREEN)
              << BOLD << stats.spillCount << RESET << "\n"
              << "  Spill instrs: " << BOLD << stats.spiltInstr     << RESET << "\n"
              << "  IF edges    : " << BOLD << stats.interferenceEdges << RESET << "\n\n";

    // Print assignment table
    if (!asgn.empty()) {
        std::cout << DIM << std::left
                  << std::setw(20) << "  Temporary"
                  << std::setw(10) << "Register" << "\n"
                  << "  " << std::string(28,'-') << "\n" << RESET;
        std::vector<std::pair<std::string,std::string>> sorted(
            asgn.begin(), asgn.end());
        std::sort(sorted.begin(), sorted.end());
        for (auto& [name, reg] : sorted)
            std::cout << "  " << std::left << std::setw(20) << name
                      << GREEN << reg << RESET << "\n";
    }

    if (!spll.empty()) {
        std::cout << "\n" << RED << "  Spilled temporaries:\n" << RESET;
        for (auto& s : spll)
            std::cout << "    " << s << " → stack slot\n";
    }
    std::cout << "\n";
}
