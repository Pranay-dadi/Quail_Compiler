// ============================================================
//  CFGAnalyzer.cpp  —  Quail Compiler
// ============================================================
#include "analysis/CFGAnalyzer.h"
#include <llvm/IR/CFG.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/Support/raw_ostream.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cstdlib>
#include <cassert>

// ── ANSI helpers (local) ──────────────────────────────────────
namespace {
    const char* R  = "\033[0m";
    const char* B  = "\033[1m";
    const char* RD = "\033[1;31m";
    const char* GR = "\033[1;32m";
    const char* YL = "\033[1;33m";
    const char* CY = "\033[1;36m";
    const char* MG = "\033[1;35m";
    const char* DM = "\033[2m";
}

// ─────────────────────────────────────────────────────────────
CFGAnalyzer::CFGAnalyzer(llvm::Module* mod) : module_(mod) {}

// ─────────────────────────────────────────────────────────────
//  analyze() — entry point
// ─────────────────────────────────────────────────────────────
void CFGAnalyzer::analyze() {
    functions_.clear();
    callEdges_.clear();
    rawCallGraph_.clear();

    for (auto& fn : *module_) {
        if (fn.isDeclaration()) continue;
        analyzeFunction(fn);
    }

    // Flatten raw call graph → callEdges_
    std::set<std::string> definedFns;
    for (auto& f : functions_) definedFns.insert(f.name);

    for (auto& [caller, callees] : rawCallGraph_) {
        for (auto& callee : callees) {
            CallEdge e;
            e.caller      = caller;
            e.callee      = callee;
            e.isRecursive = (caller == callee);
            e.isExternal  = (definedFns.find(callee) == definedFns.end());
            callEdges_.push_back(e);
        }
    }
}

// ─────────────────────────────────────────────────────────────
//  analyzeFunction()
// ─────────────────────────────────────────────────────────────
static std::string getBBName(llvm::BasicBlock& bb, int idx) {
    if (bb.hasName() && !bb.getName().empty())
        return bb.getName().str();
    return "bb" + std::to_string(idx);
}

static std::string instrToStr(llvm::Instruction& inst) {
    std::string s;
    llvm::raw_string_ostream os(s);
    inst.print(os);
    // trim leading whitespace
    size_t p = s.find_first_not_of(" \t");
    if (p != std::string::npos) s = s.substr(p);
    // truncate long lines
    if (s.size() > 60) s = s.substr(0, 60) + "…";
    return s;
}

static std::string terminatorKind(llvm::BasicBlock& bb) {
    auto* term = bb.getTerminator();
    if (!term) return "none";
    if (llvm::isa<llvm::ReturnInst>(term))      return "ret";
    if (llvm::isa<llvm::UnreachableInst>(term))  return "unreachable";
    if (auto* br = llvm::dyn_cast<llvm::BranchInst>(term))
        return br->isConditional() ? "cond_br" : "br";
    return "other";
}

