#include "ir/IRPipeline.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <map>

static const char* BLD = "\033[1m";
static const char* GRN = "\033[1;32m";
static const char* RED = "\033[1;31m";
static const char* YLW = "\033[1;33m";
static const char* CYN = "\033[1;36m";
static const char* MGN = "\033[1;35m";
static const char* DIM = "\033[2m";
static const char* RST = "\033[0m";

int IRPipeline::totalInstr(const IRModule& m) const {
    int c = 0;
    for (auto& fn : m.functions) c += fn->totalInstructions();
    return c;
}

template<typename PassT>
void IRPipeline::runPass(PassT& pass, IRFunction& fn, const std::string& name) {
    int before = fn.totalInstructions();
    auto t0 = std::chrono::steady_clock::now();
    pass.run(fn);
    auto t1 = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    int after = fn.totalInstructions();
    PipelinePassStat s;
    s.passName    = name;
    s.instrBefore = before;
    s.instrAfter  = after;
    s.changed     = pass.getStats().instructionsChanged;
    s.removed     = pass.getStats().instructionsRemoved
                  + pass.getStats().blocksRemoved * 3;
    s.timeMs      = ms;
    passStats.push_back(s);
}

// ─────────────────────────────────────────────────────────────
//  run()
//
//  KEY FIX: IRBuilder::build() now returns unique_ptr<IRModule>.
//  We assign it directly to moduleOwner (unique_ptr ← unique_ptr),
//  then point the observer `module` at moduleOwner.get().
//  No raw pointer is ever wrapped in a second unique_ptr.
// ─────────────────────────────────────────────────────────────
IRModule* IRPipeline::run(AST* ast, const std::string& moduleName) {
    passStats.clear();
    module = nullptr;
    moduleOwner.reset();

    // ── Phase 1: AST → three-address IR ──────────────────────
    IRBuilder builder(moduleName);
    moduleOwner = builder.build(ast);   // FIX: unique_ptr transfer, no raw-wrap
    module      = moduleOwner.get();    // observer pointer into owned module

    if (!module) {
        std::cerr << "\033[1;31m[IR] IRBuilder returned null module\033[0m\n";
        return nullptr;
    }
    if (builder.hasErrors()) {
        for (auto& e : builder.getErrors())
            std::cerr << "\033[1;31m[IR] " << e << "\033[0m\n";
    }
    if (module->functions.empty()) {
        std::cerr << "\033[2m[IR] No functions generated — skipping pipeline\033[0m\n";
        return module;
    }

    // ── Phase 2: SSA construction ─────────────────────────────
    if (opts.buildSSA) {
        ssaPass = std::make_unique<SSAPass>();
        for (auto& fn : module->functions)
            ssaPass->toSSA(*fn);
        std::ostringstream oss;
        for (auto& fn : module->functions) oss << fn->toString();
        ssaSnapshot = oss.str();
    }

    // ── Phase 3: Optimization passes ─────────────────────────
    for (int iter = 0; iter < opts.optIterations; iter++) {
        for (auto& fn : module->functions) {
            if (opts.constantFolding)   { ConstantFoldingPass   p; runPass(p,*fn,"ConstantFolding"); }
            if (opts.constantProp)      { ConstantPropagationPass p; runPass(p,*fn,"ConstantPropagation"); }
            if (opts.cse)               { CSEPass               p; runPass(p,*fn,"CSE"); }
            if (opts.copyProp)          { CopyPropagationPass   p; runPass(p,*fn,"CopyPropagation"); }
            if (opts.strengthReduction) { StrengthReductionPass p; runPass(p,*fn,"StrengthReduction"); }
            if (opts.licm)              { LICMPass              p; runPass(p,*fn,"LICM"); }
            if (opts.inductionVar)      { InductionVarPass      p; runPass(p,*fn,"InductionVar"); }
            if (opts.dce)               { DeadCodeEliminationPass p; runPass(p,*fn,"DCE"); }
            if (opts.peephole)          { PeepholePass          p; runPass(p,*fn,"Peephole"); }
            if (opts.bbOpt)             { BasicBlockOptPass     p; runPass(p,*fn,"BasicBlockOpt"); }
        }
    }

    // ── Phase 4: SSA destruction ──────────────────────────────
    if (opts.buildSSA && opts.destroySSA && ssaPass) {
        for (auto& fn : module->functions) {
            ssaPass->fromSSA(*fn);
            fn->rebuildEdges();
            // Full post-fromSSA cleanup: copy-prop first, then dead code, then peephole.
            { CopyPropagationPass   cp;  cp.run(*fn); }
            { DeadCodeEliminationPass d1; d1.run(*fn); }
            { PeepholePass          pp;  pp.run(*fn); }
            { DeadCodeEliminationPass d2; d2.run(*fn); }
        }
    }

    // ── Phase 5: Register allocation ─────────────────────────
    if (opts.registerAlloc) {
        regAlloc = std::make_unique<RegisterAllocator>(opts.numRegisters);
        for (auto& fn : module->functions)
            regAlloc->allocate(*fn);
    }

    return module;
}

