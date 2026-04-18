#pragma once
// ============================================================
//  IRPrinter.h  —  Quail Compiler
//
//  Rich formatted output for the IR layer:
//    printModule / printFunction / printBlock / printInstruction
//    printQuadrupleTable  — (op, arg1, arg2, result) form
//    printDAG             — value DAG per basic block
//    interferenceGraphToDOT — register allocator interference graph
//    computeStats / printStats — IR metrics
// ============================================================
#include "ir/IRCFG.h"
#include "optimizer/RegisterAllocator.h"
#include <string>
#include <ostream>
#include <iostream>
#include <unordered_map>

class IRPrinter {
public:
    explicit IRPrinter(std::ostream& os = std::cout);

    void printModule   (const IRModule& m,  bool showLiveness = false) const;
    void printFunction (const IRFunction& fn, bool showLiveness = false) const;
    void printBlock    (const BasicBlock& bb, bool showLiveness = false) const;
    void printInstruction(const IRInstruction& ins,
                          const RegisterAllocator* ra = nullptr) const;

    void printQuadrupleTable(const IRFunction& fn) const;
    void printDAG(const BasicBlock& bb) const;

    std::string dagToDOT(const BasicBlock& bb) const;
    std::string interferenceGraphToDOT(const IRFunction& fn,
                                        const RegisterAllocator& ra) const;

    struct IRStats {
        int functions=0, basicBlocks=0, instructions=0, phiNodes=0;
        int branches=0, calls=0, memOps=0, loops=0;
        int maxLoopDepth=0, temporaries=0;
    };
    IRStats computeStats(const IRModule& m) const;
    void    printStats  (const IRStats& s) const;

private:
    std::ostream& out;
    std::string   escDot(const std::string& s) const;
};
