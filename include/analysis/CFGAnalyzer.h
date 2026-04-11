#pragma once
// ============================================================
//  CFGAnalyzer.h  —  Quail Compiler
//
//  Extracts Control Flow Graphs and Call Graphs from an
//  LLVM Module, detects loop back-edges, computes cyclomatic
//  complexity, and emits Graphviz DOT files.
//
//  Usage (after CodeGen::generate() + optional optimize()):
//    CFGAnalyzer analyzer(codeGen.getModule());
//    analyzer.analyze();
//    analyzer.saveDOT(outDir+"/main_cfg.dot",
//                     analyzer.generateAllFunctionsDOT());
//    CFGAnalyzer::render(outDir+"/main_cfg.dot",
//                        outDir+"/main_cfg.png");
// ============================================================
#pragma once
#include <llvm/IR/Module.h>
#include <string>
#include <vector>
#include <set>
#include <unordered_map>
#include <utility>

// ── Per–basic-block data ──────────────────────────────────────
struct CFGBlock {
    std::string name;
    int         instrCount   = 0;
    bool        isEntry      = false;
    bool        isExit       = false;   // ends with ret / unreachable
    bool        isLoopHeader = false;   // has a back-edge pointing here
    bool        isDeadCode   = false;   // no predecessor (besides entry)
    std::string terminatorKind;         // "ret","br","cond_br","unreachable"
    std::vector<std::string> instrPreview; // first ≤5 IR lines
    std::vector<std::string> successors;
    std::vector<std::string> predecessors;
};

// ── Per-function CFG ──────────────────────────────────────────
struct FunctionCFG {
    std::string name;
    std::string returnTypeStr;
    std::vector<CFGBlock>                           blocks;
    std::vector<std::pair<std::string,std::string>> edges;
    std::set<std::pair<std::string,std::string>>    backEdges;
    int numNodes             = 0;
    int numEdges             = 0;
    int loopCount            = 0;
    int cyclomaticComplexity = 0;  // M = E – N + 2P (P=1)
    int deadBlocks           = 0;
};

// ── Call-graph edge ───────────────────────────────────────────
struct CallEdge {
    std::string caller;
    std::string callee;
    bool        isRecursive = false;
    bool        isExternal  = false;  // callee not defined in module
};

// ─────────────────────────────────────────────────────────────
class CFGAnalyzer {
public:
    explicit CFGAnalyzer(llvm::Module* module);

    // Run all analyses (must call before any generate* method)
    void analyze();

    // ── DOT generators ───────────────────────────────────────
    // One DOT per function (detailed — shows IR preview)
    std::string generateFunctionDOT(const FunctionCFG& fn,
                                    bool showInstructions = true) const;

    // All functions as subgraph clusters in one DOT
    std::string generateAllFunctionsDOT(bool showInstructions = false) const;

    // Whole-program call graph
    std::string generateCallGraphDOT() const;

    // ── File helpers ─────────────────────────────────────────
    bool saveDOT(const std::string& path,
                 const std::string& content) const;

    // Render DOT → PNG/SVG/PDF via graphviz `dot`.
    // Returns false if graphviz is not installed.
    static bool render(const std::string& dotPath,
                       const std::string& outPath,
                       const std::string& fmt = "png");

    // ── Console reports ──────────────────────────────────────
    void printComplexityReport() const;
    void printCFGSummary()       const;
    void printDeadCodeReport()   const;

    // ── Accessors ────────────────────────────────────────────
    const std::vector<FunctionCFG>& getFunctions() const { return functions_; }
    const std::vector<CallEdge>&    getCallEdges()  const { return callEdges_; }
    bool hasDeadCode() const;

private:
    llvm::Module*            module_;
    std::vector<FunctionCFG> functions_;
    std::vector<CallEdge>    callEdges_;

    // caller → set<callee> (raw, before flattening to callEdges_)
    std::unordered_map<std::string, std::set<std::string>> rawCallGraph_;

    // ── Analysis helpers ──────────────────────────────────────
    void analyzeFunction(llvm::Function& fn);
    void detectBackEdges(FunctionCFG& cfg);
    void detectDeadBlocks(FunctionCFG& cfg);
    void dfsVisit(const std::string& node,
                  std::unordered_map<std::string, int>& color,
                  const std::unordered_map<std::string,
                        std::vector<std::string>>& adj,
                  FunctionCFG& cfg);

    // ── DOT helpers ───────────────────────────────────────────
    std::string nodeId(const FunctionCFG& fn,
                       const std::string& block) const;
    std::string sanitize(const std::string& s) const;
    std::string escapeDot(const std::string& s) const;
    std::string complexityColor(int M) const;
    std::string riskLabel(int M) const;
};