// ─────────────────────────────────────────────────────────────
//  printIR
// ─────────────────────────────────────────────────────────────
void IRPipeline::printIR() const {
    if (!module) return;
    std::cout << "\n" << BLD << CYN
              << "╔══════════════════════════════════════════════════════════╗\n"
              << "║          THREE-ADDRESS CODE (IR)                        ║\n"
              << "╚══════════════════════════════════════════════════════════╝\n"
              << RST;
    for (auto& fn : module->functions) {
        std::cout << "\n" << BLD << MGN << fn->returnType << " " << fn->name << "(";
        for (size_t i = 0; i < fn->params.size(); i++) {
            if (i) std::cout << ", ";
            std::cout << fn->params[i].first << " " << fn->params[i].second;
        }
        std::cout << ")" << RST << " {\n";
        for (auto& bb : fn->blocks) {
            bool hasLive = !bb->liveIn.empty() || !bb->liveOut.empty();
            if (hasLive) {
                std::cout << DIM << "  ; liveIn:  {";
                bool first = true;
                for (auto& v : bb->liveIn) { if (!first) std::cout << ","; first=false; std::cout << v; }
                std::cout << "}\n  ; liveOut: {";
                first = true;
                for (auto& v : bb->liveOut) { if (!first) std::cout << ","; first=false; std::cout << v; }
                std::cout << "}" << RST << "\n";
            }
            std::cout << YLW << bb->label << ":" << RST;
            if (bb->isLoopHead)
                std::cout << DIM << "  ; loop header depth=" << bb->loopDepth << RST;
            std::cout << "\n";
            for (auto& ins : bb->instrs) {
                if (ins.op == IROp::NOP || ins.op == IROp::LABEL) continue;
                std::string line = ins.toString();
                if (regAlloc && ins.definesValue() && ins.result.isTemp()) {
                    std::string reg = regAlloc->getRegister(ins.result.name);
                    if (!reg.empty())
                        line += DIM + std::string("  ; ") + ins.result.name + "→" + reg + RST;
                    else if (regAlloc->isSpilled(ins.result.name))
                        line += DIM + std::string("  ; ") + ins.result.name + "→[spill]" + RST;
                }
                std::cout << line << "\n";
            }
        }
        std::cout << "}\n";
    }
}

