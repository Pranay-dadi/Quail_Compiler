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

// ── Constructor ────────────────────────────────────────────────
CodeGen::CodeGen()
    : builder(context),
      module(std::make_unique<llvm::Module>("quail", context)) {}

// ── Error helper ──────────────────────────────────────────────
void CodeGen::addError(const std::string& msg) {
    for (auto& e : errors) if (e.message == msg) return;
    errors.push_back({msg});
}

// ════════════════════════════════════════════════════════════════
//  Type helpers
// ════════════════════════════════════════════════════════════════

// Map ASTType → LLVM type
llvm::Type* CodeGen::llvmType(ASTType t) {
    switch (t) {
        case ASTType::Float:   return llvm::Type::getDoubleTy(context);
        case ASTType::Void:    return llvm::Type::getVoidTy(context);
        case ASTType::Int:
        default:               return llvm::Type::getInt32Ty(context);
    }
}

// Map ValueType (SymbolTable) → LLVM type
llvm::Type* CodeGen::llvmType(ValueType t) {
    switch (t) {
        case ValueType::Float:  return llvm::Type::getDoubleTy(context);
        case ValueType::Void:   return llvm::Type::getVoidTy(context);
        case ValueType::Int:
        default:                return llvm::Type::getInt32Ty(context);
    }
}

// Convert ASTType → ValueType for SymbolTable insertion
static ValueType astToValueType(ASTType t) {
    switch (t) {
        case ASTType::Float:   return ValueType::Float;
        case ASTType::Void:    return ValueType::Void;
        case ASTType::Int:
        default:               return ValueType::Int;
    }
}

// ── Coerce a value to the target LLVM type ────────────────────
// Handles int↔float promotion/truncation transparently.
llvm::Value* CodeGen::coerce(llvm::Value* val, llvm::Type* targetTy) {
    if (!val || !targetTy) return val;
    llvm::Type* srcTy = val->getType();
    if (srcTy == targetTy) return val;

    // i1 → i32
    if (srcTy->isIntegerTy(1) && targetTy->isIntegerTy(32))
        return builder.CreateZExt(val, targetTy, "bool_to_int");

    // i1 → double
    if (srcTy->isIntegerTy(1) && targetTy->isDoubleTy())
        return builder.CreateUIToFP(val, targetTy, "bool_to_fp");

    // i32 → double  (int → float promotion)
    if (srcTy->isIntegerTy(32) && targetTy->isDoubleTy())
        return builder.CreateSIToFP(val, targetTy, "int_to_fp");

    // double → i32  (float → int truncation)
    if (srcTy->isDoubleTy() && targetTy->isIntegerTy(32))
        return builder.CreateFPToSI(val, targetTy, "fp_to_int");

    // i32 → i1  (for branch conditions from int)
    if (srcTy->isIntegerTy(32) && targetTy->isIntegerTy(1))
        return builder.CreateICmpNE(val,
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0), "int_to_bool");

    // Fallthrough: return as-is (let LLVM catch the mismatch)
    addError("Type mismatch: cannot coerce '" +
             std::string(srcTy->isIntegerTy() ? "int" : "float") + "' to target type");
    return val;
}

// ── Promote both operands to a common numeric type ─────────────
// Rule: if either is double, both become double; otherwise both stay i32.
std::pair<llvm::Value*, llvm::Value*>
CodeGen::promoteToCommon(llvm::Value* lhs, llvm::Value* rhs) {
    if (!lhs || !rhs) return {lhs, rhs};

    auto* i32 = llvm::Type::getInt32Ty(context);
    auto* f64 = llvm::Type::getDoubleTy(context);

    // Unwrap i1 on both sides first
    if (lhs->getType()->isIntegerTy(1)) lhs = builder.CreateZExt(lhs, i32);
    if (rhs->getType()->isIntegerTy(1)) rhs = builder.CreateZExt(rhs, i32);

    bool lhsFloat = lhs->getType()->isDoubleTy();
    bool rhsFloat = rhs->getType()->isDoubleTy();

    if (lhsFloat && !rhsFloat) rhs = builder.CreateSIToFP(rhs, f64, "promote_rhs");
    if (!lhsFloat && rhsFloat) lhs = builder.CreateSIToFP(lhs, f64, "promote_lhs");

    return {lhs, rhs};
}

