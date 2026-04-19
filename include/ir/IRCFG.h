#pragma once
// ============================================================
//  IRBasicBlock.h + IRCFG.h  —  Quail Compiler IR Layer
//
//  A BasicBlock is a maximal straight-line sequence of
//  instructions with a single entry (label) and single exit
//  (terminator: jump/branch/return).
//
//  The CFG (Control Flow Graph) wraps a function's blocks
//  and the edges between them, supporting:
//    • Predecessor / successor queries
//    • Dominator tree computation (for SSA construction)
//    • Loop detection (back-edge identification)
//    • Dead block elimination
//    • DOT serialisation for visualisation
// ============================================================
#include "IRInstruction.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <memory>
#include <algorithm>
#include <sstream>
#include <iostream>

// ─────────────────────────────────────────────────────────────
//  BasicBlock
// ─────────────────────────────────────────────────────────────
struct BasicBlock {
    std::string               label;
    std::vector<IRInstruction> instrs;

    // CFG edges (labels of target blocks)
    std::vector<std::string>  successors;
    std::vector<std::string>  predecessors;

    // Analysis annotations
    bool isEntry    = false;
    bool isExit     = false;
    bool isReachable= true;
    bool isLoopHead = false;
    int  loopDepth  = 0;
    int  id         = 0;
    bool isDead     = false; 

    // Liveness / SSA state
    std::unordered_set<std::string> liveIn;
    std::unordered_set<std::string> liveOut;
    std::unordered_set<std::string> gen;  // upward-exposed uses
    std::unordered_set<std::string> kill; // definitions

    // Dominator info
    int  idomId     = -1; // immediate dominator block id
    std::vector<int> domChildren;
    std::unordered_set<int> domFrontier;

    explicit BasicBlock(const std::string& lbl, int bid = 0)
        : label(lbl), id(bid) {}

    // Append an instruction (before any terminator)
    void push(const IRInstruction& ins) { instrs.push_back(ins); }

    // Does this block end with a terminator?
    bool isTerminated() const {
        return !instrs.empty() && instrs.back().isTerminator();
    }

    // Returns the terminator (or nullptr if absent)
    const IRInstruction* terminator() const {
        if (!instrs.empty() && instrs.back().isTerminator())
            return &instrs.back();
        return nullptr;
    }
    IRInstruction* terminator() {
        if (!instrs.empty() && instrs.back().isTerminator())
            return &instrs.back();
        return nullptr;
    }

    // Count of non-NOP, non-COMMENT instructions
    int realInstrCount() const {
        int c = 0;
        for (auto& i : instrs)
            if (i.op != IROp::NOP && i.op != IROp::COMMENT) c++;
        return c;
    }

    // Collect all temporaries/variables defined in this block
    std::unordered_set<std::string> definitions() const {
        std::unordered_set<std::string> defs;
        for (auto& i : instrs)
            if (i.definesValue() && i.result.isTempOrVar())
                defs.insert(i.result.name);
        return defs;
    }

    // Collect all values used in this block (before any def)
    void computeGenKill() {
        gen.clear(); kill.clear();
        for (auto& ins : instrs) {
            // Uses
            for (auto* v : {&ins.arg1, &ins.arg2}) {
                if (v->isTempOrVar() && !kill.count(v->name))
                    gen.insert(v->name);
            }
            for (auto& ca : ins.callArgs)
                if (ca.isTempOrVar() && !kill.count(ca.name))
                    gen.insert(ca.name);
            // Definitions
            if (ins.definesValue() && ins.result.isTempOrVar())
                kill.insert(ins.result.name);
        }
    }

    std::string toString() const {
        std::ostringstream oss;
        oss << label << ":\n";
        for (auto& i : instrs) oss << i.toString() << "\n";
        return oss.str();
    }
};

// ─────────────────────────────────────────────────────────────
//  IRFunction  —  a function's CFG
// ─────────────────────────────────────────────────────────────
struct IRFunction {
    std::string name;
    std::string returnType; // "int" / "float" / "void"
    std::vector<std::pair<std::string,std::string>> params; // (type, name)

