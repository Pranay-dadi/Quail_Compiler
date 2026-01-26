#include "codegen/CodeGen.h"
#include "utils/Logger.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include <iostream>
#include "llvm/Passes/PassBuilder.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Passes/OptimizationLevel.h"

// for raw_string_ostream
CodeGen::CodeGen()
    : builder(context), module(std::make_unique<llvm::Module>("quail", context)) {
    // Optional: set module target triple / data layout if needed
}

llvm::Value* CodeGen::generate(AST* node) {
    if (!node) return nullptr;

    // Remove per-node logging – too noisy
    // Logger::log(Stage::CODEGEN, "Generating IR for " + std::string(typeid(*node).name()));

    if (auto* n = dynamic_cast<NumberAST*>(node)) {
        return llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), n->val);
    }

    if (auto* v = dynamic_cast<VariableAST*>(node)) {
        auto ptr = symbols.lookup(v->name);
        if (!ptr) {
            std::cerr << "Undefined variable: " << v->name << "\n";
            return nullptr;
        }
        return builder.CreateLoad(llvm::Type::getInt32Ty(context), ptr, v->name);
    }

    if (auto* vd = dynamic_cast<VarDeclAST*>(node)) {
        auto* alloc = builder.CreateAlloca(llvm::Type::getInt32Ty(context), nullptr, vd->name);
        symbols.insert(vd->name, alloc);
        // Comment out if you want C-like uninitialized behavior
        // builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0), alloc);
        return alloc;
    }

    if (auto* a = dynamic_cast<AssignAST*>(node)) {
        auto val = generate(a->expr.get());
        if (!val) return nullptr;
        auto ptr = symbols.lookup(a->name);
        if (!ptr) {
            std::cerr << "Assignment to undefined variable: " << a->name << "\n";
            return nullptr;
        }
        builder.CreateStore(val, ptr);
        return val;
    }

    // If statement
    if (auto* i = dynamic_cast<IfAST*>(node)) {
        auto cond = toBool(generate(i->cond.get()));
        if (!cond) return nullptr;

        auto fn = builder.GetInsertBlock()->getParent();
        auto thenBB = llvm::BasicBlock::Create(context, "then", fn);
        auto elseBB = llvm::BasicBlock::Create(context, "else");
        auto mergeBB = llvm::BasicBlock::Create(context, "merge");

        builder.CreateCondBr(cond, thenBB, elseBB);

        builder.SetInsertPoint(thenBB);
        generate(i->thenBlock.get());
        builder.CreateBr(mergeBB);

        fn->getBasicBlockList().push_back(elseBB);
        builder.SetInsertPoint(elseBB);
        if (i->elseBlock) generate(i->elseBlock.get());
        builder.CreateBr(mergeBB);

        fn->getBasicBlockList().push_back(mergeBB);
        builder.SetInsertPoint(mergeBB);
        return nullptr;
    }

    // While loop
    if (auto* w = dynamic_cast<WhileAST*>(node)) {
        auto fn = builder.GetInsertBlock()->getParent();
        auto condBB = llvm::BasicBlock::Create(context, "cond", fn);
        auto loopBB = llvm::BasicBlock::Create(context, "loop");
        auto afterBB = llvm::BasicBlock::Create(context, "after");

        builder.CreateBr(condBB);
        builder.SetInsertPoint(condBB);

        auto cond = toBool(generate(w->cond.get()));
        if (!cond) return nullptr;

        builder.CreateCondBr(cond, loopBB, afterBB);

        fn->getBasicBlockList().push_back(loopBB);
        builder.SetInsertPoint(loopBB);

        breakStack.push_back(afterBB);
        continueStack.push_back(condBB);
        generate(w->body.get());
        breakStack.pop_back();
        continueStack.pop_back();

        builder.CreateBr(condBB);

        fn->getBasicBlockList().push_back(afterBB);
        builder.SetInsertPoint(afterBB);
        return nullptr;
    }

    if (auto* b = dynamic_cast<BlockAST*>(node)) {
        symbols.enterScope();
        for (auto& stmt : b->statements) {
            generate(stmt.get());
        }
        symbols.exitScope();
        return nullptr;
    }

    // Function definition
    if (auto* f = dynamic_cast<FunctionAST*>(node)) {
        std::vector<llvm::Type*> paramTypes(f->proto->args.size(), llvm::Type::getInt32Ty(context));
        auto* ft = llvm::FunctionType::get(llvm::Type::getInt32Ty(context), paramTypes, false);

        auto* fn = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                                          f->proto->name, module.get());

        auto* entry = llvm::BasicBlock::Create(context, "entry", fn);
        builder.SetInsertPoint(entry);

        symbols.enterScope();

        size_t idx = 0;
        for (auto& arg : fn->args()) {
            // Allocate in entry block
            auto* alloc = builder.CreateAlloca(llvm::Type::getInt32Ty(context),
                                               nullptr, f->proto->args[idx]);
            builder.CreateStore(&arg, alloc);           // ← FIXED: removed wrong &
            symbols.insert(f->proto->args[idx++], alloc);
        }

        generate(f->body.get());

        symbols.exitScope();

        // Add default return 0 if no return statement
        if (!entry->getTerminator()) {
            builder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0));
        }

        return fn;
    }

    // Function call
    if (auto* c = dynamic_cast<CallAST*>(node)) {
        auto* fn = module->getFunction(c->callee);
        if (!fn) {
            std::cerr << "Undefined function: " << c->callee << "\n";
            return nullptr;
        }

        std::vector<llvm::Value*> args;
        for (auto& a : c->args) {
            auto* argVal = generate(a.get());
            if (!argVal) return nullptr;
            args.push_back(argVal);
        }
        return builder.CreateCall(fn, args);
    }

    if (auto* a = dynamic_cast<ArrayDeclAST*>(node)) {
        auto* arrType = llvm::ArrayType::get(llvm::Type::getInt32Ty(context), a->size);
        auto* alloc = builder.CreateAlloca(arrType, nullptr, a->name);
        symbols.insert(a->name, alloc);
        // Note: no initialization – arrays contain garbage
        return alloc;
    }

    if (auto* arr = dynamic_cast<ArrayAccessAST*>(node)) {
        auto* arrayPtr = symbols.lookup(arr->name);
        if (!arrayPtr) return nullptr;

        auto* index = generate(arr->index.get());
        if (!index) return nullptr;

        auto* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);
        std::vector<llvm::Value*> idxs = {zero, index};

        auto* elemPtr = builder.CreateGEP(
            arrayPtr->getType()->getPointerElementType(),
            arrayPtr, idxs, "arrayidx");

        return builder.CreateLoad(llvm::Type::getInt32Ty(context), elemPtr);
    }

    if (auto* b = dynamic_cast<BinaryAST*>(node)) {
        auto* L = generate(b->lhs.get());
        auto* R = generate(b->rhs.get());
        if (!L || !R) return nullptr;

        // Dereference if needed (should rarely happen now)
        if (L->getType()->isPointerTy()) L = builder.CreateLoad(llvm::Type::getInt32Ty(context), L);
        if (R->getType()->isPointerTy()) R = builder.CreateLoad(llvm::Type::getInt32Ty(context), R);

        if (b->op == "+") return builder.CreateAdd(L, R);
        if (b->op == "-") return builder.CreateSub(L, R);
        if (b->op == "*") return builder.CreateMul(L, R);
        if (b->op == "/") return builder.CreateSDiv(L, R);

        if (b->op == "<")  return builder.CreateICmpSLT(L, R);
        if (b->op == ">")  return builder.CreateICmpSGT(L, R);
        if (b->op == "<=") return builder.CreateICmpSLE(L, R);
        if (b->op == ">=") return builder.CreateICmpSGE(L, R);
        if (b->op == "==") return builder.CreateICmpEQ(L, R);
        if (b->op == "!=") return builder.CreateICmpNE(L, R);
    }

    if (auto* p = dynamic_cast<ProgramAST*>(node)) {
        for (auto& fn : p->functions) {
            generate(fn.get());
        }
        return nullptr;
    }

    if (auto* r = dynamic_cast<ReturnAST*>(node)) {
        auto* val = r->expr ? generate(r->expr.get()) : nullptr;
        if (val) {
            if (val->getType()->isIntegerTy(1)) {
                val = builder.CreateZExt(val, llvm::Type::getInt32Ty(context), "booltoint");
            }
            return builder.CreateRet(val);
        }
        return builder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0));
    }

    // For loop (simplified condition handling)
    if (auto* f = dynamic_cast<ForAST*>(node)) {
        if (f->init) generate(f->init.get());

        auto* fn = builder.GetInsertBlock()->getParent();
        auto* condBB = llvm::BasicBlock::Create(context, "for.cond", fn);
        auto* loopBB = llvm::BasicBlock::Create(context, "for.body");
        auto* incBB  = llvm::BasicBlock::Create(context, "for.inc");
        auto* endBB  = llvm::BasicBlock::Create(context, "for.end");

        builder.CreateBr(condBB);
        builder.SetInsertPoint(condBB);

        auto* condVal = f->cond ? generate(f->cond.get()) : nullptr;
        llvm::Value* cond = condVal ? toBool(condVal) : llvm::ConstantInt::getTrue(context);

        builder.CreateCondBr(cond, loopBB, endBB);

        fn->getBasicBlockList().push_back(loopBB);
        builder.SetInsertPoint(loopBB);

        breakStack.push_back(endBB);
        continueStack.push_back(incBB);
        generate(f->body.get());
        breakStack.pop_back();
        continueStack.pop_back();

        builder.CreateBr(incBB);

        fn->getBasicBlockList().push_back(incBB);
        builder.SetInsertPoint(incBB);
        if (f->inc) generate(f->inc.get());
        builder.CreateBr(condBB);

        fn->getBasicBlockList().push_back(endBB);
        builder.SetInsertPoint(endBB);
        return nullptr;
    }

    if (dynamic_cast<BreakAST*>(node)) {
        if (breakStack.empty()) {
            std::cerr << "break outside loop\n";
            return nullptr;
        }
        return builder.CreateBr(breakStack.back());
    }

    if (dynamic_cast<ContinueAST*>(node)) {
        if (continueStack.empty()) {
            std::cerr << "continue outside loop\n";
            return nullptr;
        }
        return builder.CreateBr(continueStack.back());
    }

    if (auto* l = dynamic_cast<LogicalAST*>(node)) {
        // short-circuit && and ||
        auto* fn = builder.GetInsertBlock()->getParent();
        auto* lhsBB = builder.GetInsertBlock();
        auto* rhsBB = llvm::BasicBlock::Create(context, "logic.rhs", fn);
        auto* mergeBB = llvm::BasicBlock::Create(context, "logic.end", fn);

        auto* L = toBool(generate(l->lhs.get()));
        if (!L) return nullptr;

        if (l->op == "&&") {
            builder.CreateCondBr(L, rhsBB, mergeBB);
        } else { // ||
            builder.CreateCondBr(L, mergeBB, rhsBB);
        }

        builder.SetInsertPoint(rhsBB);
        auto* R = toBool(generate(l->rhs.get()));
        builder.CreateBr(mergeBB);

        builder.SetInsertPoint(mergeBB);
        auto* phi = builder.CreatePHI(llvm::Type::getInt1Ty(context), 2);
        phi->addIncoming(l->op == "&&" ? llvm::ConstantInt::getFalse(context)
                                        : llvm::ConstantInt::getTrue(context), lhsBB);
        phi->addIncoming(R, rhsBB);
        return phi;
    }

    if (auto* u = dynamic_cast<UnaryAST*>(node)) {
        auto* val = generate(u->operand.get());
        if (!val) return nullptr;

        if (u->op == "-") return builder.CreateNeg(val, "negtmp");
        if (u->op == "!") {
            auto* b = toBool(val);
            return builder.CreateNot(b, "nottmp");
        }
    }

    if (auto* a = dynamic_cast<ArrayAssignAST*>(node)) {
        auto* arrayPtr = symbols.lookup(a->name);
        if (!arrayPtr) return nullptr;

        auto* idx = generate(a->index.get());
        if (!idx) return nullptr;

        auto* val = generate(a->expr.get());
        if (!val) return nullptr;

        auto* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);
        std::vector<llvm::Value*> idxs = {zero, idx};

        auto* elemPtr = builder.CreateGEP(
            arrayPtr->getType()->getPointerElementType(),
            arrayPtr, idxs);

        builder.CreateStore(val, elemPtr);
        return val;
    }

    std::cerr << "Unhandled AST node type in codegen\n";
    return nullptr;
}