// ── IR string capture ─────────────────────────────────────────
std::string CodeGen::getIRString() const {
    std::string s;
    llvm::raw_string_ostream os(s);
    module->print(os, nullptr);
    return s;
}

// ── Optimization statistics ───────────────────────────────────
void CodeGen::collectStats(OptStats::FuncStat& fs, llvm::Function& fn, bool before) {
    size_t instrs = 0, blocks = 0;
    for (auto& bb : fn) { ++blocks; for (auto& i : bb) { (void)i; ++instrs; } }
    if (before) { fs.instrBefore  = instrs; fs.blocksBefore = blocks; }
    else        { fs.instrAfter   = instrs; fs.blocksAfter  = blocks; }
}

// ── Optimize ──────────────────────────────────────────────────
void CodeGen::optimize(OptLevel level) {
    if (!module || level == OptLevel::O0) return;

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
        llvm::ModulePassManager  MPM;
        llvm::FunctionPassManager FPM;
        FPM.addPass(llvm::PromotePass());
        FPM.addPass(llvm::InstCombinePass());
        FPM.addPass(llvm::ReassociatePass());
        FPM.addPass(llvm::GVNPass());
        FPM.addPass(llvm::SimplifyCFGPass());
        MPM.addPass(llvm::createModuleToFunctionPassAdaptor(std::move(FPM)));
        MPM.run(*module, MAM);
    } else if (level == OptLevel::O2) {
        auto MPM = PB.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O2);
        MPM.run(*module, MAM);
    } else if (level == OptLevel::O3) {
        auto MPM = PB.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O3);
        MPM.run(*module, MAM);
    }

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

// ════════════════════════════════════════════════════════════════
//  Code generation
// ════════════════════════════════════════════════════════════════