    // Block storage (owned)
    std::vector<std::unique_ptr<BasicBlock>> blocks;
    // Fast name→block lookup
    std::unordered_map<std::string, BasicBlock*> blockMap;

    // ── Block management ──────────────────────────────────────
    BasicBlock* addBlock(const std::string& label) {
        auto bb = std::make_unique<BasicBlock>(label, (int)blocks.size());
        BasicBlock* ptr = bb.get();
        blockMap[label] = ptr;
        blocks.push_back(std::move(bb));
        return ptr;
    }

    BasicBlock* getBlock(const std::string& label) {
        auto it = blockMap.find(label);
        return it != blockMap.end() ? it->second : nullptr;
    }
    const BasicBlock* getBlock(const std::string& label) const {
        auto it = blockMap.find(label);
        return it != blockMap.end() ? it->second : nullptr;
    }

    BasicBlock* entry() {
        return blocks.empty() ? nullptr : blocks.front().get();
    }
    const BasicBlock* entry() const {
        return blocks.empty() ? nullptr : blocks.front().get();
    }

    // ── Edge management ───────────────────────────────────────
    void addEdge(const std::string& from, const std::string& to) {
        BasicBlock* fb = getBlock(from);
        BasicBlock* tb = getBlock(to);
        if (!fb || !tb) return;
        if (std::find(fb->successors.begin(), fb->successors.end(), to)
                == fb->successors.end())
            fb->successors.push_back(to);
        if (std::find(tb->predecessors.begin(), tb->predecessors.end(), from)
                == tb->predecessors.end())
            tb->predecessors.push_back(from);
    }

    // Rebuild successor/predecessor lists from terminators
    void rebuildEdges() {
        for (auto& bb : blocks) {
            bb->successors.clear();
            bb->predecessors.clear();
        }
        for (size_t i = 0; i < blocks.size(); ++i) {
            BasicBlock* bb = blocks[i].get();
            auto* term = bb->terminator();
            if (term) {
                if (term->op == IROp::JUMP)
                    addEdge(bb->label, term->result.name);
                else if (term->op == IROp::CJUMP) {
                    addEdge(bb->label, term->result.name);
                    addEdge(bb->label, term->arg2.name);
                } else if (term->op == IROp::CJUMP_TRUE ||
                        term->op == IROp::CJUMP_FALSE)
                    addEdge(bb->label, term->result.name);
                // RETURN / RETURN_VOID: no outgoing edges (correct)
            } else if (i + 1 < blocks.size()) {
                // No terminator = peephole removed a redundant jump.
                // The block implicitly falls through to the next one.
                addEdge(bb->label, blocks[i + 1]->label);
            }
        }
    }

    // ── Reachability ─────────────────────────────────────────
    void markReachable() {
        for (auto& b : blocks) b->isReachable = false;
        if (blocks.empty()) return;
        std::vector<std::string> worklist = {blocks.front()->label};
        while (!worklist.empty()) {
            std::string cur = worklist.back(); worklist.pop_back();
            BasicBlock* b = getBlock(cur);
            if (!b || b->isReachable) continue;
            b->isReachable = true;
            for (auto& s : b->successors) worklist.push_back(s);
        }
    }

