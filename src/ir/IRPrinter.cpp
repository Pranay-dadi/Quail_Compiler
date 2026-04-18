#include "ir/IRPrinter.h"
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <unordered_set>

// ── ANSI ──────────────────────────────────────────────────────
static const char* PB = "\033[1m";
static const char* PG = "\033[1;32m";
static const char* PR = "\033[1;31m";
static const char* PY = "\033[1;33m";
static const char* PC = "\033[1;36m";
static const char* PM = "\033[1;35m";
static const char* PD = "\033[2m";
static const char* PS = "\033[0m";

IRPrinter::IRPrinter(std::ostream& os) : out(os) {}

// ─────────────────────────────────────────────────────────────
//  printInstruction
// ─────────────────────────────────────────────────────────────
void IRPrinter::printInstruction(const IRInstruction& ins,
                                  const RegisterAllocator* ra) const {
    if (ins.op == IROp::NOP) return;

    // Color by category
    const char* col =
        ins.isTerminator()                     ? PR :
        (ins.op == IROp::CALL ||
         ins.op == IROp::CALL_VOID)            ? PM :
        (ins.op == IROp::PHI)                  ? PG :
        (ins.op == IROp::LOAD  ||
         ins.op == IROp::STORE ||
         ins.op == IROp::ALLOC ||
         ins.op == IROp::FREE)                 ? PY :
        isBinaryOp(ins.op)                     ? PC : PS;

    out << col << ins.toString() << PS;

    // Register annotation
    if (ra && ins.definesValue() && ins.result.isTemp()) {
        std::string reg = ra->getRegister(ins.result.name);
        if (!reg.empty())
            out << PD << "   ; " << ins.result.name << "→" << reg << PS;
        else if (ra->isSpilled(ins.result.name))
            out << PD << "   ; " << ins.result.name << "→[spill]" << PS;
    }
    out << "\n";
}

// ─────────────────────────────────────────────────────────────
//  printBlock
// ─────────────────────────────────────────────────────────────
void IRPrinter::printBlock(const BasicBlock& bb, bool showLiveness) const {
    if (showLiveness && (!bb.liveIn.empty() || !bb.liveOut.empty())) {
        out << PD << "  ; liveIn:  {";
        bool first = true;
        for (auto& v : bb.liveIn) { if (!first) out << ","; first=false; out << v; }
        out << "}\n  ; liveOut: {";
        first = true;
        for (auto& v : bb.liveOut) { if (!first) out << ","; first=false; out << v; }
        out << "}" << PS << "\n";
    }

    out << PY << bb.label << ":" << PS;
    if (bb.isEntry)    out << PG << "  ENTRY" << PS;
    if (bb.isExit)     out << PR << "  EXIT"  << PS;
    if (bb.isLoopHead) out << PY << "  LOOP(depth=" << bb.loopDepth << ")" << PS;
    if (!bb.isReachable) out << PD << "  UNREACHABLE" << PS;
    out << "\n";

    for (auto& ins : bb.instrs) {
        if (ins.op == IROp::LABEL) continue;
        printInstruction(ins);
    }
}

// ─────────────────────────────────────────────────────────────
//  printFunction
// ─────────────────────────────────────────────────────────────
void IRPrinter::printFunction(const IRFunction& fn, bool showLiveness) const {
    out << "\n" << PB << PM << fn.returnType << " " << fn.name << "(";
    for (size_t i = 0; i < fn.params.size(); i++) {
        if (i) out << ", ";
        out << fn.params[i].first << " " << fn.params[i].second;
    }
    out << ")" << PS << " {\n";

    for (auto& bb : fn.blocks)
        printBlock(*bb, showLiveness);

    out << "}\n";
}

// ─────────────────────────────────────────────────────────────
//  printModule
// ─────────────────────────────────────────────────────────────
void IRPrinter::printModule(const IRModule& m, bool showLiveness) const {
    out << "\n" << PB << PC
        << "╔══════════════════════════════════════════════════════════╗\n"
        << "║          THREE-ADDRESS CODE  (IR Module: " << std::left
        << std::setw(15) << m.name << ")║\n"
        << "╚══════════════════════════════════════════════════════════╝\n"
        << PS;

    if (!m.globals.empty()) {
        out << "\n" << PD << "; globals\n" << PS;
        for (auto& [t,n,v] : m.globals)
            out << PD << "  " << t << " " << n << " = " << v.toString() << "\n" << PS;
    }

    for (auto& fn : m.functions)
        printFunction(*fn, showLiveness);
}