llvm::Value* CodeGen::generate(AST* node) {
    if (!node) {
        addError("[CodeGen] Internal: null AST node");
        return nullptr;
    }

    // ── Comments: no IR ────────────────────────────────────────
    if (dynamic_cast<LineCommentAST*>(node))  return nullptr;
    if (dynamic_cast<BlockCommentAST*>(node)) return nullptr;

    // ── Integer literal ────────────────────────────────────────
    if (auto* n = dynamic_cast<NumberAST*>(node))
        return llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), n->val);

    // ── Float literal ──────────────────────────────────────────
    if (auto* f = dynamic_cast<FloatAST*>(node))
        return llvm::ConstantFP::get(llvm::Type::getDoubleTy(context), f->val);

    // ── Variable reference ─────────────────────────────────────
    if (auto* v = dynamic_cast<VariableAST*>(node)) {
        const Symbol* sym = symbols.lookup(v->name);
        if (!sym) {
            addError("Use of undeclared variable '" + v->name + "'");
            return nullptr;
        }
        if (sym->kind == SymbolKind::Function) {
            addError("'" + v->name + "' is a function, not a variable");
            return nullptr;
        }
        // Load using the symbol's declared type
        llvm::Type* ty = llvmType(sym->type);
        return builder.CreateLoad(ty, sym->value, v->name);
    }

    // ── Variable declaration ───────────────────────────────────
    if (auto* vd = dynamic_cast<VarDeclAST*>(node)) {
        // Redeclaration check
        if (symbols.isDeclaredInCurrentScope(vd->name)) {
            addError("Redeclaration of '" + vd->name + "' in same scope");
            return nullptr;
        }
        llvm::Type* ty    = llvmType(vd->type);
        auto*       alloc = builder.CreateAlloca(ty, nullptr, vd->name);
        try {
            symbols.insert(vd->name, astToValueType(vd->type),
                           SymbolKind::Variable, alloc);
        } catch (const std::runtime_error& e) {
            addError(e.what());
        }
        return alloc;
    }

    // ── Assignment ─────────────────────────────────────────────
    if (auto* a = dynamic_cast<AssignAST*>(node)) {
        Symbol* sym = symbols.lookup(a->name);
        if (!sym) {
            addError("Assignment to undeclared variable '" + a->name + "'");
            return nullptr;
        }
        auto* val = generate(a->expr.get());
        if (!val) return nullptr;
        // Coerce RHS to the variable's declared type
        val = coerce(val, llvmType(sym->type));
        if (!val) return nullptr;
        builder.CreateStore(val, sym->value);
        return val;
    }

    // ── If / If-Else ───────────────────────────────────────────
    if (auto* i = dynamic_cast<IfAST*>(node)) {
        auto* condVal = generate(i->cond.get());
        if (!condVal) return nullptr;
        auto* cond = toBool(condVal);
        if (!cond) return nullptr;
        auto* fn      = builder.GetInsertBlock()->getParent();
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

    // ── While ──────────────────────────────────────────────────
    if (auto* w = dynamic_cast<WhileAST*>(node)) {
        auto* fn      = builder.GetInsertBlock()->getParent();
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

    // ── For loop ───────────────────────────────────────────────
    if (auto* f = dynamic_cast<ForAST*>(node)) {
        if (f->init) generate(f->init.get());
        auto* fn      = builder.GetInsertBlock()->getParent();
        auto* condBB  = llvm::BasicBlock::Create(context, "for.cond", fn);
        auto* bodyBB  = llvm::BasicBlock::Create(context, "for.body", fn);
        auto* incBB   = llvm::BasicBlock::Create(context, "for.inc",  fn);
        auto* endBB   = llvm::BasicBlock::Create(context, "for.end",  fn);
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

    // ── Block ──────────────────────────────────────────────────
    if (auto* b = dynamic_cast<BlockAST*>(node)) {
        symbols.enterScope();
        for (auto& stmt : b->statements) {
            generate(stmt.get());
            if (builder.GetInsertBlock()->getTerminator()) break;
        }
        symbols.exitScope();
        return nullptr;
    }

    // ── Function definition ────────────────────────────────────
    if (auto* f = dynamic_cast<FunctionAST*>(node)) {
        // Build param type list from prototype's argTypes
        std::vector<llvm::Type*> paramTypes;
        std::vector<ValueType>   paramVT;
        for (size_t i = 0; i < f->proto->args.size(); ++i) {
            ASTType at = (i < f->proto->argTypes.size())
                         ? f->proto->argTypes[i]
                         : ASTType::Int;
            paramTypes.push_back(llvmType(at));
            paramVT.push_back(astToValueType(at));
        }

        llvm::Type* retTy = llvmType(f->proto->returnType);
        auto*       ft    = llvm::FunctionType::get(retTy, paramTypes, false);
        auto*       fn    = llvm::Function::Create(
                                ft, llvm::Function::ExternalLinkage,
                                f->proto->name, *module);

        // Register function in symbol table
        try {
            symbols.insertFunction(f->proto->name,
                                   astToValueType(f->proto->returnType),
                                   paramVT, fn);
        } catch (const std::runtime_error& e) {
            addError(e.what());
            fn->eraseFromParent();
            return nullptr;
        }

        auto* entry = llvm::BasicBlock::Create(context, "entry", fn);
        builder.SetInsertPoint(entry);
        symbols.enterScope();
        symbols.setCurrentFunction(f->proto->name);

        size_t idx = 0;
        for (auto& arg : fn->args()) {
            const std::string& pname = f->proto->args[idx];
            ASTType at = (idx < f->proto->argTypes.size())
                         ? f->proto->argTypes[idx]
                         : ASTType::Int;
            auto* alloc = builder.CreateAlloca(llvmType(at), nullptr, pname);
            builder.CreateStore(&arg, alloc);
            try {
                symbols.insert(pname, astToValueType(at),
                               SymbolKind::Parameter, alloc);
            } catch (const std::runtime_error& e) {
                addError(e.what());
            }
            idx++;
        }

        generate(f->body.get());

        // Auto-add return if missing
        if (!builder.GetInsertBlock()->getTerminator()) {
            if (retTy->isVoidTy())
                builder.CreateRetVoid();
            else if (retTy->isDoubleTy())
                builder.CreateRet(llvm::ConstantFP::get(retTy, 0.0));
            else
                builder.CreateRet(llvm::ConstantInt::get(retTy, 0));
        }

        symbols.clearCurrentFunction();
        symbols.exitScope();

        // Verify the function
        std::string errStr;
        llvm::raw_string_ostream errStream(errStr);
        if (llvm::verifyFunction(*fn, &errStream))
            addError("LLVM IR verification failed for '" +
                     f->proto->name + "': " + errStream.str());
        return fn;
    }

    // ── Function call ──────────────────────────────────────────
    if (auto* c = dynamic_cast<CallAST*>(node)) {
        // Look up in symbol table first (to get type info), then LLVM module
        const Symbol* sym = symbols.lookup(c->callee);
        auto*         fn  = module->getFunction(c->callee);

        if (!fn) {
            addError("Call to undefined function '" + c->callee + "'");
            return nullptr;
        }
        if (fn->arg_size() != c->args.size()) {
            addError("Wrong argument count to '" + c->callee + "': expected "
                     + std::to_string(fn->arg_size())
                     + ", got " + std::to_string(c->args.size()));
            return nullptr;
        }

        std::vector<llvm::Value*> args;
        size_t pi = 0;
        for (auto& a : c->args) {
            auto* v = generate(a.get());
            if (!v) return nullptr;
            // Coerce each argument to the function's expected parameter type
            llvm::Type* expectedTy = fn->getFunctionType()->getParamType(pi++);
            v = coerce(v, expectedTy);
            args.push_back(v);
        }
        return builder.CreateCall(fn, args, fn->getReturnType()->isVoidTy() ? "" : "calltmp");
    }

    // ── Array declaration ──────────────────────────────────────
    if (auto* a = dynamic_cast<ArrayDeclAST*>(node)) {
        if (a->size <= 0) {
            addError("Array '" + a->name + "' has invalid size");
            return nullptr;
        }
        if (symbols.isDeclaredInCurrentScope(a->name)) {
            addError("Redeclaration of array '" + a->name + "' in same scope");
            return nullptr;
        }
        llvm::Type* elemTy = llvmType(a->type);
        auto* arrTy  = llvm::ArrayType::get(elemTy, a->size);
        auto* alloc  = builder.CreateAlloca(arrTy, nullptr, a->name);
        try {
            symbols.insert(a->name, astToValueType(a->type),
                           SymbolKind::Array, alloc, a->size);
        } catch (const std::runtime_error& e) {
            addError(e.what());
        }
        return alloc;
    }

    // ── Array access ───────────────────────────────────────────
    if (auto* arr = dynamic_cast<ArrayAccessAST*>(node)) {
        const Symbol* sym = symbols.lookup(arr->name);
        if (!sym) {
            addError("Use of undeclared array '" + arr->name + "'");
            return nullptr;
        }
        if (sym->kind != SymbolKind::Array) {
            addError("'" + arr->name + "' is not an array");
            return nullptr;
        }
        auto* idx    = generate(arr->index.get()); if (!idx) return nullptr;
        idx = coerce(idx, llvm::Type::getInt32Ty(context)); // index must be i32
        auto* alloca = llvm::cast<llvm::AllocaInst>(sym->value);
        auto* arrTy  = alloca->getAllocatedType();
        auto* zero   = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);
        auto* gep    = builder.CreateGEP(arrTy, sym->value, {zero, idx}, arr->name + ".gep");
        return builder.CreateLoad(llvmType(sym->type), gep, arr->name + ".load");
    }

    // ── Array assign ───────────────────────────────────────────
    if (auto* aa = dynamic_cast<ArrayAssignAST*>(node)) {
        const Symbol* sym = symbols.lookup(aa->name);
        if (!sym) {
            addError("Assignment to undeclared array '" + aa->name + "'");
            return nullptr;
        }
        if (sym->kind != SymbolKind::Array) {
            addError("'" + aa->name + "' is not an array");
            return nullptr;
        }
        auto* idx = generate(aa->index.get()); if (!idx) return nullptr;
        idx = coerce(idx, llvm::Type::getInt32Ty(context));
        auto* val = generate(aa->expr.get());  if (!val) return nullptr;
        val = coerce(val, llvmType(sym->type)); // coerce value to element type
        auto* alloca = llvm::cast<llvm::AllocaInst>(sym->value);
        auto* arrTy  = alloca->getAllocatedType();
        auto* zero   = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);
        auto* gep    = builder.CreateGEP(arrTy, sym->value, {zero, idx}, aa->name + ".gep");
        builder.CreateStore(val, gep);
        return val;
    }

    // ── Binary operators ───────────────────────────────────────
    if (auto* bin = dynamic_cast<BinaryAST*>(node)) {
        auto* lhs = generate(bin->lhs.get());
        auto* rhs = generate(bin->rhs.get());
        if (!lhs || !rhs) return nullptr;

        // Load through any pointer (shouldn't normally occur in practice)
        if (lhs->getType()->isPointerTy())
            lhs = builder.CreateLoad(llvm::Type::getInt32Ty(context), lhs);
        if (rhs->getType()->isPointerTy())
            rhs = builder.CreateLoad(llvm::Type::getInt32Ty(context), rhs);

        // Promote to common type (int+float → float+float)
        auto [l, r] = promoteToCommon(lhs, rhs);
        bool isFloat = l->getType()->isDoubleTy();

        if (bin->op == "+")  return isFloat ? builder.CreateFAdd(l,r,"fadd") : builder.CreateAdd(l,r,"add");
        if (bin->op == "-")  return isFloat ? builder.CreateFSub(l,r,"fsub") : builder.CreateSub(l,r,"sub");
        if (bin->op == "*")  return isFloat ? builder.CreateFMul(l,r,"fmul") : builder.CreateMul(l,r,"mul");
        if (bin->op == "/") {
            if (!isFloat) {
                // Integer division by zero guard
                auto* zero = llvm::ConstantInt::get(l->getType(), 0);
                auto* isZero = builder.CreateICmpEQ(r, zero, "divzero");
                // Replace zero divisor with 1 silently to avoid UB
                auto* safeR = builder.CreateSelect(isZero,
                    llvm::ConstantInt::get(l->getType(), 1), r, "safe_div");
                return builder.CreateSDiv(l, safeR, "div");
            }
            return builder.CreateFDiv(l, r, "fdiv");
        }
        if (bin->op == "<")  return isFloat ? builder.CreateFCmpOLT(l,r,"flt") : builder.CreateICmpSLT(l,r,"lt");
        if (bin->op == ">")  return isFloat ? builder.CreateFCmpOGT(l,r,"fgt") : builder.CreateICmpSGT(l,r,"gt");
        if (bin->op == "<=") return isFloat ? builder.CreateFCmpOLE(l,r,"fle") : builder.CreateICmpSLE(l,r,"le");
        if (bin->op == ">=") return isFloat ? builder.CreateFCmpOGE(l,r,"fge") : builder.CreateICmpSGE(l,r,"ge");
        if (bin->op == "==") return isFloat ? builder.CreateFCmpOEQ(l,r,"feq") : builder.CreateICmpEQ(l,r,"eq");
        if (bin->op == "!=") return isFloat ? builder.CreateFCmpONE(l,r,"fne") : builder.CreateICmpNE(l,r,"ne");

        addError("Unknown binary operator '" + bin->op + "'");
        return nullptr;
    }

    // ── Logical operators (short-circuit) ─────────────────────
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
        // == and != on logical values
        auto* lhs = generate(log->lhs.get());
        auto* rhs = generate(log->rhs.get());
        if (!lhs || !rhs) return nullptr;
        auto [l, r] = promoteToCommon(lhs, rhs);
        if (log->op == "==") return builder.CreateICmpEQ(l, r);
        if (log->op == "!=") return builder.CreateICmpNE(l, r);
        addError("Unknown logical operator '" + log->op + "'");
        return nullptr;
    }

    // ── Unary operators ────────────────────────────────────────
    if (auto* u = dynamic_cast<UnaryAST*>(node)) {
        auto* operand = generate(u->operand.get()); if (!operand) return nullptr;
        if (u->op == "-") {
            operand = coerce(operand, llvm::Type::getInt32Ty(context));
            return builder.CreateNeg(operand, "neg");
        }
        if (u->op == "!") {
            auto* b = toBool(operand); if (!b) return nullptr;
            return builder.CreateNot(b, "not");
        }
        addError("Unknown unary operator '" + u->op + "'");
        return nullptr;
    }

    // ── Return ─────────────────────────────────────────────────
    if (auto* ret = dynamic_cast<ReturnAST*>(node)) {
        auto* fn    = builder.GetInsertBlock()->getParent();
        auto* retTy = fn->getReturnType();

        llvm::Value* val = nullptr;
        if (ret->expr) {
            val = generate(ret->expr.get()); if (!val) return nullptr;
            val = coerce(val, retTy);
        } else {
            if (retTy->isVoidTy())   return builder.CreateRetVoid();
            if (retTy->isDoubleTy()) val = llvm::ConstantFP::get(retTy, 0.0);
            else                     val = llvm::ConstantInt::get(retTy, 0);
        }
        return builder.CreateRet(val);
    }

    // ── Break / Continue ───────────────────────────────────────
    if (dynamic_cast<BreakAST*>(node)) {
        if (breakStack.empty()) { addError("'break' outside loop"); return nullptr; }
        return builder.CreateBr(breakStack.back());
    }
    if (dynamic_cast<ContinueAST*>(node)) {
        if (continueStack.empty()) { addError("'continue' outside loop"); return nullptr; }
        return builder.CreateBr(continueStack.back());
    }

    // ── Post-increment ─────────────────────────────────────────
    if (auto* inc = dynamic_cast<PostIncAST*>(node)) {
        Symbol* sym = symbols.lookup(inc->name);
        if (!sym) {
            addError("Use of undeclared variable '" + inc->name + "' in '++'");
            return nullptr;
        }
        llvm::Type* ty  = llvmType(sym->type);
        auto*       old = builder.CreateLoad(ty, sym->value, inc->name);
        llvm::Value* one;
        if (ty->isDoubleTy())
            one = llvm::ConstantFP::get(ty, 1.0);
        else
            one = llvm::ConstantInt::get(ty, 1);
        llvm::Value* incremented = ty->isDoubleTy()
            ? builder.CreateFAdd(old, one, "finc")
            : builder.CreateAdd(old, one, "inc");
        builder.CreateStore(incremented, sym->value);
        return old; // post-increment returns old value
    }

    // ── Program ────────────────────────────────────────────────
    if (auto* prog = dynamic_cast<ProgramAST*>(node)) {
        for (auto& item : prog->topLevel)
            generate(item.get());
        return nullptr;
    }

    addError(std::string("Unhandled AST node type: ") + typeid(*node).name());
    return nullptr;
}

// ── toBool ────────────────────────────────────────────────────
llvm::Value* CodeGen::toBool(llvm::Value* v) {
    if (!v) return nullptr;
    if (v->getType()->isIntegerTy(1))  return v;
    if (v->getType()->isIntegerTy(32))
        return builder.CreateICmpNE(v,
            llvm::ConstantInt::get(v->getType(), 0), "bool");
    if (v->getType()->isDoubleTy())
        return builder.CreateFCmpONE(v,
            llvm::ConstantFP::get(v->getType(), 0.0), "fbool");
    addError("toBool: unsupported type");
    return nullptr;
}

// ── File / dump helpers ───────────────────────────────────────
void CodeGen::dumpToFile(const std::string& filename) {
    std::error_code EC;
    llvm::raw_fd_ostream out(filename, EC);
    if (EC) { addError("Cannot write '" + filename + "': " + EC.message()); return; }
    module->print(out, nullptr);
}

void CodeGen::dump() {
    module->print(llvm::outs(), nullptr);
}