void CFGAnalyzer::analyzeFunction(llvm::Function& fn) {
    FunctionCFG cfg;
    cfg.name = fn.getName().str();

    // Build return-type string
    {
        std::string s;
        llvm::raw_string_ostream os(s);
        fn.getReturnType()->print(os);
        cfg.returnTypeStr = s;
    }

    // ── Name every basic block ────────────────────────────────
    std::unordered_map<llvm::BasicBlock*, std::string> bbNames;
    {
        int idx = 0;
        for (auto& bb : fn)
            bbNames[&bb] = getBBName(bb, idx++);
    }

    // ── Build adjacency list (for DFS later) ─────────────────
    std::unordered_map<std::string, std::vector<std::string>> adj;

    // ── Predecessor tracking ─────────────────────────────────
    std::unordered_map<std::string, std::vector<std::string>> predMap;
    for (auto& bb : fn) {
        for (auto* succ : llvm::successors(&bb))
            predMap[bbNames[succ]].push_back(bbNames[&bb]);
    }

    // ── Populate blocks ───────────────────────────────────────
    for (auto& bb : fn) {
        CFGBlock block;
        block.name           = bbNames[&bb];
        block.isEntry        = (&bb == &fn.getEntryBlock());
        block.instrCount     = (int)bb.size();
        block.terminatorKind = terminatorKind(bb);
        block.predecessors   = predMap.count(block.name)
                               ? predMap[block.name]
                               : std::vector<std::string>{};

        if (auto* t = bb.getTerminator())
            block.isExit = llvm::isa<llvm::ReturnInst>(t) ||
                           llvm::isa<llvm::UnreachableInst>(t);

        // IR preview (up to 5 instructions)
        int shown = 0;
        for (auto& inst : bb) {
            if (shown >= 5) { block.instrPreview.push_back("…"); break; }
            block.instrPreview.push_back(instrToStr(inst));
            shown++;
        }

        // Successors
        for (auto* succ : llvm::successors(&bb)) {
            std::string sn = bbNames.count(succ) ? bbNames[succ] : "?";
            block.successors.push_back(sn);
            cfg.edges.push_back({block.name, sn});
            adj[block.name].push_back(sn);
        }

        // Track calls for the call graph (skip C runtime functions)
        static const std::set<std::string> skipFns = {
            "printf","scanf","malloc","free","memcpy","memset","puts"
        };
        for (auto& inst : bb) {
            if (auto* call = llvm::dyn_cast<llvm::CallInst>(&inst)) {
                if (auto* callee = call->getCalledFunction()) {
                    std::string cname = callee->getName().str();
                    if (!skipFns.count(cname))
                        rawCallGraph_[cfg.name].insert(cname);
                }
            }
        }

        cfg.blocks.push_back(std::move(block));
    }

    cfg.numNodes = (int)cfg.blocks.size();
    cfg.numEdges = (int)cfg.edges.size();

    detectBackEdges(cfg);
    detectDeadBlocks(cfg);

    cfg.loopCount            = (int)cfg.backEdges.size();
    cfg.cyclomaticComplexity = cfg.numEdges - cfg.numNodes + 2;

    functions_.push_back(std::move(cfg));
}

// ─────────────────────────────────────────────────────────────
//  Back-edge detection via DFS coloring
//  Color: 0 = white (unvisited), 1 = gray (on stack), 2 = black
// ─────────────────────────────────────────────────────────────
void CFGAnalyzer::dfsVisit(
    const std::string& node,
    std::unordered_map<std::string, int>& color,
    const std::unordered_map<std::string, std::vector<std::string>>& adj,
    FunctionCFG& cfg)
{
    color[node] = 1;
    auto it = adj.find(node);
    if (it != adj.end()) {
        for (auto& succ : it->second) {
            if (color.find(succ) == color.end()) color[succ] = 0;
            if (color[succ] == 0) {
                dfsVisit(succ, color, adj, cfg);
            } else if (color[succ] == 1) {
                // Back edge: node → succ
                cfg.backEdges.insert({node, succ});
                // Mark succ as a loop header
                for (auto& blk : cfg.blocks)
                    if (blk.name == succ) {
                        blk.isLoopHeader = true;
                        break;
                    }
            }
        }
    }
    color[node] = 2;
}

void CFGAnalyzer::detectBackEdges(FunctionCFG& cfg) {
    if (cfg.blocks.empty()) return;

    std::unordered_map<std::string, std::vector<std::string>> adj;
    for (auto& e : cfg.edges)
        adj[e.first].push_back(e.second);

    std::unordered_map<std::string, int> color;
    for (auto& b : cfg.blocks) color[b.name] = 0;

    // Start from the entry block
    dfsVisit(cfg.blocks[0].name, color, adj, cfg);
}

