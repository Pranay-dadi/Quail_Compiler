#pragma once
// ============================================================
//  IRPipeline.h  —  Quail Compiler
//
//  Orchestrates the full IR analysis and optimization pipeline.
//
//  FIX v2:
//    • IRBuilder::build() now returns unique_ptr<IRModule>
//    • IRPipeline::run() assigns that directly to moduleOwner
//      so there is exactly one owner and no double-free.
//    • module (raw observer pointer) is set from moduleOwner.get()
//      after assignment.
// ============================================================
#include "ir/IRBuilder.h"
#include "ir/IRCFG.h"
#include "optimizer/SSAPass.h"
#include "optimizer/OptimizationPasses.h"
#include "optimizer/RegisterAllocator.h"
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <map>

// ─────────────────────────────────────────────────────────────
//  Pipeline options
// ─────────────────────────────────────────────────────────────
struct IRPipelineOpts {
    bool buildSSA          = true;
    bool destroySSA        = true;
    bool constantFolding   = true;
    bool constantProp      = true;
    bool cse               = true;
    bool copyProp          = true;
    bool dce               = true;
    bool strengthReduction = true;
    bool licm              = true;
    bool inductionVar      = true;
    bool peephole          = true;
    bool bbOpt             = true;
    bool registerAlloc     = true;
    int  numRegisters      = 8;
    int  optIterations     = 3;

    static IRPipelineOpts O0() {
        IRPipelineOpts o;
        o.buildSSA = o.destroySSA = false;
        o.constantFolding = o.constantProp = o.cse = o.copyProp =
        o.dce = o.strengthReduction = o.licm = o.inductionVar =
        o.peephole = o.bbOpt = false;
        return o;
    }
    static IRPipelineOpts O1() {
        IRPipelineOpts o;
        o.licm = o.inductionVar = o.cse = false;
        return o;
    }
    static IRPipelineOpts O2() { return IRPipelineOpts{}; }
    static IRPipelineOpts O3() {
        IRPipelineOpts o;
        o.optIterations = 6;
        return o;
    }
};

// ─────────────────────────────────────────────────────────────
//  Per-pass statistics
// ─────────────────────────────────────────────────────────────
struct PipelinePassStat {
    std::string passName;
    int         instrBefore = 0;
    int         instrAfter  = 0;
    int         changed     = 0;
    int         removed     = 0;
    double      timeMs      = 0.0;
};

// ─────────────────────────────────────────────────────────────
//  IRPipeline
// ─────────────────────────────────────────────────────────────
class IRPipeline {
public:
    IRPipeline() = default;

    void setOpts(const IRPipelineOpts& o) { opts = o; }

    // Run the full pipeline.  Returns a non-owning pointer to
    // the module (lifetime = this IRPipeline object).
    IRModule* run(AST* ast, const std::string& moduleName = "quail");

    IRModule* getModule() const { return module; }
    const std::vector<PipelinePassStat>& getPassStats() const { return passStats; }

    void printIR()              const;
    void printReport()          const;
    void saveDOT(const std::string& outDir) const;
    void printRegAllocReport()  const;
    void printQuadruples()      const;
    void printSSA()             const;
    std::string summaryLine()   const;

private:
    IRPipelineOpts opts;

    // moduleOwner holds the unique_ptr returned by IRBuilder::build().
    // module is a non-owning observer pointer into moduleOwner.
    std::unique_ptr<IRModule>  moduleOwner;
    IRModule*                  module    = nullptr;

    std::vector<PipelinePassStat>      passStats;
    std::unique_ptr<SSAPass>           ssaPass;
    std::unique_ptr<RegisterAllocator> regAlloc;
    std::string                        ssaSnapshot;

    int totalInstr(const IRModule& m) const;

    template<typename PassT>
    void runPass(PassT& pass, IRFunction& fn, const std::string& name);
};