// ─────────────────────────────────────────────────────────────
//  printQuadrupleTable
// ─────────────────────────────────────────────────────────────
void IRPrinter::printQuadrupleTable(const IRFunction& fn) const {
    const int W0=5, W1=14, W2=14, W3=14, W4=14;
    std::string sep(W0+W1+W2+W3+W4+2,'-');

    out << "\n" << PB << PC
        << "── Quadruple Table: " << fn.name << "() ────────────────────\n"
        << PS;
    out << sep << "\n";
    out << PB
        << std::left
        << std::setw(W0) << "#"
        << std::setw(W1) << "OP"
        << std::setw(W2) << "ARG1"
        << std::setw(W3) << "ARG2"
        << std::setw(W4) << "RESULT"
        << PS << "\n" << sep << "\n";

    int idx = 0;
    for (auto& bb : fn.blocks) {
        out << PD << "  [" << bb->label << "]\n" << PS;
        for (auto& ins : bb->instrs) {
            if (ins.op == IROp::NOP || ins.op == IROp::COMMENT) continue;

            std::string a1 = ins.arg1.isUndef()  ? "-" : ins.arg1.toString();
            std::string a2 = ins.arg2.isUndef()  ? "-" : ins.arg2.toString();
            std::string rs = ins.result.isUndef() ? "-" : ins.result.toString();

            if (ins.op == IROp::CALL || ins.op == IROp::CALL_VOID) {
                a2 = "(";
                for (size_t i=0;i<ins.callArgs.size();i++) {
                    if (i) a2+=",";
                    a2+=ins.callArgs[i].toString();
                }
                a2+=")";
            }
            if (ins.op == IROp::PHI) {
                a1="phi("; bool f=true;
                for (auto& [lbl,v]:ins.phiSources) {
                    if (!f) a1+=","; f=false;
                    a1+=v.toString()+"<"+lbl+">";
                }
                a1+=")"; a2="-";
            }

            const char* col =
                ins.isTerminator() ? PR :
                isComparisonOp(ins.op) ? PG :
                (ins.op==IROp::CALL||ins.op==IROp::CALL_VOID) ? PM :
                isBinaryOp(ins.op) ? PC : PS;

            out << PD << std::setw(W0) << idx++ << PS
                << col << std::left << std::setw(W1) << opName(ins.op) << PS
                << std::setw(W2) << a1
                << std::setw(W3) << a2
                << std::setw(W4) << rs << "\n";
        }
    }
    out << sep << "\n";
}

// ─────────────────────────────────────────────────────────────
//  printDAG — reconstruct and print value DAG per block
// ─────────────────────────────────────────────────────────────
void IRPrinter::printDAG(const BasicBlock& bb) const {
    out << "\n" << PB << PC << "── Value DAG: " << bb.label << " ────────\n" << PS;

    // Build a simple DAG representation:
    // Each definition is a node; operands are edges pointing to their defs.
    // Nodes with no uses in this block are roots.
    struct Node {
        std::string name;
        std::string expr;   // human-readable expression
        std::vector<std::string> operands;
    };
    std::unordered_map<std::string, Node> nodes;
    std::unordered_set<std::string> usedAsOp;

    for (auto& ins : bb.instrs) {
        if (!ins.definesValue() || ins.result.isUndef()) continue;
        Node n;
        n.name = ins.result.toString();
        n.expr = opName(ins.op);
        if (!ins.arg1.isUndef()) {
            n.operands.push_back(ins.arg1.toString());
            usedAsOp.insert(ins.arg1.toString());
        }
        if (!ins.arg2.isUndef()) {
            n.operands.push_back(ins.arg2.toString());
            usedAsOp.insert(ins.arg2.toString());
        }
        nodes[n.name] = n;
    }

    // Print roots first (nodes not used as operands — visible results)
    std::function<void(const std::string&, int)> print;
    std::unordered_set<std::string> visited;
    print = [&](const std::string& name, int depth) {
        if (visited.count(name)) return;
        visited.insert(name);
        std::string indent(depth*2, ' ');
        auto it = nodes.find(name);
        if (it == nodes.end()) {
            out << indent << PD << name << " (input)" << PS << "\n";
            return;
        }
        out << indent << PC << name << PS << " = " << PY << it->second.expr << PS << "\n";
        for (auto& op : it->second.operands)
            print(op, depth+1);
    };

    // Print all roots
    for (auto& [name, node] : nodes) {
        if (!usedAsOp.count(name))
            print(name, 0);
    }
    out << "\n";
}