// ─────────────────────────────────────────────────────────────
//  Dead-block detection — no reachable predecessor (except entry)
// ─────────────────────────────────────────────────────────────
void CFGAnalyzer::detectDeadBlocks(FunctionCFG& cfg) {
    // Blocks reachable from entry via BFS
    std::set<std::string> reachable;
    if (cfg.blocks.empty()) return;

    std::unordered_map<std::string, std::vector<std::string>> adj;
    for (auto& e : cfg.edges) adj[e.first].push_back(e.second);

    std::vector<std::string> queue = {cfg.blocks[0].name};
    reachable.insert(cfg.blocks[0].name);
    while (!queue.empty()) {
        std::string cur = queue.back(); queue.pop_back();
        for (auto& s : adj[cur]) {
            if (!reachable.count(s)) {
                reachable.insert(s);
                queue.push_back(s);
            }
        }
    }

    cfg.deadBlocks = 0;
    for (auto& blk : cfg.blocks) {
        if (!reachable.count(blk.name) && !blk.isEntry) {
            blk.isDeadCode = true;
            cfg.deadBlocks++;
        }
    }
}

bool CFGAnalyzer::hasDeadCode() const {
    for (auto& fn : functions_)
        if (fn.deadBlocks > 0) return true;
    return false;
}

// ─────────────────────────────────────────────────────────────
//  DOT helpers
// ─────────────────────────────────────────────────────────────
std::string CFGAnalyzer::sanitize(const std::string& s) const {
    std::string r;
    for (char c : s)
        r += (isalnum((unsigned char)c) || c == '_') ? c : '_';
    if (!r.empty() && isdigit((unsigned char)r[0])) r = "_" + r;
    return r;
}

std::string CFGAnalyzer::escapeDot(const std::string& s) const {
    std::string r;
    for (char c : s) {
        if      (c == '"')  r += "\\\"";
        else if (c == '\\') r += "\\\\";
        else if (c == '\n') r += "\\l";
        else if (c == '<')  r += "\\<";
        else if (c == '>')  r += "\\>";
        else if (c == '{')  r += "\\{";
        else if (c == '}')  r += "\\}";
        else if (c == '|')  r += "\\|";
        else                r += c;
    }
    return r;
}

std::string CFGAnalyzer::nodeId(const FunctionCFG& fn,
                                 const std::string& block) const {
    return sanitize(fn.name) + "__" + sanitize(block);
}

std::string CFGAnalyzer::complexityColor(int M) const {
    if (M <= 5)  return "#c8f0c8"; // green
    if (M <= 10) return "#f0f0a0"; // yellow
    if (M <= 20) return "#f0c880"; // orange
    return "#f08080";              // red
}

std::string CFGAnalyzer::riskLabel(int M) const {
    if (M <= 5)  return "LOW";
    if (M <= 10) return "MODERATE";
    if (M <= 20) return "HIGH";
    return "VERY HIGH";
}

