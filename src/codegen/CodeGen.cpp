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

    if (auto* n = dynamic_cast<NumberAST*>(node))
        return llvm::ConstantInt::get(context, llvm::APInt(32, n->val));

    if (auto* b = dynamic_cast<BinaryAST*>(node)) {
        auto L = generate(b->lhs.get());
        auto R = generate(b->rhs.get());
        return builder.CreateAdd(L, R, "addtmp");
    }

    if (auto* v = dynamic_cast<VarDeclAST*>(node)) {
        auto alloc = builder.CreateAlloca(
            llvm::Type::getInt32Ty(context), nullptr, v->name);
        symbols.insert(v->name, alloc);
        return alloc;
    }

    // Assignment
    if (auto* a = dynamic_cast<AssignAST*>(node)) {
        auto val = generate(a->expr.get());
        auto var = symbols.lookup(a->name);
        builder.CreateStore(val, var);
        return val;
    }

    // If
    if (auto* i = dynamic_cast<IfAST*>(node)) {
        auto cond = generate(i->cond.get());
        cond = builder.CreateICmpNE(
            cond, llvm::ConstantInt::get(context, llvm::APInt(32,0)));

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
        cond = builder.CreateICmpNE(
            cond, llvm::ConstantInt::get(context, llvm::APInt(32,0)));
        builder.CreateCondBr(cond, loopBB, afterBB);

        fn->getBasicBlockList().push_back(loopBB);
        builder.SetInsertPoint(loopBB);
        generate(w->body.get());
        builder.CreateBr(condBB);

        fn->getBasicBlockList().push_back(afterBB);
        builder.SetInsertPoint(afterBB);
        return nullptr;
    }

    // Block
    if (auto* b = dynamic_cast<BlockAST*>(node)) {
        for (auto& stmt : b->statements)
            generate(stmt.get());
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

        size_t idx = 0;
        for (auto& arg : fn->args()) {
            auto alloc = builder.CreateAlloca(
                llvm::Type::getInt32Ty(context));
            builder.CreateStore(&arg, alloc);
            symbols.insert(f->proto->args[idx++], alloc);
        }

        generate(f->body.get());
        builder.CreateRet(
            llvm::ConstantInt::get(context, llvm::APInt(32, 0)));

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
        llvm::Value *arrayPtr = namedValues[arr->name];
        llvm::Value *index = generate(arr->index.get());

        llvm::Value *zero =
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);

        std::vector<llvm::Value *> idxs = {zero, index};

        llvm::Type *elemType =
            arrayPtr->getType()->getPointerElementType();

        return builder.CreateGEP(elemType, arrayPtr, idxs, "arrayidx");
    }


    if (auto* b = dynamic_cast<BinaryAST*>(node)) {
        auto L = generate(b->lhs.get());
        auto R = generate(b->rhs.get());

        switch (b->op) {
            case '<': return builder.CreateICmpSLT(L, R);
            case '>': return builder.CreateICmpSGT(L, R);
            case '=': return builder.CreateICmpEQ(L, R);
        }
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

void CodeGen::dump() {
    module->print(llvm::outs(), nullptr);
}