// ─────────────────────────────────────────────────────────────
//  printQuadruples
// ─────────────────────────────────────────────────────────────
void IRPipeline::printQuadruples() const {
    if (!module) return;
    std::cout << "\n" << BLD << CYN
              << "╔══════════════════════════════════════════════════════════╗\n"
              << "║          QUADRUPLE TABLE  (op, arg1, arg2, result)      ║\n"
              << "╚══════════════════════════════════════════════════════════╝\n"
              << RST;
    const int W0=6, W1=14, W2=14, W3=14, W4=16;
    std::string sep(W0+W1+W2+W3+W4+4, '-');
    for (auto& fn : module->functions) {
        std::cout << "\n" << BLD << MGN << fn->name << "()" << RST << "\n"
                  << sep << "\n"
                  << BLD
                  << std::left << std::setw(W0) << "#"
                  << std::setw(W1) << "OP"
                  << std::setw(W2) << "ARG1"
                  << std::setw(W3) << "ARG2"
                  << std::setw(W4) << "RESULT"
                  << RST << "\n" << sep << "\n";
        int idx = 0;
        for (auto& bb : fn->blocks) {
            std::cout << DIM << "  --- " << bb->label << " ---" << RST << "\n";
            for (auto& ins : bb->instrs) {
                if (ins.op == IROp::NOP || ins.op == IROp::COMMENT) continue;
                std::string a1 = ins.arg1.isUndef()  ? "-" : ins.arg1.toString();
                std::string a2 = ins.arg2.isUndef()  ? "-" : ins.arg2.toString();
                std::string rs = ins.result.isUndef() ? "-" : ins.result.toString();
                if (ins.op == IROp::CALL || ins.op == IROp::CALL_VOID) {
                    a2 = "(";
                    for (size_t i=0; i<ins.callArgs.size(); i++) {
                        if (i) a2 += ",";
                        a2 += ins.callArgs[i].toString();
                    }
                    a2 += ")";
                }
                if (ins.op == IROp::PHI) {
                    a1 = "phi("; bool f=true;
                    for (auto& [lbl,v] : ins.phiSources) {
                        if (!f) a1+=","; f=false;
                        a1 += v.toString()+"<"+lbl+">";
                    }
                    a1 += ")"; a2 = "-";
                }
                const char* opCol =
                    (isBinaryOp(ins.op) && !isComparisonOp(ins.op)) ? YLW :
                    isComparisonOp(ins.op)  ? GRN  :
                    ins.isTerminator()      ? RED  :
                    (ins.op==IROp::CALL||ins.op==IROp::CALL_VOID) ? MGN : RST;
                std::cout << DIM << std::setw(W0) << idx++ << RST
                          << opCol << std::left << std::setw(W1) << opName(ins.op) << RST
                          << std::setw(W2) << a1
                          << std::setw(W3) << a2
                          << std::setw(W4) << rs << "\n";
            }
        }
        std::cout << sep << "\n";
    }
}

// ─────────────────────────────────────────────────────────────
//  printSSA
// ─────────────────────────────────────────────────────────────
void IRPipeline::printSSA() const {
    if (ssaSnapshot.empty()) {
        std::cout << DIM << "  (SSA form not captured)\n" << RST;
        return;
    }
    std::cout << "\n" << BLD << CYN
              << "╔══════════════════════════════════════════════════════════╗\n"
              << "║          STATIC SINGLE ASSIGNMENT (SSA) FORM            ║\n"
              << "╚══════════════════════════════════════════════════════════╝\n"
              << RST << ssaSnapshot;
}