// ─────────────────────────────────────────────────────────────
//  generateFunctionDOT()
// ─────────────────────────────────────────────────────────────
std::string CFGAnalyzer::generateFunctionDOT(const FunctionCFG& fn,
                                              bool showInstr) const {
    std::ostringstream dot;
    dot << "digraph CFG_" << sanitize(fn.name) << " {\n";
    dot << "  graph [\n"
        << "    label=\"CFG: " << fn.name << "()  |  "
        << "Nodes=" << fn.numNodes << "  "
        << "Edges=" << fn.numEdges << "  "
        << "M=" << fn.cyclomaticComplexity << " (" << riskLabel(fn.cyclomaticComplexity) << ")  "
        << "Loops=" << fn.loopCount
        << (fn.deadBlocks > 0 ? "  \\u26a0 Dead=" + std::to_string(fn.deadBlocks) : "")
        << "\"\n"
        << "    fontsize=14, labelloc=t, bgcolor=\"#f6f8fa\"\n"
        << "    rankdir=TB\n"
        << "  ];\n"
        << "  node [fontname=\"Courier New\", fontsize=10];\n"
        << "  edge [fontname=\"Arial\", fontsize=9];\n\n";

    // ── Nodes ─────────────────────────────────────────────────
    for (auto& blk : fn.blocks) {
        std::string id = nodeId(fn, blk.name);

        // Pick fill/border colors
        std::string fill, border, fontcol = "#000000";
        if (blk.isDeadCode) {
            fill = "#d0d0d0"; border = "#888888"; fontcol = "#666666";
        } else if (blk.isEntry && blk.isExit) {
            fill = "#b0ffb0"; border = "#007700";
        } else if (blk.isEntry) {
            fill = "#b8f0b8"; border = "#007700";
        } else if (blk.isExit) {
            fill = "#ffb8b8"; border = "#cc0000";
        } else if (blk.isLoopHeader) {
            fill = "#ffe0a0"; border = "#cc7700";
        } else {
            fill = "#ddeeff"; border = "#336699";
        }

        // Build label
        std::string label = blk.name;
        if (blk.isEntry)      label += " \\u25b6 ENTRY";
        if (blk.isExit)       label += " \\u25a0 EXIT";
        if (blk.isLoopHeader) label += " \\u21ba LOOP";
        if (blk.isDeadCode)   label += " \\u26a0 DEAD";
        label += "  [" + blk.terminatorKind + "]";
        label += "\\n(" + std::to_string(blk.instrCount) + " instructions)";

        if (showInstr) {
            label += "\\l";
            for (auto& line : blk.instrPreview)
                label += "  " + escapeDot(line) + "\\l";
        }

        dot << "  " << id << " [\n"
            << "    label=\"" << label << "\"\n"
            << "    shape=box\n"
            << "    style=\"filled,rounded\"\n"
            << "    fillcolor=\"" << fill << "\"\n"
            << "    color=\"" << border << "\"\n"
            << "    fontcolor=\"" << fontcol << "\"\n"
            << "    penwidth=" << (blk.isEntry || blk.isExit ? "2.0" : "1.2") << "\n"
            << "  ];\n";
    }
    dot << "\n";

    // ── Edges ─────────────────────────────────────────────────
    for (auto& [from, to] : fn.edges) {
        bool isBack = fn.backEdges.count({from, to}) > 0;
        dot << "  " << nodeId(fn, from) << " -> " << nodeId(fn, to);
        if (isBack) {
            dot << " [color=\"#cc2222\", style=dashed, label=\"back-edge\","
                << " constraint=false, penwidth=2.0, fontsize=8]";
        } else {
            dot << " [color=\"#2255aa\", penwidth=1.2]";
        }
        dot << ";\n";
    }

    // ── Legend ────────────────────────────────────────────────
    dot << "\n  // Legend\n"
        << "  legend [shape=note, label=\""
        << "LEGEND\\l"
        << "  \\u25b6 Entry block\\l"
        << "  \\u25a0 Exit block (return)\\l"
        << "  \\u21ba Loop header\\l"
        << "  \\u26a0 Dead code\\l"
        << "  Red dashed = back-edge (loop)\\l"
        << "\", fillcolor=\"#ffffcc\", style=filled, fontsize=9];\n";

    dot << "}\n";
    return dot.str();
}