void CodeGen::optimize() {
    if (!module) return;

    llvm::PassBuilder PB;

    llvm::LoopAnalysisManager LAM;
    llvm::FunctionAnalysisManager FAM;
    llvm::CGSCCAnalysisManager CGAM;
    llvm::ModuleAnalysisManager MAM;

    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);

    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

    // LLVM 14: buildPerModuleDefaultPipeline accepts PassBuilder::OptimizationLevel
    // Level 2 = -O2
    llvm::ModulePassManager MPM = PB.buildPerModuleDefaultPipeline(
        llvm::OptimizationLevel::O2
    );

    MPM.run(*module, MAM);

    // Optional verification
    std::string errstr;
    llvm::raw_string_ostream os(errstr);
    if (llvm::verifyModule(*module, &os)) {
        std::cerr << "Verification failed after optimization:\n" << os.str() << "\n";
    }
}

llvm::Value* CodeGen::toBool(llvm::Value* val) {
    if (!val) return nullptr;

    if (val->getType()->isIntegerTy(1)) {
        return val;
    }
    if (val->getType()->isIntegerTy(32)) {
        return builder.CreateICmpNE(val, llvm::ConstantInt::get(val->getType(), 0));
    }

    std::cerr << "Cannot convert value to bool (unsupported type)\n";
    return nullptr;
}

void CodeGen::dumpToFile(const std::string& filename) {
    std::error_code EC;
    llvm::raw_fd_ostream out(filename, EC);
    if (EC) {
        std::cerr << "Error opening file: " << EC.message() << "\n";
        return;
    }
    if (llvm::verifyModule(*module, &llvm::errs())) {
        std::cerr << "Module verification failed!\n";
    }
    module->print(out, nullptr);
}

void CodeGen::dump() {
    if (llvm::verifyModule(*module, &llvm::errs())) {
        std::cerr << "Module verification failed!\n";
    }
    module->print(llvm::outs(), nullptr);
}