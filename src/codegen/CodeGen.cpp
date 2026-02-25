#include "codegen/CodeGen.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/OptimizationLevel.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/Analysis/CGSCCPassManager.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Scalar/Reassociate.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"
#include "llvm/Transforms/Utils/Mem2Reg.h"
#include "llvm/Support/raw_ostream.h"
#include <iostream>

// ── Constructor ────────────────────────────────────────────────────────────────
CodeGen::CodeGen()
    : builder(context),
      module(std::make_unique<llvm::Module>("quail", context)) {}

// ── Error helper ──────────────────────────────────────────────────────────────
void CodeGen::addError(const std::string& msg) {
    for (auto& e : errors) if (e.message == msg) return;
    errors.push_back({msg});
}

// ── IR string capture ─────────────────────────────────────────────────────────
std::string CodeGen::getIRString() const {
    std::string s;
    llvm::raw_string_ostream os(s);
    module->print(os, nullptr);
    return s;
}

// ── Optimization statistics ────────────────────────────────────────────────────
void CodeGen::collectStats(OptStats::FuncStat& fs, llvm::Function& fn, bool before) {
    size_t instrs = 0, blocks = 0;
    for (auto& bb : fn) { ++blocks; for (auto& i : bb) { (void)i; ++instrs; } }
    if (before) { fs.instrBefore  = instrs; fs.blocksBefore = blocks; }
    else        { fs.instrAfter   = instrs; fs.blocksAfter  = blocks; }
}

// ── Optimize ──────────────────────────────────────────────────────────────────
void CodeGen::optimize(OptLevel level) {
    if (!module || level == OptLevel::O0) return;

    // Collect BEFORE stats
    optStats = OptStats{};
    for (auto& fn : *module) {
        if (fn.isDeclaration()) continue;
        OptStats::FuncStat fs;
        fs.name = fn.getName().str();
        collectStats(fs, fn, true);
        optStats.totalInstrBefore  += fs.instrBefore;
        optStats.totalBlocksBefore += fs.blocksBefore;
        optStats.functions.push_back(fs);
    }

    // Build pass pipeline
    llvm::PassBuilder            PB;
    llvm::LoopAnalysisManager    LAM;
    llvm::FunctionAnalysisManager FAM;
    llvm::CGSCCAnalysisManager   CGAM;
    llvm::ModuleAnalysisManager  MAM;

    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

    if (level == OptLevel::O1) {
        // Hand-picked lightweight passes
        llvm::ModulePassManager  MPM;
        llvm::FunctionPassManager FPM;
        FPM.addPass(llvm::PromotePass());           // mem2reg: alloca → SSA registers
        FPM.addPass(llvm::InstCombinePass());       // peephole algebraic simplification
        FPM.addPass(llvm::ReassociatePass());       // reassociate for CSE
        FPM.addPass(llvm::GVNPass());               // global value numbering (dead code elim)
        FPM.addPass(llvm::SimplifyCFGPass());       // dead blocks, branch simplification
        MPM.addPass(llvm::createModuleToFunctionPassAdaptor(std::move(FPM)));
        MPM.run(*module, MAM);

    } else if (level == OptLevel::O2) {
        auto MPM = PB.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O2);
        MPM.run(*module, MAM);

    } else if (level == OptLevel::O3) {
        auto MPM = PB.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O3);
        MPM.run(*module, MAM);
    }

    // Collect AFTER stats
    size_t fi = 0;
    for (auto& fn : *module) {
        if (fn.isDeclaration()) continue;
        if (fi < optStats.functions.size()) {
            auto& fs = optStats.functions[fi++];
            collectStats(fs, fn, false);
            optStats.totalInstrAfter  += fs.instrAfter;
            optStats.totalBlocksAfter += fs.blocksAfter;
        }
    }
}