// ─────────────────────────────────────────────────────────────
//  escDot — escape for DOT labels
// ─────────────────────────────────────────────────────────────
std::string IRPrinter::escDot(const std::string& s) const {
    std::string r;
    for (char c : s) {
        if (c=='"')  { r+="\\\""; continue; }
        if (c=='<')  { r+="\\<";  continue; }
        if (c=='>')  { r+="\\>";  continue; }
        if (c=='{')  { r+="\\{";  continue; }
        if (c=='}')  { r+="\\}";  continue; }
        if (c=='|')  { r+="\\|";  continue; }
        if (c=='\n') { r+="\\l";  continue; }
        r+=c;
    }
    return r;
}

// ─────────────────────────────────────────────────────────────
//  dagToDOT — Graphviz DOT for value DAG of a basic block
// ─────────────────────────────────────────────────────────────
std::string IRPrinter::dagToDOT(const BasicBlock& bb) const {
    std::ostringstream dot;
    dot << "digraph DAG_" << bb.label << " {\n";
    dot << "  graph [label=\"Value DAG: " << bb.label
        << "\", fontsize=14, labelloc=t, bgcolor=\"#f8f8f8\"];\n";
    dot << "  node [fontname=Courier, fontsize=10, style=filled];\n\n";

    int nodeId = 0;
    std::unordered_map<std::string, int> idMap;

    // Assign IDs
    for (auto& ins : bb.instrs) {
        if (!ins.definesValue() || ins.result.isUndef()) continue;
        idMap[ins.result.toString()] = nodeId++;
    }

    // Emit nodes
    for (auto& ins : bb.instrs) {
        if (!ins.definesValue() || ins.result.isUndef()) continue;
        std::string name = ins.result.toString();
        int id = idMap[name];
        std::string fill =
            isComparisonOp(ins.op) ? "#c8f0c8" :
            isBinaryOp(ins.op)     ? "#ddeeff" :
            (ins.op==IROp::LOAD||ins.op==IROp::STORE) ? "#ffe0a0" : "#e8e8e8";
        std::string label = escDot(name) + "\\n" + escDot(opName(ins.op));
        dot << "  n" << id << " [label=\"" << label
            << "\", fillcolor=\"" << fill << "\"];\n";
    }

    // Emit edges
    for (auto& ins : bb.instrs) {
        if (!ins.definesValue() || ins.result.isUndef()) continue;
        int dstId = idMap[ins.result.toString()];
        auto emitEdge = [&](const IRValue& v, const std::string& lbl) {
            if (v.isUndef()) return;
            auto it = idMap.find(v.toString());
            if (it != idMap.end())
                dot << "  n" << it->second << " -> n" << dstId
                    << " [label=\"" << lbl << "\"];\n";
        };
        emitEdge(ins.arg1, "l");
        emitEdge(ins.arg2, "r");
    }

    dot << "}\n";
    return dot.str();
}