// ─────────────────────────────────────────────────────────────
//  printReport
// ─────────────────────────────────────────────────────────────
void IRPipeline::printReport() const {
    if (passStats.empty()) {
        std::cout << DIM << "  (no optimization passes ran)\n" << RST;
        return;
    }
    std::cout << "\n" << BLD << MGN
              << "╔══════════════════════════════════════════════════════════╗\n"
              << "║          OPTIMIZATION PASS REPORT                       ║\n"
              << "╚══════════════════════════════════════════════════════════╝\n"
              << RST;
    const int NW=24, IW=8, RW=8, CW=8, TW=10;
    std::string sep(NW+IW*2+RW+CW+TW+5, '-');
    std::cout << sep << "\n"
              << BLD
              << std::left  << std::setw(NW) << "Pass"
              << std::right << std::setw(IW) << "Before"
              << std::setw(IW) << "After"
              << std::setw(RW) << "Removed"
              << std::setw(CW) << "Changed"
              << std::setw(TW) << "Time(ms)"
              << RST << "\n" << sep << "\n";

    std::map<std::string, PipelinePassStat> totals;
    for (auto& s : passStats) {
        auto& t = totals[s.passName];
        t.passName    = s.passName;
        if (t.instrBefore == 0) t.instrBefore = s.instrBefore;  // capture first run
        t.instrAfter = s.instrAfter; 
        t.changed    += s.changed;
        t.removed    += s.removed;
        t.timeMs     += s.timeMs;
    }

    int overallBefore = 0, overallAfter = 0;
    bool first = true;
    for (auto& [name, s] : totals) {
        if (first) { overallBefore = s.instrBefore; first = false; }
        overallAfter = s.instrAfter;
        auto col = [](int v) -> const char* { return v > 0 ? GRN : RST; };
        std::cout << std::left  << std::setw(NW) << s.passName
                  << std::right << std::setw(IW) << s.instrBefore
                  << std::setw(IW) << s.instrAfter
                  << col(s.removed) << std::setw(RW) << s.removed << RST
                  << col(s.changed) << std::setw(CW) << s.changed << RST
                  << DIM << std::setw(TW) << std::fixed << std::setprecision(2)
                  << s.timeMs << RST << "\n";
    }
    std::cout << sep << "\n";
    int reduction = overallBefore > 0
        ? (int)((double)(overallBefore-overallAfter)/overallBefore*100) : 0;
    std::cout << BLD << std::left << std::setw(NW) << "TOTAL"
              << std::right << std::setw(IW) << overallBefore
              << std::setw(IW) << overallAfter
              << GRN << std::setw(RW) << (overallBefore-overallAfter) << RST
              << BLD << "\n" << RST;
    std::cout << "\n  Instruction reduction: "
              << (reduction > 0 ? GRN : RST) << BLD << reduction << "%" << RST << "\n\n";
    if (ssaPass) {
        auto& ss = ssaPass->getStats();
        std::cout << DIM << "  SSA: " << ss.phiInserted << " ϕ inserted, "
                  << ss.renames << " renames, "
                  << ss.phiRemoved << " ϕ removed\n" << RST;
    }
}

// ─────────────────────────────────────────────────────────────
//  printRegAllocReport
// ─────────────────────────────────────────────────────────────
void IRPipeline::printRegAllocReport() const {
    if (!regAlloc || !module) {
        std::cout << DIM << "  (register allocation not run)\n" << RST;
        return;
    }
    for (auto& fn : module->functions)
        regAlloc->printReport(*fn);
}

// ─────────────────────────────────────────────────────────────
//  saveDOT
// ─────────────────────────────────────────────────────────────
void IRPipeline::saveDOT(const std::string& outDir) const {
    if (!module) return;
    for (auto& fn : module->functions) {
        std::string path = outDir + "/" + fn->name + "_ir_cfg.dot";
        std::ofstream f(path);
        if (!f.is_open()) {
            std::cerr << RED << "Cannot write DOT: " << path << RST << "\n";
            continue;
        }
        f << fn->toDOT();
        std::cout << DIM << "  IR CFG DOT: " << path << RST << "\n";
    }
}

// ─────────────────────────────────────────────────────────────
//  summaryLine
// ─────────────────────────────────────────────────────────────
std::string IRPipeline::summaryLine() const {
    if (!module) return "(no module)";
    int total = 0;
    for (auto& fn : module->functions) total += fn->totalInstructions();
    std::ostringstream oss;
    oss << (int)module->functions.size() << " function(s), "
        << total << " IR instruction(s)";
    if (regAlloc) {
        auto& s = regAlloc->getStats();
        oss << ", " << s.registersUsed << "/" << opts.numRegisters << " regs";
        if (s.spillCount > 0) oss << ", " << s.spillCount << " spill(s)";
    }
    return oss.str();
}