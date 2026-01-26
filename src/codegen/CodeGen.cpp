#include "codegen/CodeGen.h"
#include "utils/Logger.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Transforms/Scalar/InstSimplifyPass.h"
#include "llvm/Support/raw_ostream.h"

CodeGen::CodeGen()
    : builder(context), module(std::make_unique<llvm::Module>("Quail_Compiler", context)) {}

llvm::Value* CodeGen::generate(AST* node) {
    Logger::log(Stage::CODEGEN, "Generating LLVM IR");
    if (!node) return nullptr;
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
        // optional: store 0
        builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0), alloc);
        return alloc;
    }

    if (auto* a = dynamic_cast<AssignAST*>(node)) {
        auto val = generate(a->expr.get());
        auto ptr = symbols.lookup(a->name);
        if (!ptr) return nullptr;
        builder.CreateStore(val, ptr);
        return val;
    }

    // If
    if (auto* i = dynamic_cast<IfAST*>(node)) {
        auto condV = toBool(generate(i->cond.get()));
        if (!condV) return nullptr;
        auto cond = generate(i->cond.get());
        if (cond->getType()->isIntegerTy(32)) {
            auto zero = llvm::ConstantInt::get(context, llvm::APInt(32, 0));
            cond = builder.CreateICmpNE(cond, zero);
        }


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

    // While
    if (auto* w = dynamic_cast<WhileAST*>(node)) {
        auto fn = builder.GetInsertBlock()->getParent();
        auto condBB = llvm::BasicBlock::Create(context,"cond",fn);
        auto loopBB = llvm::BasicBlock::Create(context,"loop");
        auto afterBB = llvm::BasicBlock::Create(context,"after");

        builder.CreateBr(condBB);
        builder.SetInsertPoint(condBB);

        auto cond = generate(w->cond.get());

        if (cond->getType()->isIntegerTy(32)) {
            auto zero = llvm::ConstantInt::get(context, llvm::APInt(32, 0));
            cond = builder.CreateICmpNE(cond, zero);
        }


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

    // Block
    if (auto* b = dynamic_cast<BlockAST*>(node)) {
        symbols.enterScope();
        for (auto& stmt : b->statements)
            generate(stmt.get());
        symbols.exitScope();
        return nullptr;
    }

    // Function definition
    if (auto* f = dynamic_cast<FunctionAST*>(node)) {
        std::vector<llvm::Type*> ints(
            f->proto->args.size(),
            llvm::Type::getInt32Ty(context));

        auto ft = llvm::FunctionType::get(
            llvm::Type::getInt32Ty(context), ints, false);

        auto fn = llvm::Function::Create(
            ft, llvm::Function::ExternalLinkage,
            f->proto->name, module.get());

        auto bb = llvm::BasicBlock::Create(context, "entry", fn);
        builder.SetInsertPoint(bb);

        symbols.enterScope();

        size_t idx = 0;
        for (auto& arg : fn->args()) {
            llvm::IRBuilder<> tmp(
                &fn->getEntryBlock(),
                fn->getEntryBlock().begin()
            );
            auto alloc = tmp.CreateAlloca(
                llvm::Type::getInt32Ty(context)
            );
            builder.CreateStore(&arg, alloc);
            symbols.insert(f->proto->args[idx++], alloc);
        }

        generate(f->body.get());

        symbols.exitScope();

        if (!bb->getTerminator()) {
            builder.CreateRet(
                llvm::ConstantInt::get(context, llvm::APInt(32, 0)));
        }
        return fn;
    }

    // Function call
    if (auto* c = dynamic_cast<CallAST*>(node)) {
        auto fn = module->getFunction(c->callee);
        std::vector<llvm::Value*> args;
        for (auto& a : c->args)
            args.push_back(generate(a.get()));
        return builder.CreateCall(fn, args);
    }

    if (auto* a = dynamic_cast<ArrayDeclAST*>(node)) {
        auto arrType = llvm::ArrayType::get(
            llvm::Type::getInt32Ty(context), a->size);

        auto alloc = builder.CreateAlloca(arrType, nullptr, a->name);
        symbols.insert(a->name, alloc);
        return alloc;
    }

    if (auto *arr = dynamic_cast<ArrayAccessAST *>(node)) {
        llvm::Value *arrayPtr = symbols.lookup(arr->name);
        llvm::Value *index = generate(arr->index.get());

        llvm::Value *zero =
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);

        std::vector<llvm::Value *> idxs = {zero, index};

        auto elemPtr = builder.CreateGEP(
            arrayPtr->getType()->getPointerElementType(),
            arrayPtr, idxs, "arrayidx");

        return builder.CreateLoad(
            llvm::Type::getInt32Ty(context), elemPtr);
    }

    if (auto* b = dynamic_cast<BinaryAST*>(node)) {
        auto L = generate(b->lhs.get());
        auto R = generate(b->rhs.get());
        if (!L || !R) return nullptr;

        // Load if needed
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

    if (auto *p = dynamic_cast<ProgramAST*>(node)) {
        for (auto &fn : p->functions)
            generate(fn.get());
        return nullptr;
    }

    if (auto* r = dynamic_cast<ReturnAST*>(node)) {
    auto val = generate(r->expr.get());

    if (val->getType()->isIntegerTy(1)) {
        val = builder.CreateZExt(
            val,
            llvm::Type::getInt32Ty(context),
            "booltoint"
        );
    }
    return builder.CreateRet(val);
    }

    if (auto* f = dynamic_cast<ForAST*>(node)) {
        generate(f->init.get());

        auto fn = builder.GetInsertBlock()->getParent();
        auto condBB = llvm::BasicBlock::Create(context, "for.cond", fn);
        auto loopBB = llvm::BasicBlock::Create(context, "for.body");
        auto incBB  = llvm::BasicBlock::Create(context, "for.inc");
        auto endBB  = llvm::BasicBlock::Create(context, "for.end");

        builder.CreateBr(condBB);
        builder.SetInsertPoint(condBB);

        auto cond = generate(f->cond.get());
        if (cond->getType()->isIntegerTy(32)) {
            auto zero = llvm::ConstantInt::get(context, llvm::APInt(32, 0));
            cond = builder.CreateICmpNE(cond, zero);
        }
        else{
            cond = builder.CreateICmpNE(cond,llvm::ConstantInt::get(context, llvm::APInt(32,0)));
        }

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
        generate(f->inc.get());
        builder.CreateBr(condBB);

        fn->getBasicBlockList().push_back(endBB);
        builder.SetInsertPoint(endBB);
        return nullptr;
    }

    if (dynamic_cast<BreakAST*>(node))
        return builder.CreateBr(breakStack.back());

    if (dynamic_cast<ContinueAST*>(node))
        return builder.CreateBr(continueStack.back());

    if (auto* l = dynamic_cast<LogicalAST*>(node)) {
        auto L = toBool(generate(l->lhs.get()));
        auto fn = builder.GetInsertBlock()->getParent();

        auto lhsBB = builder.GetInsertBlock();
        auto rhsBB = llvm::BasicBlock::Create(context, "logic.rhs", fn);
        auto mergeBB = llvm::BasicBlock::Create(context, "logic.end", fn);

        if (l->op == "&&")
            builder.CreateCondBr(L, rhsBB, mergeBB);
        else
            builder.CreateCondBr(L, mergeBB, rhsBB);

        builder.SetInsertPoint(rhsBB);
        auto R = toBool(generate(l->rhs.get()));
        builder.CreateBr(mergeBB);

        builder.SetInsertPoint(mergeBB);
        auto phi = builder.CreatePHI(
            llvm::Type::getInt1Ty(context), 2
        );

        if (l->op == "&&") {
            phi->addIncoming(
                llvm::ConstantInt::getFalse(context), lhsBB
            );
        } else {
            phi->addIncoming(
                llvm::ConstantInt::getTrue(context), lhsBB
            );
        }
        phi->addIncoming(R, rhsBB);
        return phi;
    }


    if (auto* u = dynamic_cast<UnaryAST*>(node)) {
        auto val = generate(u->operand.get());
        if (!val) return nullptr;

        if (u->op == "-") {
            return builder.CreateNeg(val, "negtmp");
        }
        if (u->op == "!") {
            auto b = toBool(val);
            return builder.CreateNot(b, "nottmp");
        }
    }

    if (auto* a = dynamic_cast<ArrayAssignAST*>(node)) {
        auto arrayPtr = symbols.lookup(a->name);
        auto idx = generate(a->index.get());
        auto val = generate(a->expr.get());

        auto zero = llvm::ConstantInt::get(
            llvm::Type::getInt32Ty(context), 0);

        std::vector<llvm::Value*> idxs = {zero, idx};

        auto elemPtr = builder.CreateGEP(
            arrayPtr->getType()->getPointerElementType(),
            arrayPtr, idxs);

        builder.CreateStore(val, elemPtr);
        return val;
    }

    return nullptr;
}


void CodeGen::optimize() {
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

    llvm::ModulePassManager MPM =
        PB.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O2);

    MPM.run(*module, MAM);
}

llvm::Value* CodeGen::toBool(llvm::Value* val) {
    if (val->getType()->isIntegerTy(1))
        return val;
    if (val->getType()->isIntegerTy(32))
        return builder.CreateICmpNE(val, llvm::ConstantInt::get(val->getType(), 0));
    // add float support later
    return nullptr; // error
}

void CodeGen::dumpToFile(const std::string &filename) {
    std::error_code EC;
    llvm::raw_fd_ostream out(filename, EC);
    module->print(out, nullptr);
}

void CodeGen::dump() {
    module->print(llvm::outs(), nullptr);
}