// ─────────────────────────────────────────────────────────────
//  interferenceGraphToDOT
// ─────────────────────────────────────────────────────────────
std::string IRPrinter::interferenceGraphToDOT(
    const IRFunction& fn, const RegisterAllocator& ra) const
{
    std::ostringstream dot;
    dot << "digraph IG_" << fn.name << " {\n";
    dot << "  graph [label=\"Interference Graph: " << fn.name
        << "()\", fontsize=14, labelloc=t, bgcolor=\"#f8f0ff\"];\n";
    dot << "  node [fontname=Arial, fontsize=11, shape=ellipse, style=filled];\n";
    dot << "  edge [dir=none, color=\"#888888\"];\n\n";

    // Color map: register → color
    static const std::vector<std::string> COLORS = {
        "#b8f0b8","#b8d8f8","#f8e8b8","#f8b8b8","#e8b8f8",
        "#b8f8f0","#f8d8b8","#d8b8f8","#b8f8d8","#f8b8e8"
    };

    auto& assign = ra.getAssignment();
    auto& spills = ra.getSpills();

    // Collect all nodes
    std::unordered_set<std::string> all;
    for (auto& [a, neighbors] : ra.getAssignment()) all.insert(a);
    for (auto& s : spills) all.insert(s);

    // Emit nodes
    std::unordered_map<std::string,int> nodeId;
    int nid = 0;
    for (auto& name : all) {
        nodeId[name] = nid;
        std::string fill = "#d0d0d0"; // spilled
        std::string label = escDot(name);
        auto it = assign.find(name);
        if (it != assign.end()) {
            // Derive color index from register name
            int ridx = 0;
            try { ridx = std::stoi(it->second.substr(1)); } catch (...) {}
            fill = COLORS[ridx % COLORS.size()];
            label += "\\n" + it->second;
        } else if (spills.count(name)) {
            label += "\\n[spill]";
        }
        dot << "  n" << nid++ << " [label=\"" << label
            << "\", fillcolor=\"" << fill << "\"];\n";
    }
    dot << "\n";

    // Emit edges (from interference graph stored in regAlloc)
    // We re-derive edges from the assignment — if two nodes have the
    // same register they SHOULDN'T interfere (this validates correctness)
    std::unordered_set<std::string> emitted;
    for (auto& [a, ra2] : assign) {
        for (auto& [b, rb2] : assign) {
            if (a >= b) continue;
            std::string key = a + "|" + b;
            if (emitted.count(key)) continue;
            emitted.insert(key);
            // Only show edges between nodes (simplified — real edges come from liveness)
            auto ita = nodeId.find(a);
            auto itb = nodeId.find(b);
            if (ita != nodeId.end() && itb != nodeId.end()) {
                bool conflict = (ra2 == rb2); // same register = real interference
                if (conflict)
                    dot << "  n" << ita->second << " -> n" << itb->second
                        << " [color=red, penwidth=2];\n";
            }
        }
    }

    dot << "}\n";
    return dot.str();
}

// ─────────────────────────────────────────────────────────────
//  computeStats / printStats
// ─────────────────────────────────────────────────────────────
IRPrinter::IRStats IRPrinter::computeStats(const IRModule& m) const {
    IRStats s;
    s.functions = (int)m.functions.size();
    for (auto& fn : m.functions) {
        s.basicBlocks += (int)fn->blocks.size();
        s.loops       += (int)fn->loops.size();
        std::unordered_set<std::string> temps;
        for (auto& bb : fn->blocks) {
            s.maxLoopDepth = std::max(s.maxLoopDepth, bb->loopDepth);
            for (auto& ins : bb->instrs) {
                s.instructions++;
                if (ins.op == IROp::PHI) s.phiNodes++;
                if (ins.isTerminator() &&
                    (ins.op==IROp::CJUMP||ins.op==IROp::CJUMP_TRUE||ins.op==IROp::CJUMP_FALSE))
                    s.branches++;
                if (ins.op==IROp::CALL||ins.op==IROp::CALL_VOID) s.calls++;
                if (ins.op==IROp::LOAD||ins.op==IROp::STORE||
                    ins.op==IROp::ARRAY_LOAD||ins.op==IROp::ARRAY_STORE) s.memOps++;
                if (ins.definesValue() && ins.result.isTemp())
                    temps.insert(ins.result.name);
            }
        }
        s.temporaries += (int)temps.size();
    }
    return s;
}

void IRPrinter::printStats(const IRStats& s) const {
    out << "\n" << PB << PC
        << "── IR Statistics ─────────────────────────────────────────\n"
        << PS
        << "  Functions    : " << PB << s.functions    << PS << "\n"
        << "  Basic blocks : " << PB << s.basicBlocks  << PS << "\n"
        << "  Instructions : " << PB << s.instructions << PS << "\n"
        << "  Temporaries  : " << PB << s.temporaries  << PS << "\n"
        << "  Phi nodes    : " << PB << s.phiNodes     << PS << "\n"
        << "  Branches     : " << PB << s.branches     << PS << "\n"
        << "  Calls        : " << PB << s.calls        << PS << "\n"
        << "  Memory ops   : " << PB << s.memOps       << PS << "\n"
        << "  Loops        : " << PB << s.loops        << PS << "\n"
        << "  Max loop depth: " << PB << s.maxLoopDepth << PS << "\n\n";
}