// ─────────────────────────────────────────────────────────────
//  generateAllFunctionsDOT()  — cluster view
// ─────────────────────────────────────────────────────────────
std::string CFGAnalyzer::generateAllFunctionsDOT(bool showInstr) const {
    std::ostringstream dot;
    dot << "digraph AllCFGs {\n";
    dot << "  graph [label=\"Control Flow Graphs — All Functions\","
        << " fontsize=18, labelloc=t, compound=true, bgcolor=\"#f0f0f0\","
        << " rankdir=TB];\n"
        << "  node  [fontname=\"Courier New\", fontsize=9];\n"
        << "  edge  [fontname=\"Arial\", fontsize=8];\n\n";

    for (auto& fn : functions_) {
        std::string clr = complexityColor(fn.cyclomaticComplexity);
        dot << "  subgraph cluster_" << sanitize(fn.name) << " {\n"
            << "    label=\"" << fn.name << "()  M=" << fn.cyclomaticComplexity
            << " · " << riskLabel(fn.cyclomaticComplexity) << "\";\n"
            << "    style=filled; fillcolor=\"" << clr << "\"; penwidth=2;\n"
            << "    fontsize=11;\n\n";

        for (auto& blk : fn.blocks) {
            std::string id = nodeId(fn, blk.name);
            std::string fill = blk.isDeadCode ? "#d0d0d0"
                             : blk.isEntry    ? "#b8f0b8"
                             : blk.isExit     ? "#ffb8b8"
                             : blk.isLoopHeader ? "#ffe0a0"
                             : "#ddeeff";
            std::string label = blk.name;
            if (blk.isEntry)      label += " ENTRY";
            if (blk.isExit)       label += " EXIT";
            if (blk.isLoopHeader) label += " LOOP";
            if (blk.isDeadCode)   label += " DEAD";
            label += "\\n" + std::to_string(blk.instrCount) + "i";
            dot << "    " << id
                << " [label=\"" << label << "\","
                << " shape=box, style=\"filled,rounded\","
                << " fillcolor=\"" << fill << "\"];\n";
        }

        for (auto& [from, to] : fn.edges) {
            bool isBack = fn.backEdges.count({from, to}) > 0;
            dot << "    " << nodeId(fn, from) << " -> " << nodeId(fn, to);
            if (isBack) dot << " [color=red, style=dashed, constraint=false]";
            dot << ";\n";
        }
        dot << "  }\n\n";
    }
    dot << "}\n";
    return dot.str();
}

// ─────────────────────────────────────────────────────────────
//  generateCallGraphDOT()
// ─────────────────────────────────────────────────────────────
std::string CFGAnalyzer::generateCallGraphDOT() const {
    std::ostringstream dot;
    dot << "digraph CallGraph {\n";
    dot << "  graph [label=\"Call Graph\", fontsize=18, labelloc=t,"
        << " bgcolor=\"#f8f0ff\", rankdir=LR];\n"
        << "  node  [fontname=\"Arial\", fontsize=11, shape=box,"
        << " style=\"filled,rounded\"];\n"
        << "  edge  [fontname=\"Arial\", fontsize=9, color=\"#553399\","
        << " arrowsize=0.8];\n\n";

    // Collect all names
    std::set<std::string> defined, allNodes;
    for (auto& f : functions_) { defined.insert(f.name); allNodes.insert(f.name); }
    for (auto& e : callEdges_) allNodes.insert(e.callee);

    // Nodes
    for (auto& nm : allNodes) {
        bool def = defined.count(nm) > 0;
        int  M   = 0;
        for (auto& f : functions_)
            if (f.name == nm) { M = f.cyclomaticComplexity; break; }
        std::string fill = def ? complexityColor(M) : "#ffe0e0";
        std::string shape = def ? "box" : "ellipse";
        std::string label = nm;
        if (def) label += "\\nM=" + std::to_string(M);
        else     label += "\\n[external]";
        dot << "  " << sanitize(nm)
            << " [label=\"" << label << "\","
            << " fillcolor=\"" << fill << "\","
            << " shape=" << shape << "];\n";
    }
    dot << "\n";

    // Edges
    for (auto& e : callEdges_) {
        dot << "  " << sanitize(e.caller) << " -> " << sanitize(e.callee);
        if (e.isRecursive)
            dot << " [color=red, label=\"recursive\", style=dashed, penwidth=2]";
        else if (e.isExternal)
            dot << " [color=\"#999999\", style=dotted]";
        dot << ";\n";
    }

    // Legend
    dot << "\n  legend [shape=note, style=filled, fillcolor=\"#ffffcc\","
        << " fontsize=9, label=\""
        << "LEGEND\\l"
        << "  Green = low complexity (M≤5)\\l"
        << "  Yellow = moderate (M≤10)\\l"
        << "  Orange = high (M≤20)\\l"
        << "  Red = very high (M>20)\\l"
        << "  Red box = external function\\l"
        << "\"];\n";

    dot << "}\n";
    return dot.str();
}

// ─────────────────────────────────────────────────────────────
//  File helpers
// ─────────────────────────────────────────────────────────────
bool CFGAnalyzer::saveDOT(const std::string& path,
                           const std::string& content) const {
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << content;
    return true;
}