    // ── Dominator computation (iterative algorithm) ───────────
    // Returns idom[i] = id of immediate dominator of block i,
    // -1 for the entry block.
    void computeDominators() {
        int n = (int)blocks.size();
        if (n == 0) return;

        std::unordered_map<std::string,int> idx;
        for (int i = 0; i < n; i++) idx[blocks[i]->label] = i;

        std::vector<int> idom(n, -1);
        idom[0] = -1;

        // FIX: guard against -1 idom values entering the intersect walk
        auto intersect = [&](int b1, int b2) -> int {
            while (b1 != b2) {
                while (b1 > b2) {
                    if (idom[b1] == -1) return b2; // unprocessed — bail out
                    b1 = idom[b1];
                }
                while (b2 > b1) {
                    if (idom[b2] == -1) return b1; // unprocessed — bail out
                    b2 = idom[b2];
                }
            }
            return b1;
        };

        bool changed = true;
        while (changed) {
            changed = false;
            for (int i = 1; i < n; i++) {
                BasicBlock* bb = blocks[i].get();
                if (!bb->isReachable) continue;
                int newIdom = -1;
                for (auto& p : bb->predecessors) {
                    auto it = idx.find(p);
                    if (it == idx.end()) continue;
                    int pi = it->second;
                    if (idom[pi] == -1) continue;
                    newIdom = (newIdom == -1) ? pi : intersect(newIdom, pi);
                }
                if (newIdom != -1 && newIdom != idom[i]) {
                    idom[i] = newIdom;
                    changed = true;
                }
            }
        }

        for (int i = 0; i < n; i++)
            blocks[i]->idomId = idom[i];
    }
    // ── Liveness analysis (backward dataflow) ─────────────────
    void computeLiveness() {
        for (auto& bb : blocks) bb->computeGenKill();
        bool changed = true;
        while (changed) {
            changed = false;
            for (int i = (int)blocks.size()-1; i >= 0; i--) {
                BasicBlock* bb = blocks[i].get();
                // liveOut[B] = union of liveIn[S] for each successor S
                std::unordered_set<std::string> newOut;
                for (auto& s : bb->successors) {
                    BasicBlock* sb = getBlock(s);
                    if (sb) for (auto& v : sb->liveIn) newOut.insert(v);
                }
                // liveIn[B] = gen[B] ∪ (liveOut[B] \ kill[B])
                std::unordered_set<std::string> newIn = bb->gen;
                for (auto& v : newOut)
                    if (!bb->kill.count(v)) newIn.insert(v);

                if (newOut != bb->liveOut || newIn != bb->liveIn) {
                    bb->liveOut = std::move(newOut);
                    bb->liveIn  = std::move(newIn);
                    changed = true;
                }
            }
        }
    }

    // ── Back-edge / loop detection (DFS coloring) ─────────────
    struct LoopInfo {
        std::string headerLabel;
        std::set<std::string> body; // all blocks in loop
    };
    std::vector<LoopInfo> loops;

    void detectLoops() {
        loops.clear();
        if (blocks.empty()) return;
        for (auto& b : blocks) b->loopDepth = 0;
        std::unordered_map<std::string,int> color; // 0=white,1=gray,2=black
        std::function<void(const std::string&)> dfs;
        dfs = [&](const std::string& lbl) {
            color[lbl] = 1;
            BasicBlock* bb = getBlock(lbl);
            if (!bb) { color[lbl] = 2; return; }
            for (auto& s : bb->successors) {
                if (!color.count(s)) dfs(s);
                else if (color[s] == 1) {
                    // back-edge lbl → s: s is a loop header
                    BasicBlock* hdr = getBlock(s);
                    if (hdr) hdr->isLoopHead = true;
                    // collect loop body via reverse DFS from lbl to s
                    LoopInfo li;
                    li.headerLabel = s;
                    std::vector<std::string> stack = {lbl};
                    while (!stack.empty()) {
                        std::string cur = stack.back(); stack.pop_back();
                        if (li.body.count(cur)) continue;
                        li.body.insert(cur);
                        BasicBlock* cb = getBlock(cur);
                        if (!cb) continue;
                        for (auto& p : cb->predecessors)
                            if (!li.body.count(p)) stack.push_back(p);
                    }
                    li.body.insert(s);
                    loops.push_back(std::move(li));
                }
            }
            color[lbl] = 2;
        };
        dfs(blocks.front()->label);

        // Set loop depth
        for (auto& li : loops)
            for (auto& lbl : li.body)
                if (BasicBlock* b = getBlock(lbl)) b->loopDepth++;
    }