// ── Code generation ────────────────────────────────────────────────────────────
llvm::Value* CodeGen::generate(AST* node) {
    if (!node) {
        addError("[CodeGen] Internal: null AST node passed to generate()");
        return nullptr;
    }

    // ── Comments: no IR emitted, just pass through ─────────────────────────
    if (dynamic_cast<LineCommentAST*>(node))  return nullptr;
    if (dynamic_cast<BlockCommentAST*>(node)) return nullptr;

    // ── Number literal ─────────────────────────────────────────────────────
    if (auto* n = dynamic_cast<NumberAST*>(node))
        return llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), n->val);

    // ── Float literal ──────────────────────────────────────────────────────
    if (auto* f = dynamic_cast<FloatAST*>(node))
        return llvm::ConstantFP::get(llvm::Type::getDoubleTy(context), f->val);

    // ── Variable reference ─────────────────────────────────────────────────
    if (auto* v = dynamic_cast<VariableAST*>(node)) {
        auto* ptr = symbols.lookup(v->name);
        if (!ptr) { addError("Use of undeclared variable '" + v->name + "'"); return nullptr; }
        return builder.CreateLoad(llvm::Type::getInt32Ty(context), ptr, v->name);
    }

    // ── Variable declaration ───────────────────────────────────────────────
    if (auto* vd = dynamic_cast<VarDeclAST*>(node)) {
        auto* alloc = builder.CreateAlloca(
            llvm::Type::getInt32Ty(context), nullptr, vd->name);
        symbols.insert(vd->name, alloc);
        return alloc;
    }

    // ── Assignment ─────────────────────────────────────────────────────────
    if (auto* a = dynamic_cast<AssignAST*>(node)) {
        auto* val = generate(a->expr.get());
        if (!val) return nullptr;
        auto* ptr = symbols.lookup(a->name);
        if (!ptr) { addError("Assignment to undeclared variable '" + a->name + "'"); return nullptr; }
        builder.CreateStore(val, ptr);
        return val;
    }

    // ── If / If-Else ───────────────────────────────────────────────────────
    if (auto* i = dynamic_cast<IfAST*>(node)) {
        auto* condVal = generate(i->cond.get());
        if (!condVal) return nullptr;
        auto* cond = toBool(condVal);
        if (!cond) return nullptr;
        auto* fn = builder.GetInsertBlock()->getParent();
        auto* thenBB  = llvm::BasicBlock::Create(context, "then",   fn);
        auto* elseBB  = llvm::BasicBlock::Create(context, "else",   fn);
        auto* mergeBB = llvm::BasicBlock::Create(context, "ifcont", fn);
        builder.CreateCondBr(cond, thenBB, elseBB);
        builder.SetInsertPoint(thenBB);
        generate(i->thenBlock.get());
        if (!builder.GetInsertBlock()->getTerminator()) builder.CreateBr(mergeBB);
        builder.SetInsertPoint(elseBB);
        if (i->elseBlock) generate(i->elseBlock.get());
        if (!builder.GetInsertBlock()->getTerminator()) builder.CreateBr(mergeBB);
        builder.SetInsertPoint(mergeBB);
        return nullptr;
    }

    // ── While ──────────────────────────────────────────────────────────────
    if (auto* w = dynamic_cast<WhileAST*>(node)) {
        auto* fn = builder.GetInsertBlock()->getParent();
        auto* condBB  = llvm::BasicBlock::Create(context, "while.cond", fn);
        auto* bodyBB  = llvm::BasicBlock::Create(context, "while.body", fn);
        auto* afterBB = llvm::BasicBlock::Create(context, "while.end",  fn);
        builder.CreateBr(condBB);
        builder.SetInsertPoint(condBB);
        auto* cond = toBool(generate(w->cond.get()));
        if (!cond) return nullptr;
        builder.CreateCondBr(cond, bodyBB, afterBB);
        builder.SetInsertPoint(bodyBB);
        breakStack.push_back(afterBB); continueStack.push_back(condBB);
        generate(w->body.get());
        breakStack.pop_back(); continueStack.pop_back();
        if (!builder.GetInsertBlock()->getTerminator()) builder.CreateBr(condBB);
        builder.SetInsertPoint(afterBB);
        return nullptr;
    }

    // ── For loop ───────────────────────────────────────────────────────────
    if (auto* f = dynamic_cast<ForAST*>(node)) {
        if (f->init) generate(f->init.get());
        auto* fn = builder.GetInsertBlock()->getParent();
        auto* condBB = llvm::BasicBlock::Create(context, "for.cond", fn);
        auto* bodyBB = llvm::BasicBlock::Create(context, "for.body", fn);
        auto* incBB  = llvm::BasicBlock::Create(context, "for.inc",  fn);
        auto* endBB  = llvm::BasicBlock::Create(context, "for.end",  fn);
        builder.CreateBr(condBB);
        builder.SetInsertPoint(condBB);
        llvm::Value* condVal = f->cond ? generate(f->cond.get()) : nullptr;
        auto* cond = condVal ? toBool(condVal) : llvm::ConstantInt::getTrue(context);
        builder.CreateCondBr(cond, bodyBB, endBB);
        builder.SetInsertPoint(bodyBB);
        breakStack.push_back(endBB); continueStack.push_back(incBB);
        generate(f->body.get());
        breakStack.pop_back(); continueStack.pop_back();
        if (!builder.GetInsertBlock()->getTerminator()) builder.CreateBr(incBB);
        builder.SetInsertPoint(incBB);
        if (f->inc) generate(f->inc.get());
        builder.CreateBr(condBB);
        builder.SetInsertPoint(endBB);
        return nullptr;
    }

    // ── Block ──────────────────────────────────────────────────────────────
    if (auto* b = dynamic_cast<BlockAST*>(node)) {
        symbols.enterScope();
        for (auto& stmt : b->statements) {
            generate(stmt.get());
            if (builder.GetInsertBlock()->getTerminator()) break;
        }
        symbols.exitScope();
        return nullptr;
    }

    // ── Function definition ────────────────────────────────────────────────
    if (auto* f = dynamic_cast<FunctionAST*>(node)) {
        std::vector<llvm::Type*> paramTypes(
            f->proto->args.size(), llvm::Type::getInt32Ty(context));
        auto* ft = llvm::FunctionType::get(
            llvm::Type::getInt32Ty(context), paramTypes, false);
        auto* fn = llvm::Function::Create(
            ft, llvm::Function::ExternalLinkage, f->proto->name, *module);
        auto* entry = llvm::BasicBlock::Create(context, "entry", fn);
        builder.SetInsertPoint(entry);
        symbols.enterScope();
        size_t idx = 0;
        for (auto& arg : fn->args()) {
            auto* alloc = builder.CreateAlloca(
                llvm::Type::getInt32Ty(context), nullptr, f->proto->args[idx]);
            builder.CreateStore(&arg, alloc);
            symbols.insert(f->proto->args[idx++], alloc);
        }
        generate(f->body.get());
        if (!builder.GetInsertBlock()->getTerminator())
            builder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0));
        symbols.exitScope();
        std::string errStr;
        llvm::raw_string_ostream errStream(errStr);
        if (llvm::verifyFunction(*fn, &errStream))
            addError("LLVM IR verification failed for '" + f->proto->name + "': " + errStream.str());
        return fn;
    }

    // ── Function call ──────────────────────────────────────────────────────
    if (auto* c = dynamic_cast<CallAST*>(node)) {
        auto* fn = module->getFunction(c->callee);
        if (!fn) { addError("Call to undefined function '" + c->callee + "'"); return nullptr; }
        if (fn->arg_size() != c->args.size()) {
            addError("Wrong argument count to '" + c->callee + "': expected "
                     + std::to_string(fn->arg_size()) + ", got " + std::to_string(c->args.size()));
            return nullptr;
        }
        std::vector<llvm::Value*> args;
        for (auto& a : c->args) {
            auto* v = generate(a.get());
            if (!v) return nullptr;
            args.push_back(v);
        }
        return builder.CreateCall(fn, args);
    }

    // ── Array declaration ──────────────────────────────────────────────────
    if (auto* a = dynamic_cast<ArrayDeclAST*>(node)) {
        if (a->size <= 0) { addError("Array '" + a->name + "' has invalid size"); return nullptr; }
        auto* arrTy = llvm::ArrayType::get(llvm::Type::getInt32Ty(context), a->size);
        auto* alloc = builder.CreateAlloca(arrTy, nullptr, a->name);
        symbols.insert(a->name, alloc);
        return alloc;
    }

    // ── Array access ───────────────────────────────────────────────────────
    if (auto* arr = dynamic_cast<ArrayAccessAST*>(node)) {
        auto* base = symbols.lookup(arr->name);
        if (!base) { addError("Use of undeclared array '" + arr->name + "'"); return nullptr; }
        auto* idx = generate(arr->index.get()); if (!idx) return nullptr;
        auto* alloca = llvm::cast<llvm::AllocaInst>(base);
        auto* arrTy  = alloca->getAllocatedType();
        auto* zero   = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);
        auto* gep    = builder.CreateGEP(arrTy, base, {zero, idx}, arr->name + ".gep");
        return builder.CreateLoad(llvm::Type::getInt32Ty(context), gep, arr->name + ".load");
    }

    // ── Array assign ───────────────────────────────────────────────────────
    if (auto* aa = dynamic_cast<ArrayAssignAST*>(node)) {
        auto* base = symbols.lookup(aa->name);
        if (!base) { addError("Assignment to undeclared array '" + aa->name + "'"); return nullptr; }
        auto* idx = generate(aa->index.get()); if (!idx) return nullptr;
        auto* val = generate(aa->expr.get());  if (!val) return nullptr;
        auto* alloca = llvm::cast<llvm::AllocaInst>(base);
        auto* arrTy  = alloca->getAllocatedType();
        auto* zero   = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);
        auto* gep    = builder.CreateGEP(arrTy, base, {zero, idx}, aa->name + ".gep");
        builder.CreateStore(val, gep);
        return val;
    }

    // ── Binary operators ───────────────────────────────────────────────────
    if (auto* bin = dynamic_cast<BinaryAST*>(node)) {
        auto* lhs = generate(bin->lhs.get());
        auto* rhs = generate(bin->rhs.get());
        if (!lhs || !rhs) return nullptr;
        if (lhs->getType()->isPointerTy())
            lhs = builder.CreateLoad(llvm::Type::getInt32Ty(context), lhs);
        if (rhs->getType()->isPointerTy())
            rhs = builder.CreateLoad(llvm::Type::getInt32Ty(context), rhs);
        if (bin->op == "+")  return builder.CreateAdd(lhs, rhs);
        if (bin->op == "-")  return builder.CreateSub(lhs, rhs);
        if (bin->op == "*")  return builder.CreateMul(lhs, rhs);
        if (bin->op == "/")  return builder.CreateSDiv(lhs, rhs);
        if (bin->op == "<")  return builder.CreateICmpSLT(lhs, rhs);
        if (bin->op == ">")  return builder.CreateICmpSGT(lhs, rhs);
        if (bin->op == "<=") return builder.CreateICmpSLE(lhs, rhs);
        if (bin->op == ">=") return builder.CreateICmpSGE(lhs, rhs);
        if (bin->op == "==") return builder.CreateICmpEQ(lhs, rhs);
        if (bin->op == "!=") return builder.CreateICmpNE(lhs, rhs);
        addError("Unknown binary operator '" + bin->op + "'"); return nullptr;
    }

    // ── Logical operators ──────────────────────────────────────────────────
    if (auto* log = dynamic_cast<LogicalAST*>(node)) {
        if (log->op == "&&") {
            auto* fn      = builder.GetInsertBlock()->getParent();
            auto* lhsBB   = builder.GetInsertBlock();
            auto* rhsBB   = llvm::BasicBlock::Create(context, "and.rhs",   fn);
            auto* mergeBB = llvm::BasicBlock::Create(context, "and.merge", fn);
            auto* lhsVal  = toBool(generate(log->lhs.get())); if (!lhsVal) return nullptr;
            builder.CreateCondBr(lhsVal, rhsBB, mergeBB);
            lhsBB = builder.GetInsertBlock();
            builder.SetInsertPoint(rhsBB);
            auto* rhsVal = toBool(generate(log->rhs.get())); if (!rhsVal) return nullptr;
            auto* rhsEnd = builder.GetInsertBlock();
            builder.CreateBr(mergeBB);
            builder.SetInsertPoint(mergeBB);
            auto* phi = builder.CreatePHI(llvm::Type::getInt1Ty(context), 2);
            phi->addIncoming(llvm::ConstantInt::getFalse(context), lhsBB);
            phi->addIncoming(rhsVal, rhsEnd);
            return phi;
        }
        if (log->op == "||") {
            auto* fn      = builder.GetInsertBlock()->getParent();
            auto* lhsBB   = builder.GetInsertBlock();
            auto* rhsBB   = llvm::BasicBlock::Create(context, "or.rhs",   fn);
            auto* mergeBB = llvm::BasicBlock::Create(context, "or.merge", fn);
            auto* lhsVal  = toBool(generate(log->lhs.get())); if (!lhsVal) return nullptr;
            builder.CreateCondBr(lhsVal, mergeBB, rhsBB);
            lhsBB = builder.GetInsertBlock();
            builder.SetInsertPoint(rhsBB);
            auto* rhsVal = toBool(generate(log->rhs.get())); if (!rhsVal) return nullptr;
            auto* rhsEnd = builder.GetInsertBlock();
            builder.CreateBr(mergeBB);
            builder.SetInsertPoint(mergeBB);
            auto* phi = builder.CreatePHI(llvm::Type::getInt1Ty(context), 2);
            phi->addIncoming(llvm::ConstantInt::getTrue(context), lhsBB);
            phi->addIncoming(rhsVal, rhsEnd);
            return phi;
        }
        auto* lhs = generate(log->lhs.get());
        auto* rhs = generate(log->rhs.get());
        if (!lhs || !rhs) return nullptr;
        auto* i32 = llvm::Type::getInt32Ty(context);
        if (lhs->getType()->isIntegerTy(1)) lhs = builder.CreateZExt(lhs, i32);
        if (rhs->getType()->isIntegerTy(1)) rhs = builder.CreateZExt(rhs, i32);
        if (log->op == "==") return builder.CreateICmpEQ(lhs, rhs);
        if (log->op == "!=") return builder.CreateICmpNE(lhs, rhs);
        addError("Unknown logical operator '" + log->op + "'"); return nullptr;
    }

    // ── Unary operators ────────────────────────────────────────────────────
    if (auto* u = dynamic_cast<UnaryAST*>(node)) {
        auto* operand = generate(u->operand.get()); if (!operand) return nullptr;
        if (u->op == "-") {
            if (operand->getType()->isIntegerTy(1))
                operand = builder.CreateZExt(operand, llvm::Type::getInt32Ty(context));
            return builder.CreateNeg(operand, "negtmp");
        }
        if (u->op == "!") { auto* b = toBool(operand); if (!b) return nullptr; return builder.CreateNot(b, "nottmp"); }
        addError("Unknown unary operator '" + u->op + "'"); return nullptr;
    }

    // ── Return ─────────────────────────────────────────────────────────────
    if (auto* ret = dynamic_cast<ReturnAST*>(node)) {
        llvm::Value* val = nullptr;
        if (ret->expr) {
            val = generate(ret->expr.get()); if (!val) return nullptr;
            if (val->getType()->isIntegerTy(1))
                val = builder.CreateZExt(val, llvm::Type::getInt32Ty(context));
        } else {
            val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);
        }
        return builder.CreateRet(val);
    }

    // ── Break / Continue ───────────────────────────────────────────────────
    if (dynamic_cast<BreakAST*>(node)) {
        if (breakStack.empty()) { addError("'break' outside loop"); return nullptr; }
        builder.CreateBr(breakStack.back()); return nullptr;
    }
    if (dynamic_cast<ContinueAST*>(node)) {
        if (continueStack.empty()) { addError("'continue' outside loop"); return nullptr; }
        builder.CreateBr(continueStack.back()); return nullptr;
    }

    // ── Post-increment ─────────────────────────────────────────────────────
    if (auto* inc = dynamic_cast<PostIncAST*>(node)) {
        auto* ptr = symbols.lookup(inc->name);
        if (!ptr) { addError("Use of undeclared variable '" + inc->name + "' in '++'"); return nullptr; }
        auto* old = builder.CreateLoad(llvm::Type::getInt32Ty(context), ptr);
        auto* one = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 1);
        builder.CreateStore(builder.CreateAdd(old, one), ptr);
        return old;
    }

    // ── Program ────────────────────────────────────────────────────────────
    if (auto* prog = dynamic_cast<ProgramAST*>(node)) {
        for (auto& item : prog->topLevel)
            generate(item.get());
        return nullptr;
    }

    addError(std::string("Unhandled AST node type: ") + typeid(*node).name());
    return nullptr;
}

// ── Helpers ────────────────────────────────────────────────────────────────────

llvm::Value* CodeGen::toBool(llvm::Value* v) {
    if (!v) return nullptr;
    if (v->getType()->isIntegerTy(1))  return v;
    if (v->getType()->isIntegerTy(32))
        return builder.CreateICmpNE(v, llvm::ConstantInt::get(v->getType(), 0));
    addError("toBool: unsupported type"); return nullptr;
}

void CodeGen::dumpToFile(const std::string& filename) {
    std::error_code EC;
    llvm::raw_fd_ostream out(filename, EC);
    if (EC) { addError("Cannot write '" + filename + "': " + EC.message()); return; }
    module->print(out, nullptr);
}

void CodeGen::dump() {
    module->print(llvm::outs(), nullptr);
}