bool CFGAnalyzer::render(const std::string& dotPath,
                          const std::string& outPath,
                          const std::string& fmt) {
    std::string cmd = "dot -T" + fmt + " \"" + dotPath +
                      "\" -o \"" + outPath + "\" 2>/dev/null";
    return std::system(cmd.c_str()) == 0;
}

// ─────────────────────────────────────────────────────────────
//  Console reports
// ─────────────────────────────────────────────────────────────
void CFGAnalyzer::printComplexityReport() const {
    std::cout << "\n" << B << MG
              << "╔══════════════════════════════════════════════════════════╗\n"
              << "║           CYCLOMATIC COMPLEXITY  REPORT                 ║\n"
              << "╚══════════════════════════════════════════════════════════╝\n"
              << R;
    std::cout << B
              << std::left << std::setw(32) << "Function"
              << std::setw(6)  << "M"
              << std::setw(8)  << "Nodes"
              << std::setw(8)  << "Edges"
              << std::setw(8)  << "Loops"
              << std::setw(8)  << "Dead"
              << "Risk\n" << R
              << std::string(70, '-') << "\n";

    for (auto& fn : functions_) {
        int M = fn.cyclomaticComplexity;
        const char* col = (M <= 5) ? GR : (M <= 10) ? YL : RD;
        const char* risk = riskLabel(M).c_str();
        std::cout << std::left << std::setw(32) << fn.name
                  << col << B << std::setw(6) << M << R
                  << DM << std::setw(8) << fn.numNodes
                  << std::setw(8) << fn.numEdges << R
                  << std::setw(8) << fn.loopCount
                  << (fn.deadBlocks > 0 ? std::string(RD) : std::string(DM))
                  << std::setw(8) << fn.deadBlocks << R
                  << col << risk << R << "\n";
    }
    std::cout << std::string(70, '-') << "\n"
              << DM << "  M = Cyclomatic Complexity (E − N + 2P)\n" << R
              << "  LOW ≤5 | MODERATE ≤10 | HIGH ≤20 | VERY HIGH >20\n\n";
}

void CFGAnalyzer::printCFGSummary() const {
    std::cout << "\n" << B << CY
              << "── CFG Summary ──────────────────────────────────────────\n"
              << R;
    int totalBlocks = 0, totalEdges = 0, totalLoops = 0, totalDead = 0;
    for (auto& fn : functions_) {
        totalBlocks += fn.numNodes;
        totalEdges  += fn.numEdges;
        totalLoops  += fn.loopCount;
        totalDead   += fn.deadBlocks;
    }
    std::cout << "  Functions analysed : " << B << functions_.size() << R << "\n"
              << "  Total basic blocks : " << B << totalBlocks << R << "\n"
              << "  Total CFG edges    : " << B << totalEdges  << R << "\n"
              << "  Back-edges (loops) : " << B << totalLoops  << R << "\n"
              << "  Dead basic blocks  : "
              << (totalDead > 0 ? std::string(RD) : std::string(GR))
              << B << totalDead << R << "\n"
              << "  Call edges         : " << B << callEdges_.size() << R << "\n\n";
}

void CFGAnalyzer::printDeadCodeReport() const {
    bool any = false;
    for (auto& fn : functions_)
        if (fn.deadBlocks > 0) { any = true; break; }

    if (!any) {
        std::cout << GR << "  No unreachable basic blocks detected.\n" << R;
        return;
    }

    std::cout << "\n" << B << RD
              << "╔══════════════════════════════════════════════════════════╗\n"
              << "║               DEAD CODE WARNING                         ║\n"
              << "╚══════════════════════════════════════════════════════════╝\n"
              << R;
    for (auto& fn : functions_) {
        for (auto& blk : fn.blocks) {
            if (blk.isDeadCode) {
                std::cout << RD << "  [DEAD] " << R
                          << fn.name << "() → block '" << blk.name << "'"
                          << " (" << blk.instrCount << " instrs)\n";
            }
        }
    }
    std::cout << "\n";
}