    // ── Instruction iteration ─────────────────────────────────
    void forEachInstruction(std::function<void(BasicBlock&, IRInstruction&)> fn) {
        for (auto& bb : blocks)
            for (auto& ins : bb->instrs)
                fn(*bb, ins);
    }
    void forEachInstruction(std::function<void(const BasicBlock&,
                                               const IRInstruction&)> fn) const {
        for (auto& bb : blocks)
            for (auto& ins : bb->instrs)
                fn(*bb, ins);
    }

    // ── Pretty print ──────────────────────────────────────────
    std::string toString() const {
        std::ostringstream oss;
        oss << "function " << returnType << " " << name << "(";
        for (size_t i = 0; i < params.size(); i++) {
            if (i) oss << ", ";
            oss << params[i].first << " " << params[i].second;
        }
        oss << ") {\n";
        for (auto& bb : blocks) oss << bb->toString();
        oss << "}\n";
        return oss.str();
    }

    // ── DOT for CFG visualisation ─────────────────────────────
    std::string toDOT() const {
        std::ostringstream dot;
        std::string fn = name;
        dot << "digraph CFG_" << fn << " {\n";
        dot << "  graph [label=\"CFG: " << fn << "()\", fontsize=14, labelloc=t];\n";
        dot << "  node [shape=box, fontname=\"Courier\", fontsize=10];\n\n";

        for (auto& bb : blocks) {
            std::string fill = "#ddeeff";
            if (bb->isEntry)    fill = "#b8f0b8";
            else if (bb->isExit) fill = "#ffb8b8";
            else if (bb->isLoopHead) fill = "#ffe0a0";
            else if (!bb->isReachable) fill = "#d0d0d0";

            std::string label = bb->label;
            for (auto& ins : bb->instrs) {
                std::string s = ins.toString();
                // escape for DOT
                for (size_t i = 0; i < s.size(); i++) {
                    if (s[i] == '"') s.replace(i,1,"\\\""), i++;
                    else if (s[i] == '<') s.replace(i,1,"\\<"), i++;
                    else if (s[i] == '>') s.replace(i,1,"\\>"), i++;
                    else if (s[i] == '{') s.replace(i,1,"\\{"), i++;
                    else if (s[i] == '}') s.replace(i,1,"\\}"), i++;
                    else if (s[i] == '|') s.replace(i,1,"\\|"), i++;
                }
                label += "\\l" + s;
            }
            dot << "  " << bb->label
                << " [label=\"" << label << "\\l\""
                << ", style=filled, fillcolor=\"" << fill << "\""
                << "];\n";
        }
        dot << "\n";

        for (auto& bb : blocks)
            for (auto& s : bb->successors)
                dot << "  " << bb->label << " -> " << s << ";\n";

        dot << "}\n";
        return dot.str();
    }

    int totalInstructions() const {
        int c = 0;
        for (auto& b : blocks) c += (int)b->instrs.size();
        return c;
    }
};

// ─────────────────────────────────────────────────────────────
//  IRModule  —  collection of functions
// ─────────────────────────────────────────────────────────────
struct IRModule {
    std::string name;
    std::vector<std::unique_ptr<IRFunction>> functions;
    // Global variable declarations: (type, name, initialValue)
    std::vector<std::tuple<std::string,std::string,IRValue>> globals;

    IRFunction* addFunction(const std::string& n) {
        auto fn = std::make_unique<IRFunction>();
        fn->name = n;
        IRFunction* ptr = fn.get();
        functions.push_back(std::move(fn));
        return ptr;
    }

    IRFunction* getFunction(const std::string& n) {
        for (auto& f : functions) if (f->name == n) return f.get();
        return nullptr;
    }

    std::string toString() const {
        std::ostringstream oss;
        oss << "; Module: " << name << "\n\n";
        for (auto& [t,n,v] : globals)
            oss << "global " << t << " " << n
                << " = " << v.toString() << "\n";
        if (!globals.empty()) oss << "\n";
        for (auto& fn : functions) oss << fn->toString() << "\n";
        return oss.str();
    }

    int totalInstructions() const {
        int c = 0;
        for (auto& f : functions) c += f->totalInstructions();
        return c;
    }
};
