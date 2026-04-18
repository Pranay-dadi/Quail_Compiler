#pragma once
// ============================================================
//  IRIntegration.h  —  Quail Compiler
//
//  Integrates the new IR/optimizer layer into the existing
//  compileSinglePass() function.
//
//  Drop this header into your existing project and call
//  runIRPipeline() after the parser/AST stage.
//
//  The IR pipeline is independent of LLVM — it operates
//  entirely on the Quail IR (IRModule / IRFunction) and
//  produces its own three-address code output, optimization
//  reports, CFG DOT files, and register allocation.
// ============================================================
#include "ir/IRPipeline.h"
#include "ir/IRPrinter.h"
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

// ─────────────────────────────────────────────────────────────
//  IRRunOptions — what to display / save
// ─────────────────────────────────────────────────────────────
struct IRRunOptions {
    bool printIR         = true;   // three-address listing
    bool printQuadruples = false;  // (op,arg1,arg2,result) table
    bool printSSA        = false;  // SSA form snapshot
    bool printReport     = true;   // optimization pass stats
    bool printRegAlloc   = false;  // register allocation table
    bool printStats      = true;   // IR metrics summary
    bool saveCFGDOT      = false;  // write CFG .dot files
    bool saveDAGDOT      = false;  // write DAG .dot files per block
    bool renderDOT       = false;  // call graphviz dot
    bool showLiveness    = false;  // annotate liveIn/liveOut
    IRPipelineOpts pipeOpts = IRPipelineOpts::O2();
};

// ─────────────────────────────────────────────────────────────
//  runIRPipeline()
//  Call this after parse + AST is ready.
//  stem  = filename stem for output files
//  outDir = directory for .dot / .png outputs
// ─────────────────────────────────────────────────────────────
inline void runIRPipeline(AST*               ast,
                           const std::string& stem,
                           const std::string& outDir,
                           const IRRunOptions& irOpts,
                           bool               verbose)
{
    if (!ast) return;

    IRPipeline pipeline;
    pipeline.setOpts(irOpts.pipeOpts);

    if (verbose)
        std::cout << "\n\033[1m\033[1;35m"
                  << "╔══════════════════════════════════════════════════════════╗\n"
                  << "║         QUAIL IR PIPELINE                               ║\n"
                  << "╚══════════════════════════════════════════════════════════╝\n"
                  << "\033[0m";

    IRModule* mod = pipeline.run(ast, stem);
    if (!mod) {
        if (verbose)
            std::cerr << "\033[1;31mIR pipeline returned null module\033[0m\n";
        return;
    }

    IRPrinter printer;

    // ── Statistics ────────────────────────────────────────────
    if (irOpts.printStats && verbose) {
        auto stats = printer.computeStats(*mod);
        printer.printStats(stats);
    }

    // ── Three-address IR listing ──────────────────────────────
    if (irOpts.printIR && verbose)
        printer.printModule(*mod, irOpts.showLiveness);

    // ── Quadruple table ───────────────────────────────────────
    if (irOpts.printQuadruples && verbose)
        for (auto& fn : mod->functions)
            printer.printQuadrupleTable(*fn);

    // ── SSA form ──────────────────────────────────────────────
    if (irOpts.printSSA && verbose)
        pipeline.printSSA();

    // ── Optimization report ───────────────────────────────────
    if (irOpts.printReport && verbose)
        pipeline.printReport();

    // ── Register allocation report ────────────────────────────
    if (irOpts.printRegAlloc && verbose)
        pipeline.printRegAllocReport();

    // ── Save CFG DOT files ────────────────────────────────────
    if (irOpts.saveCFGDOT) {
        fs::create_directories(outDir);
        for (auto& fn : mod->functions) {
            std::string path = outDir + "/" + stem + "_" + fn->name + "_cfg.dot";
            std::ofstream f(path);
            if (f) {
                f << fn->toDOT();
                if (verbose)
                    std::cout << "\033[2m  IR CFG DOT: " << path << "\033[0m\n";
                if (irOpts.renderDOT) {
                    std::string png = outDir + "/" + stem + "_" + fn->name + "_cfg.png";
                    std::string cmd = "dot -Tpng \"" + path + "\" -o \"" + png + "\" 2>/dev/null";
                    if (std::system(cmd.c_str()) == 0 && verbose)
                        std::cout << "\033[1;32m  IR CFG PNG: " << png << "\033[0m\n";
                }
            }
        }
    }

    // ── Save DAG DOT files ────────────────────────────────────
    if (irOpts.saveDAGDOT) {
        fs::create_directories(outDir);
        for (auto& fn : mod->functions) {
            for (auto& bb : fn->blocks) {
                std::string path = outDir + "/" + stem + "_" + fn->name
                                 + "_" + bb->label + "_dag.dot";
                std::ofstream f(path);
                if (f) {
                    f << printer.dagToDOT(*bb);
                    if (verbose)
                        std::cout << "\033[2m  DAG DOT: " << path << "\033[0m\n";
                }
            }
        }
    }

    if (verbose)
        std::cout << "\033[2m  IR summary: " << pipeline.summaryLine()
                  << "\033[0m\n";
}

// ─────────────────────────────────────────────────────────────
//  Flag parsing helpers for main.cpp
//  Add these to the argv loop in main():
//
//    else if (a == "--ir")            irOpts.printIR         = true;
//    else if (a == "--ir-quads")      irOpts.printQuadruples = true;
//    else if (a == "--ir-ssa")        irOpts.printSSA        = true;
//    else if (a == "--ir-report")     irOpts.printReport     = true;
//    else if (a == "--ir-regalloc")   irOpts.printRegAlloc   = true;
//    else if (a == "--ir-stats")      irOpts.printStats      = true;
//    else if (a == "--ir-cfg")        irOpts.saveCFGDOT      = true;
//    else if (a == "--ir-dag")        irOpts.saveDAGDOT      = true;
//    else if (a == "--ir-liveness")   irOpts.showLiveness    = true;
//    else if (a == "--ir-O0")         irOpts.pipeOpts = IRPipelineOpts::O0();
//    else if (a == "--ir-O1")         irOpts.pipeOpts = IRPipelineOpts::O1();
//    else if (a == "--ir-O2")         irOpts.pipeOpts = IRPipelineOpts::O2();
//    else if (a == "--ir-O3")         irOpts.pipeOpts = IRPipelineOpts::O3();
//    else if (a == "--ir-regs" && i+1 < argc)
//                                     irOpts.pipeOpts.numRegisters = std::stoi(argv[++i]);
//    else if (a == "--ir-all") {
//        irOpts.printIR=irOpts.printQuadruples=irOpts.printSSA=
//        irOpts.printReport=irOpts.printRegAlloc=irOpts.printStats=
//        irOpts.saveCFGDOT=irOpts.saveDAGDOT=true;
//    }
// ─────────────────────────────────────────────────────────────
