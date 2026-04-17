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
      module(std::make_unique<llvm::Module>("quail", context)),
      currentThisAlloca(nullptr) {}

// ── Error helper ──────────────────────────────────────────────
void CodeGen::addError(const std::string& msg) {
    for (auto& e : errors) if (e.message == msg) return;
    errors.push_back({msg});
}

// ══════════════════════════════════════════════════════════════
//  Type helpers
// ══════════════════════════════════════════════════════════════

llvm::Type* CodeGen::llvmType(ASTType t) {
    switch (t) {
        case ASTType::Float:   return llvm::Type::getDoubleTy(context);
        case ASTType::Void:    return llvm::Type::getVoidTy(context);
        case ASTType::Int:
        default:               return llvm::Type::getInt32Ty(context);
    }
}

llvm::Type* CodeGen::llvmType(ValueType t) {
    switch (t) {
        case ValueType::Float:  return llvm::Type::getDoubleTy(context);
        case ValueType::Void:   return llvm::Type::getVoidTy(context);
        case ValueType::Int:
        default:                return llvm::Type::getInt32Ty(context);
    }
}

static ValueType astToValueType(ASTType t) {
    switch (t) {
        case ASTType::Float:   return ValueType::Float;
        case ASTType::Void:    return ValueType::Void;
        case ASTType::Int:
        default:               return ValueType::Int;
    }
}

llvm::Value* CodeGen::coerce(llvm::Value* val, llvm::Type* targetTy) {
    if (!val || !targetTy) return val;
    llvm::Type* srcTy = val->getType();
    if (srcTy == targetTy) return val;
    if (srcTy->isIntegerTy(1) && targetTy->isIntegerTy(32))
        return builder.CreateZExt(val, targetTy, "bool_to_int");
    if (srcTy->isIntegerTy(1) && targetTy->isDoubleTy())
        return builder.CreateUIToFP(val, targetTy, "bool_to_fp");
    if (srcTy->isIntegerTy(32) && targetTy->isDoubleTy())
        return builder.CreateSIToFP(val, targetTy, "int_to_fp");
    if (srcTy->isDoubleTy() && targetTy->isIntegerTy(32))
        return builder.CreateFPToSI(val, targetTy, "fp_to_int");
    if (srcTy->isIntegerTy(32) && targetTy->isIntegerTy(1))
        return builder.CreateICmpNE(val,
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0), "int_to_bool");
    // pointer compatibility — silently accept
    if (srcTy->isPointerTy() && targetTy->isPointerTy()) return val;
    addError("Type mismatch: cannot coerce types");
    return val;
}

std::pair<llvm::Value*, llvm::Value*>
CodeGen::promoteToCommon(llvm::Value* lhs, llvm::Value* rhs) {
    if (!lhs || !rhs) return {lhs, rhs};
    auto* i32 = llvm::Type::getInt32Ty(context);
    auto* f64 = llvm::Type::getDoubleTy(context);
    if (lhs->getType()->isIntegerTy(1)) lhs = builder.CreateZExt(lhs, i32);
    if (rhs->getType()->isIntegerTy(1)) rhs = builder.CreateZExt(rhs, i32);
    bool lhsFloat = lhs->getType()->isDoubleTy();
    bool rhsFloat = rhs->getType()->isDoubleTy();
    if (lhsFloat && !rhsFloat) rhs = builder.CreateSIToFP(rhs, f64, "promote_rhs");
    if (!lhsFloat && rhsFloat) lhs = builder.CreateSIToFP(lhs, f64, "promote_lhs");
    return {lhs, rhs};
}

std::string CodeGen::getIRString() const {
    std::string s;
    llvm::raw_string_ostream os(s);
    module->print(os, nullptr);
    return s;
}

void CodeGen::collectStats(OptStats::FuncStat& fs, llvm::Function& fn, bool before) {
    size_t instrs = 0, blocks = 0;
    for (auto& bb : fn) { ++blocks; for (auto& i : bb) { (void)i; ++instrs; } }
    if (before) { fs.instrBefore  = instrs; fs.blocksBefore = blocks; }
    else        { fs.instrAfter   = instrs; fs.blocksAfter  = blocks; }
}

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

// ══════════════════════════════════════════════════════════════
//  Runtime function declarations
// ══════════════════════════════════════════════════════════════

llvm::Function* CodeGen::getOrDeclarePrintf() {
    if (printfFunc) return printfFunc;
    if (auto* f = module->getFunction("printf")) { printfFunc = f; return f; }
    auto* i8PtrTy = llvm::Type::getInt8PtrTy(context);
    auto* i32Ty   = llvm::Type::getInt32Ty(context);
    auto* ft      = llvm::FunctionType::get(i32Ty, {i8PtrTy}, true);
    printfFunc    = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                                           "printf", *module);
    printfFunc->addParamAttr(0, llvm::Attribute::NoCapture);
    return printfFunc;
}

llvm::Function* CodeGen::getOrDeclareScanf() {
    if (scanfFunc) return scanfFunc;
    if (auto* f = module->getFunction("scanf")) { scanfFunc = f; return f; }
    auto* i8PtrTy = llvm::Type::getInt8PtrTy(context);
    auto* i32Ty   = llvm::Type::getInt32Ty(context);
    auto* ft      = llvm::FunctionType::get(i32Ty, {i8PtrTy}, true);
    scanfFunc     = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                                           "scanf", *module);
    scanfFunc->addParamAttr(0, llvm::Attribute::NoCapture);
    return scanfFunc;
}

llvm::Function* CodeGen::getOrDeclareMalloc() {
    if (mallocFunc) return mallocFunc;
    if (auto* f = module->getFunction("malloc")) { mallocFunc = f; return f; }
    auto* i8PtrTy = llvm::Type::getInt8PtrTy(context);
    auto* i64Ty   = llvm::Type::getInt64Ty(context);
    auto* ft      = llvm::FunctionType::get(i8PtrTy, {i64Ty}, false);
    mallocFunc    = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                                           "malloc", *module);
    return mallocFunc;
}

llvm::Function* CodeGen::getOrDeclareFree() {
    if (freeFunc) return freeFunc;
    if (auto* f = module->getFunction("free")) { freeFunc = f; return f; }
    auto* i8PtrTy = llvm::Type::getInt8PtrTy(context);
    auto* voidTy  = llvm::Type::getVoidTy(context);
    auto* ft      = llvm::FunctionType::get(voidTy, {i8PtrTy}, false);
    freeFunc      = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                                           "free", *module);
    return freeFunc;
}

llvm::Function* CodeGen::getOrDeclareStrlen() {
    if (strlenFunc) return strlenFunc;
    if (auto* f = module->getFunction("strlen")) { strlenFunc = f; return f; }
    auto* i8PtrTy = llvm::Type::getInt8PtrTy(context);
    auto* i64Ty   = llvm::Type::getInt64Ty(context);
    auto* ft      = llvm::FunctionType::get(i64Ty, {i8PtrTy}, false);
    strlenFunc    = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                                           "strlen", *module);
    return strlenFunc;
}

llvm::Function* CodeGen::getOrDeclareStrcmp() {
    if (strcmpFunc) return strcmpFunc;
    if (auto* f = module->getFunction("strcmp")) { strcmpFunc = f; return f; }
    auto* i8PtrTy = llvm::Type::getInt8PtrTy(context);
    auto* i32Ty   = llvm::Type::getInt32Ty(context);
    auto* ft      = llvm::FunctionType::get(i32Ty, {i8PtrTy, i8PtrTy}, false);
    strcmpFunc    = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                                           "strcmp", *module);
    return strcmpFunc;
}

llvm::Function* CodeGen::getOrDeclareStrcat() {
    if (strcatFunc) return strcatFunc;
    if (auto* f = module->getFunction("strcat")) { strcatFunc = f; return f; }
    auto* i8PtrTy = llvm::Type::getInt8PtrTy(context);
    auto* ft      = llvm::FunctionType::get(i8PtrTy, {i8PtrTy, i8PtrTy}, false);
    strcatFunc    = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                                           "strcat", *module);
    return strcatFunc;
}

llvm::Function* CodeGen::getOrDeclareStrcpy() {
    if (strcpyFunc) return strcpyFunc;
    if (auto* f = module->getFunction("strcpy")) { strcpyFunc = f; return f; }
    auto* i8PtrTy = llvm::Type::getInt8PtrTy(context);
    auto* ft      = llvm::FunctionType::get(i8PtrTy, {i8PtrTy, i8PtrTy}, false);
    strcpyFunc    = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                                           "strcpy", *module);
    return strcpyFunc;
}

llvm::Value* CodeGen::buildFmtPtr(const std::string& fmt) {
    auto it = fmtCache.find(fmt);
    if (it != fmtCache.end()) {
        auto* gv   = it->second;
        auto* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);
        return builder.CreateGEP(gv->getValueType(), gv, {zero, zero}, ".fmtptr");
    }
    auto* strConst = llvm::ConstantDataArray::getString(context, fmt);
    auto* gv = new llvm::GlobalVariable(*module, strConst->getType(),
        true, llvm::GlobalValue::PrivateLinkage, strConst, ".fmt");
    gv->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
    gv->setAlignment(llvm::MaybeAlign(1));
    fmtCache[fmt] = gv;
    auto* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);
    return builder.CreateGEP(gv->getValueType(), gv, {zero, zero}, ".fmtptr");
}

// ══════════════════════════════════════════════════════════════
//  OOP helpers
// ══════════════════════════════════════════════════════════════

llvm::Value* CodeGen::fieldGEPFromPtr(llvm::StructType* structTy,
                                       llvm::Value*      objPtr,
                                       int               fieldIdx,
                                       const std::string& tag)
{
    auto* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);
    auto* fidx = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), fieldIdx);
    return builder.CreateGEP(structTy, objPtr, {zero, fidx}, tag);
}

llvm::Value* CodeGen::fieldGEP(const Symbol* sym,
                                const std::string& fieldName,
                                const std::string& tag)
{
    auto it = classInfos.find(sym->objectClass);
    if (it == classInfos.end()) {
        addError("Unknown class '" + sym->objectClass + "'");
        return nullptr;
    }
    int idx = it->second.fieldIndex(fieldName);
    if (idx < 0) {
        addError("Class '" + sym->objectClass + "' has no field '" + fieldName + "'");
        return nullptr;
    }
    return fieldGEPFromPtr(it->second.llvmType, sym->value, idx,
                           tag.empty() ? sym->name + "." + fieldName : tag);
}

void CodeGen::generateMethod(FunctionAST* f,
                              const std::string& className,
                              llvm::StructType*  structTy)
{
    auto* structPtrTy = llvm::PointerType::get(structTy, 0);

    std::vector<llvm::Type*> paramTypes;
    std::vector<ValueType>   paramVT;
    paramTypes.push_back(structPtrTy);
    paramVT.push_back(ValueType::Unknown);

    for (size_t i = 0; i < f->proto->args.size(); ++i) {
        ASTType at = (i < f->proto->argTypes.size())
                     ? f->proto->argTypes[i] : ASTType::Int;
        paramTypes.push_back(llvmType(at));
        paramVT.push_back(astToValueType(at));
    }

    llvm::Type* retTy       = llvmType(f->proto->returnType);
    auto*       ft          = llvm::FunctionType::get(retTy, paramTypes, false);
    std::string mangledName = className + "_" + f->proto->name;

    auto* fn = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                                      mangledName, *module);

    std::vector<ValueType> regParams(paramVT.begin() + 1, paramVT.end());
    try { symbols.insertFunction(mangledName, astToValueType(f->proto->returnType),
                                 regParams, fn); }
    catch (...) {}

    auto* entry = llvm::BasicBlock::Create(context, "entry", fn);
    builder.SetInsertPoint(entry);
    symbols.enterScope();
    symbols.setCurrentFunction(mangledName);

    auto argIt = fn->args().begin();
    argIt->setName("this_arg");
    auto* thisPtrAlloca = builder.CreateAlloca(structPtrTy, nullptr, "this.addr");
    builder.CreateStore(&*argIt, thisPtrAlloca);
    currentThisAlloca = thisPtrAlloca;
    ++argIt;

    size_t idx = 0;
    for (auto it = argIt; it != fn->args().end(); ++it, ++idx) {
        if (idx >= f->proto->args.size()) {
            addError("generateMethod '" + mangledName + "': arg count mismatch"); break;
        }
        const std::string& pname = f->proto->args[idx];
        ASTType at = (idx < f->proto->argTypes.size())
                     ? f->proto->argTypes[idx] : ASTType::Int;
        auto* alloc = builder.CreateAlloca(llvmType(at), nullptr, pname);
        builder.CreateStore(&*it, alloc);
        try { symbols.insert(pname, astToValueType(at), SymbolKind::Parameter, alloc); }
        catch (const std::runtime_error& e) { addError(e.what()); }
    }

    generate(f->body.get());

    if (!builder.GetInsertBlock()->getTerminator()) {
        if (retTy->isVoidTy())        builder.CreateRetVoid();
        else if (retTy->isDoubleTy()) builder.CreateRet(llvm::ConstantFP::get(retTy, 0.0));
        else                          builder.CreateRet(llvm::ConstantInt::get(retTy, 0));
    }

    currentThisAlloca = nullptr;
    symbols.clearCurrentFunction();
    symbols.exitScope();

    std::string errStr;
    llvm::raw_string_ostream errStream(errStr);
    if (llvm::verifyFunction(*fn, &errStream))
        addError("IR verify failed for '" + mangledName + "': " + errStream.str());
}

void CodeGen::dumpToFile(const std::string& filename) {
    std::error_code EC;
    llvm::raw_fd_ostream out(filename, EC);
    if (EC) { addError("Cannot write '" + filename + "': " + EC.message()); return; }
    module->print(out, nullptr);
}

void CodeGen::dump() { module->print(llvm::outs(), nullptr); }

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
llvm::Value* CodeGen::generate(AST* node) {
    if (!node) { addError("[CodeGen] Internal: null AST node"); return nullptr; }

    // ── Comments ───────────────────────────────────────────────
    if (dynamic_cast<LineCommentAST*>(node))  return nullptr;
    if (dynamic_cast<BlockCommentAST*>(node)) return nullptr;

    // ════════════════════════════════════════════════════════════
    //  String literal
    // ════════════════════════════════════════════════════════════
    if (auto* str = dynamic_cast<StringAST*>(node))
        return builder.CreateGlobalStringPtr(str->value, ".str");

    // ════════════════════════════════════════════════════════════
    //  String type — variable declarations and built-in operations
    // ════════════════════════════════════════════════════════════

    if (auto* sv = dynamic_cast<StringVarDeclAST*>(node)) {
        if (symbols.isDeclaredInCurrentScope(sv->name)) {
            addError("Redeclaration of string '" + sv->name + "' in same scope");
            return nullptr;
        }
        auto* i8PtrTy = llvm::Type::getInt8PtrTy(context);
        auto* alloc   = builder.CreateAlloca(i8PtrTy, nullptr, sv->name);
        if (sv->init) {
            auto* initVal = generate(sv->init.get());
            if (!initVal) return nullptr;
            if (!initVal->getType()->isPointerTy())
                initVal = builder.CreateIntToPtr(initVal, i8PtrTy);
            builder.CreateStore(initVal, alloc);
        } else {
            builder.CreateStore(
                llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(i8PtrTy)),
                alloc);
        }
        try { symbols.insert(sv->name, ValueType::Unknown, SymbolKind::Pointer, alloc); }
        catch (const std::runtime_error& e) { addError(e.what()); return nullptr; }
        return alloc;
    }

    if (auto* sl = dynamic_cast<StrLenAST*>(node)) {
        Symbol* sym = symbols.lookup(sl->varName);
        if (!sym) { addError("strlen: undeclared variable '" + sl->varName + "'"); return nullptr; }
        auto* i8PtrTy = llvm::Type::getInt8PtrTy(context);
        auto* strPtr  = builder.CreateLoad(i8PtrTy, sym->value, sl->varName);
        auto* len64   = builder.CreateCall(getOrDeclareStrlen(), {strPtr}, "strlen_result");
        return builder.CreateTrunc(len64, llvm::Type::getInt32Ty(context), "strlen_i32");
    }

    if (auto* sc = dynamic_cast<StrCmpAST*>(node)) {
        auto* lhsVal = generate(sc->lhs.get());
        auto* rhsVal = generate(sc->rhs.get());
        if (!lhsVal || !rhsVal) return nullptr;
        return builder.CreateCall(getOrDeclareStrcmp(), {lhsVal, rhsVal}, "strcmp_result");
    }

    if (auto* sc = dynamic_cast<StrCatAST*>(node)) {
        Symbol* destSym = symbols.lookup(sc->dest);
        Symbol* srcSym  = symbols.lookup(sc->src);
        if (!destSym) { addError("strcat: undeclared dest '" + sc->dest + "'"); return nullptr; }
        if (!srcSym)  { addError("strcat: undeclared src '"  + sc->src  + "'"); return nullptr; }
        auto* i8PtrTy = llvm::Type::getInt8PtrTy(context);
        auto* destPtr = builder.CreateLoad(i8PtrTy, destSym->value, sc->dest);
        auto* srcPtr  = builder.CreateLoad(i8PtrTy, srcSym->value,  sc->src);
        return builder.CreateCall(getOrDeclareStrcat(), {destPtr, srcPtr}, "strcat_result");
    }

    // ════════════════════════════════════════════════════════════
    //  Heap allocation — new / delete
    // ════════════════════════════════════════════════════════════

    if (auto* ne = dynamic_cast<NewExprAST*>(node)) {
        auto it = classTypes.find(ne->className);
        if (it == classTypes.end()) {
            addError("'new': unknown class '" + ne->className + "'"); return nullptr;
        }
        llvm::StructType* structTy = it->second;
        auto* nullPtr  = llvm::ConstantPointerNull::get(llvm::PointerType::get(structTy, 0));
        auto* one      = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 1);
        auto* sizeGEP  = builder.CreateGEP(structTy, nullPtr, {one}, "sizeof_gep");
        auto* sizeVal  = builder.CreatePtrToInt(sizeGEP,
                             llvm::Type::getInt64Ty(context), "sizeof");
        auto* rawPtr   = builder.CreateCall(getOrDeclareMalloc(), {sizeVal}, "heap_obj");
        auto* structPtr = builder.CreateBitCast(rawPtr,
            llvm::PointerType::get(structTy, 0), ne->className + "_ptr");
        builder.CreateStore(llvm::Constant::getNullValue(structTy), structPtr);
        return structPtr;
    }

    if (auto* ns = dynamic_cast<NewScalarAST*>(node)) {
        llvm::Type* ty    = llvmType(ns->type);
        auto* nullPtr     = llvm::ConstantPointerNull::get(llvm::PointerType::get(ty, 0));
        auto* one         = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 1);
        auto* sizeGEP     = builder.CreateGEP(ty, nullPtr, {one}, "sizeof_gep");
        auto* sizeVal     = builder.CreatePtrToInt(sizeGEP,
                                llvm::Type::getInt64Ty(context), "sizeof");
        auto* rawPtr      = builder.CreateCall(getOrDeclareMalloc(), {sizeVal}, "heap_scalar");
        auto* typedPtr    = builder.CreateBitCast(rawPtr,
            llvm::PointerType::get(ty, 0), "scalar_ptr");
        builder.CreateStore(llvm::Constant::getNullValue(ty), typedPtr);
        return typedPtr;
    }

    if (auto* del = dynamic_cast<DeleteAST*>(node)) {
        Symbol* sym = symbols.lookup(del->ptrName);
        if (!sym) { addError("delete: undeclared pointer '" + del->ptrName + "'"); return nullptr; }
        // sym->value is the alloca holding the pointer; load it first
        llvm::Type* storedTy = nullptr;
        if (sym->kind == SymbolKind::Pointer)
            storedTy = llvm::PointerType::get(llvmType(sym->type), 0);
        else
            storedTy = llvm::Type::getInt8PtrTy(context);
        auto* ptrVal = builder.CreateLoad(storedTy, sym->value, del->ptrName);
        auto* i8Ptr  = builder.CreateBitCast(ptrVal, llvm::Type::getInt8PtrTy(context));
        builder.CreateCall(getOrDeclareFree(), {i8Ptr});
        return nullptr;
    }

    // ════════════════════════════════════════════════════════════
    //  Pointers — address-of, dereference, arrow access/assign
    // ════════════════════════════════════════════════════════════

    if (auto* ao = dynamic_cast<AddressOfAST*>(node)) {
        Symbol* sym = symbols.lookup(ao->name);
        if (!sym) { addError("Address-of undeclared variable '" + ao->name + "'"); return nullptr; }
        return sym->value;  // alloca IS the pointer
    }

    if (auto* dr = dynamic_cast<DerefAST*>(node)) {
        auto* ptrVal = generate(dr->operand.get());
        if (!ptrVal) return nullptr;
        if (!ptrVal->getType()->isPointerTy()) {
            addError("Cannot dereference a non-pointer value"); return nullptr;
        }
        // Use i32 as default pointee type
        auto* elemTy = llvm::Type::getInt32Ty(context);
        return builder.CreateLoad(elemTy, ptrVal, "deref");
    }

    if (auto* da = dynamic_cast<DerefAssignAST*>(node)) {
        auto* ptrVal = generate(da->ptr.get());
        if (!ptrVal) return nullptr;
        if (!ptrVal->getType()->isPointerTy()) {
            addError("Left side of dereference assignment is not a pointer"); return nullptr;
        }
        auto* val = generate(da->expr.get());
        if (!val) return nullptr;
        builder.CreateStore(val, ptrVal);
        return val;
    }

    if (auto* pdi = dynamic_cast<PtrVarDeclInitAST*>(node)) {
        if (symbols.isDeclaredInCurrentScope(pdi->name)) {
            addError("Redeclaration of '" + pdi->name + "' in same scope"); return nullptr;
        }
        llvm::Type* pointeeTy = llvmType(pdi->baseType);
        llvm::Type* ptrTy     = llvm::PointerType::get(pointeeTy, 0);
        auto* alloc = builder.CreateAlloca(ptrTy, nullptr, pdi->name);
        try { symbols.insert(pdi->name, astToValueType(pdi->baseType),
                             SymbolKind::Pointer, alloc); }
        catch (const std::runtime_error& e) { addError(e.what()); return nullptr; }
        auto* initVal = generate(pdi->init.get());
        if (!initVal) return nullptr;
        if (!initVal->getType()->isPointerTy()) {
            addError("Initializer of pointer '" + pdi->name + "' is not a pointer");
            return nullptr;
        }
        builder.CreateStore(initVal, alloc);
        return alloc;
    }

    if (auto* pd = dynamic_cast<PtrVarDeclAST*>(node)) {
        if (symbols.isDeclaredInCurrentScope(pd->name)) {
            addError("Redeclaration of '" + pd->name + "' in same scope"); return nullptr;
        }
        llvm::Type* pointeeTy = llvmType(pd->baseType);
        llvm::Type* ptrTy     = llvm::PointerType::get(pointeeTy, 0);
        auto* alloc = builder.CreateAlloca(ptrTy, nullptr, pd->name);
        builder.CreateStore(
            llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptrTy)), alloc);
        try { symbols.insert(pd->name, astToValueType(pd->baseType),
                             SymbolKind::Pointer, alloc); }
        catch (const std::runtime_error& e) { addError(e.what()); }
        return alloc;
    }

    if (auto* aa = dynamic_cast<ArrowAccessAST*>(node)) {
        Symbol* sym = symbols.lookup(aa->objPtrName);
        if (!sym) { addError("Use of undeclared pointer '" + aa->objPtrName + "'"); return nullptr; }
        auto cit = classInfos.find(sym->objectClass);
        if (cit == classInfos.end()) {
            addError("Cannot determine class type for pointer '" + aa->objPtrName + "'");
            return nullptr;
        }
        auto* structPtrTy = llvm::PointerType::get(cit->second.llvmType, 0);
        auto* objPtr = builder.CreateLoad(structPtrTy, sym->value, aa->objPtrName);
        int idx = cit->second.fieldIndex(aa->memberName);
        if (idx < 0) {
            addError("Class '" + cit->first + "' has no field '" + aa->memberName + "'");
            return nullptr;
        }
        auto* gep = fieldGEPFromPtr(cit->second.llvmType, objPtr, idx,
                                    aa->objPtrName + "->" + aa->memberName + ".ptr");
        ValueType ft = cit->second.fieldType(aa->memberName);
        return builder.CreateLoad(llvmType(ft), gep, aa->memberName);
    }

    if (auto* aa = dynamic_cast<ArrowAssignAST*>(node)) {
        Symbol* sym = symbols.lookup(aa->objPtrName);
        if (!sym) { addError("Assignment through undeclared pointer '" + aa->objPtrName + "'"); return nullptr; }
        auto cit = classInfos.find(sym->objectClass);
        if (cit == classInfos.end()) {
            addError("Cannot determine class type for pointer '" + aa->objPtrName + "'");
            return nullptr;
        }
        auto* structPtrTy = llvm::PointerType::get(cit->second.llvmType, 0);
        auto* objPtr = builder.CreateLoad(structPtrTy, sym->value, aa->objPtrName);
        int idx = cit->second.fieldIndex(aa->memberName);
        if (idx < 0) {
            addError("Class '" + cit->first + "' has no field '" + aa->memberName + "'");
            return nullptr;
        }
        auto* val = generate(aa->expr.get());
        if (!val) return nullptr;
        ValueType ft = cit->second.fieldType(aa->memberName);
        val = coerce(val, llvmType(ft));
        auto* gep = fieldGEPFromPtr(cit->second.llvmType, objPtr, idx,
                                    aa->objPtrName + "->" + aa->memberName + ".ptr");
        builder.CreateStore(val, gep);
        return val;
    }

    // ════════════════════════════════════════════════════════════
    //  print / println
    // ════════════════════════════════════════════════════════════
    if (auto* pr = dynamic_cast<PrintAST*>(node)) {
        auto* printfFn = getOrDeclarePrintf();
        if (!printfFn) return nullptr;
        auto* i8PtrTy = llvm::Type::getInt8PtrTy(context);

        for (size_t i = 0; i < pr->exprs.size(); ++i) {
            bool isStringLit = (dynamic_cast<StringAST*>(pr->exprs[i].get()) != nullptr);
            auto* val = generate(pr->exprs[i].get());
            if (!val) continue;

            llvm::Type* ty = val->getType();
            llvm::Value* fmtPtr = nullptr;
            std::vector<llvm::Value*> callArgs;

            if (isStringLit || ty == i8PtrTy || ty->isPointerTy()) {
                fmtPtr = buildFmtPtr("%s");
                callArgs = {fmtPtr, val};
            } else if (ty->isDoubleTy()) {
                fmtPtr = buildFmtPtr("%g");
                callArgs = {fmtPtr, val};
            } else {
                if (!ty->isIntegerTy(32))
                    val = coerce(val, llvm::Type::getInt32Ty(context));
                fmtPtr = buildFmtPtr("%d");
                callArgs = {fmtPtr, val};
            }
            builder.CreateCall(printfFn, callArgs);
        }

        if (pr->newline)
            builder.CreateCall(printfFn, {buildFmtPtr("\n")});

        return nullptr;
    }

    // ════════════════════════════════════════════════════════════
    //  scan
    // ════════════════════════════════════════════════════════════
    if (auto* sc = dynamic_cast<ScanAST*>(node)) {
        auto* scanfFn = getOrDeclareScanf();
        if (!scanfFn) return nullptr;

        for (const auto& varName : sc->varNames) {
            Symbol* sym = symbols.lookup(varName);
            if (!sym) { addError("scan: use of undeclared variable '" + varName + "'"); continue; }
            if (sym->kind == SymbolKind::Function) {
                addError("scan: '" + varName + "' is a function"); continue;
            }
            if (sym->kind == SymbolKind::Object) {
                addError("scan: cannot read directly into object '" + varName + "'"); continue;
            }
            bool isFloat = (sym->type == ValueType::Float);
            auto* fmtPtr = buildFmtPtr(isFloat ? "%lf" : "%d");
            builder.CreateCall(scanfFn, {fmtPtr, sym->value});
        }
        return nullptr;
    }

    // ════════════════════════════════════════════════════════════
    //  OOP — Class declaration (with inheritance)
    // ════════════════════════════════════════════════════════════
    if (auto* cls = dynamic_cast<ClassDeclAST*>(node)) {
        ClassInfo info;
        info.name       = cls->name;
        info.parentName = cls->parentName;

        std::vector<llvm::Type*> fieldLLVMTypes;

        // Inherit parent fields
        if (cls->hasParent()) {
            auto pit = classInfos.find(cls->parentName);
            if (pit == classInfos.end()) {
                addError("Class '" + cls->name + "' extends unknown class '"
                         + cls->parentName + "'");
                return nullptr;
            }
            for (auto& [fname, ftype] : pit->second.allFields) {
                fieldLLVMTypes.push_back(llvmType(ftype));
                info.allFields.push_back({fname, ftype});
            }
        }

        // Own fields
        for (auto& f : cls->fields) {
            fieldLLVMTypes.push_back(llvmType(f.type));
            info.fields.push_back({f.name, astToValueType(f.type)});
            info.allFields.push_back({f.name, astToValueType(f.type)});
        }

        auto* structTy       = llvm::StructType::create(context, fieldLLVMTypes, cls->name);
        info.llvmType        = structTy;
        classTypes[cls->name] = structTy;
        classInfos[cls->name] = info;

        // Inherit parent methods via thin wrapper shims
        if (cls->hasParent()) {
            auto pit = classInfos.find(cls->parentName);
            if (pit != classInfos.end()) {
                std::string parentPrefix = cls->parentName + "_";
                for (auto& fn : *module) {
                    std::string fnName = fn.getName().str();
                    if (fnName.size() <= parentPrefix.size()) continue;
                    if (fnName.substr(0, parentPrefix.size()) != parentPrefix) continue;
                    std::string methodName = fnName.substr(parentPrefix.size());
                    std::string childName  = cls->name + "_" + methodName;
                    if (module->getFunction(childName)) continue;

                    auto* parentFn    = &fn;
                    auto* parentFnTy  = parentFn->getFunctionType();

                    std::vector<llvm::Type*> newParams;
                    newParams.push_back(llvm::PointerType::get(structTy, 0));
                    for (size_t pi = 1; pi < parentFnTy->getNumParams(); ++pi)
                        newParams.push_back(parentFnTy->getParamType(pi));

                    auto* newFnTy = llvm::FunctionType::get(
                        parentFnTy->getReturnType(), newParams, false);
                    auto* wrapper = llvm::Function::Create(
                        newFnTy, llvm::Function::ExternalLinkage, childName, *module);

                    auto* entry   = llvm::BasicBlock::Create(context, "entry", wrapper);
                    auto  savedIP = builder.saveIP();
                    builder.SetInsertPoint(entry);

                    auto* parentPtrTy = llvm::PointerType::get(pit->second.llvmType, 0);
                    auto* argIt       = wrapper->arg_begin();
                    auto* castedThis  = builder.CreateBitCast(&*argIt, parentPtrTy, "upcast");
                    ++argIt;

                    std::vector<llvm::Value*> callArgs = {castedThis};
                    for (auto it = argIt; it != wrapper->arg_end(); ++it)
                        callArgs.push_back(&*it);

                    if (parentFnTy->getReturnType()->isVoidTy()) {
                        builder.CreateCall(parentFn, callArgs);
                        builder.CreateRetVoid();
                    } else {
                        auto* result = builder.CreateCall(parentFn, callArgs, "inherited");
                        builder.CreateRet(result);
                    }
                    builder.restoreIP(savedIP);
                }
            }
        }

        // Generate own methods
        currentClassName = cls->name;
        for (auto& method : cls->methods)
            generateMethod(method.get(), cls->name, structTy);
        currentClassName.clear();

        return nullptr;
    }

    // ════════════════════════════════════════════════════════════
    //  OOP — Object declaration
    // ════════════════════════════════════════════════════════════
    if (auto* od = dynamic_cast<ObjectDeclAST*>(node)) {
        auto it = classTypes.find(od->className);
        if (it == classTypes.end()) {
            addError("Unknown class '" + od->className + "'"); return nullptr;
        }
        if (symbols.isDeclaredInCurrentScope(od->varName)) {
            addError("Redeclaration of '" + od->varName + "' in same scope"); return nullptr;
        }
        auto* alloc = builder.CreateAlloca(it->second, nullptr, od->varName);
        builder.CreateStore(llvm::Constant::getNullValue(it->second), alloc);
        try { symbols.insert(od->varName, ValueType::Unknown, SymbolKind::Object,
                             alloc, 0, od->className); }
        catch (const std::runtime_error& e) { addError(e.what()); }
        return alloc;
    }

    // ════════════════════════════════════════════════════════════
    //  OOP — Member access / assign / method call / this / super
    // ════════════════════════════════════════════════════════════
    if (auto* ma = dynamic_cast<MemberAccessAST*>(node)) {
        const Symbol* sym = symbols.lookup(ma->objName);
        if (!sym) { addError("Use of undeclared object '" + ma->objName + "'"); return nullptr; }
        if (sym->kind != SymbolKind::Object) {
            addError("'" + ma->objName + "' is not an object"); return nullptr;
        }
        auto it = classInfos.find(sym->objectClass);
        if (it == classInfos.end()) { addError("Unknown class '" + sym->objectClass + "'"); return nullptr; }
        int idx = it->second.fieldIndex(ma->memberName);
        if (idx < 0) {
            addError("'" + sym->objectClass + "' has no field '" + ma->memberName + "'");
            return nullptr;
        }
        auto* gep = fieldGEPFromPtr(it->second.llvmType, sym->value, idx,
                                    ma->objName + "." + ma->memberName + ".ptr");
        ValueType ft = it->second.fieldType(ma->memberName);
        return builder.CreateLoad(llvmType(ft), gep, ma->memberName);
    }

    if (auto* ma = dynamic_cast<MemberAssignAST*>(node)) {
        const Symbol* sym = symbols.lookup(ma->objName);
        if (!sym) { addError("Assignment to undeclared object '" + ma->objName + "'"); return nullptr; }
        if (sym->kind != SymbolKind::Object) {
            addError("'" + ma->objName + "' is not an object"); return nullptr;
        }
        auto it = classInfos.find(sym->objectClass);
        if (it == classInfos.end()) { addError("Unknown class '" + sym->objectClass + "'"); return nullptr; }
        int idx = it->second.fieldIndex(ma->memberName);
        if (idx < 0) {
            addError("'" + sym->objectClass + "' has no field '" + ma->memberName + "'");
            return nullptr;
        }
        auto* val = generate(ma->expr.get());
        if (!val) return nullptr;
        ValueType ft = it->second.fieldType(ma->memberName);
        val = coerce(val, llvmType(ft));
        auto* gep = fieldGEPFromPtr(it->second.llvmType, sym->value, idx,
                                    ma->objName + "." + ma->memberName + ".ptr");
        builder.CreateStore(val, gep);
        return val;
    }

    if (auto* mc = dynamic_cast<MethodCallAST*>(node)) {
        std::string className;
        llvm::Value* thisPtr = nullptr;

        if (mc->objName == "this") {
            if (currentClassName.empty() || !currentThisAlloca) {
                addError("'this' method call outside of a method"); return nullptr;
            }
            className = currentClassName;
            auto* structPtrTy = llvm::PointerType::get(classTypes[className], 0);
            thisPtr = builder.CreateLoad(structPtrTy, currentThisAlloca, "this");
        } else {
            const Symbol* sym = symbols.lookup(mc->objName);
            if (!sym) { addError("Use of undeclared object '" + mc->objName + "'"); return nullptr; }
            if (sym->kind != SymbolKind::Object && sym->kind != SymbolKind::Pointer) {
                addError("'" + mc->objName + "' is not an object"); return nullptr;
            }
            className = sym->objectClass;
            if (sym->kind == SymbolKind::Pointer) {
                auto* structPtrTy = llvm::PointerType::get(classTypes[className], 0);
                thisPtr = builder.CreateLoad(structPtrTy, sym->value, mc->objName);
            } else {
                thisPtr = sym->value;
            }
        }

        std::string mangledName = className + "_" + mc->methodName;
        auto* fn = module->getFunction(mangledName);
        if (!fn) {
            addError("Method '" + mc->methodName + "' not found in class '" + className + "'");
            return nullptr;
        }

        size_t expectedArgs = fn->arg_size() - 1;
        if (expectedArgs != mc->args.size()) {
            addError("Wrong argument count to '" + className + "::" + mc->methodName
                     + "': expected " + std::to_string(expectedArgs)
                     + ", got " + std::to_string(mc->args.size()));
            return nullptr;
        }

        std::vector<llvm::Value*> args;
        args.push_back(thisPtr);
        for (size_t pi = 0; pi < mc->args.size(); ++pi) {
            auto* v = generate(mc->args[pi].get());
            if (!v) return nullptr;
            v = coerce(v, fn->getFunctionType()->getParamType(pi + 1));
            args.push_back(v);
        }
        return builder.CreateCall(fn, args,
               fn->getReturnType()->isVoidTy() ? "" : "call_" + mc->methodName);
    }

    if (auto* ta = dynamic_cast<ThisAccessAST*>(node)) {
        if (!currentThisAlloca || currentClassName.empty()) {
            addError("'this' used outside of a method"); return nullptr;
        }
        auto it = classInfos.find(currentClassName);
        if (it == classInfos.end()) { addError("Unknown class '" + currentClassName + "'"); return nullptr; }
        int idx = it->second.fieldIndex(ta->memberName);
        if (idx < 0) {
            addError("'" + currentClassName + "' has no field '" + ta->memberName + "'");
            return nullptr;
        }
        auto* structPtrTy = llvm::PointerType::get(it->second.llvmType, 0);
        auto* thisPtr     = builder.CreateLoad(structPtrTy, currentThisAlloca, "this");
        auto* gep         = fieldGEPFromPtr(it->second.llvmType, thisPtr, idx,
                                            "this." + ta->memberName + ".ptr");
        ValueType ft = it->second.fieldType(ta->memberName);
        return builder.CreateLoad(llvmType(ft), gep, ta->memberName);
    }

    if (auto* ta = dynamic_cast<ThisAssignAST*>(node)) {
        if (!currentThisAlloca || currentClassName.empty()) {
            addError("'this' assignment outside of a method"); return nullptr;
        }
        auto it = classInfos.find(currentClassName);
        if (it == classInfos.end()) { addError("Unknown class '" + currentClassName + "'"); return nullptr; }
        int idx = it->second.fieldIndex(ta->memberName);
        if (idx < 0) {
            addError("'" + currentClassName + "' has no field '" + ta->memberName + "'");
            return nullptr;
        }
        auto* val         = generate(ta->expr.get());
        if (!val) return nullptr;
        ValueType ft      = it->second.fieldType(ta->memberName);
        val               = coerce(val, llvmType(ft));
        auto* structPtrTy = llvm::PointerType::get(it->second.llvmType, 0);
        auto* thisPtr     = builder.CreateLoad(structPtrTy, currentThisAlloca, "this");
        auto* gep         = fieldGEPFromPtr(it->second.llvmType, thisPtr, idx,
                                            "this." + ta->memberName + ".ptr");
        builder.CreateStore(val, gep);
        return val;
    }

    // super.method(args)
    if (auto* sc = dynamic_cast<SuperCallAST*>(node)) {
        if (currentClassName.empty() || !currentThisAlloca) {
            addError("'super' used outside of a method"); return nullptr;
        }
        auto it = classInfos.find(currentClassName);
        if (it == classInfos.end() || it->second.parentName.empty()) {
            addError("Class '" + currentClassName + "' has no parent (super call invalid)");
            return nullptr;
        }
        std::string parentClass = it->second.parentName;
        std::string mangledName = parentClass + "_" + sc->methodName;
        auto* parentFn = module->getFunction(mangledName);
        if (!parentFn) {
            addError("Parent method '" + parentClass + "::" + sc->methodName + "' not found");
            return nullptr;
        }
        auto pit = classInfos.find(parentClass);
        if (pit == classInfos.end()) {
            addError("Unknown parent class '" + parentClass + "'"); return nullptr;
        }
        auto* structPtrTy = llvm::PointerType::get(classTypes[currentClassName], 0);
        auto* thisPtr     = builder.CreateLoad(structPtrTy, currentThisAlloca, "this");
        auto* parentPtrTy = llvm::PointerType::get(pit->second.llvmType, 0);
        auto* parentThis  = builder.CreateBitCast(thisPtr, parentPtrTy, "upcast_super");

        std::vector<llvm::Value*> args = {parentThis};
        size_t expectedArgs = parentFn->arg_size() - 1;
        if (expectedArgs != sc->args.size()) {
            addError("Wrong argument count to super." + sc->methodName
                     + ": expected " + std::to_string(expectedArgs)
                     + ", got " + std::to_string(sc->args.size()));
            return nullptr;
        }
        for (size_t pi = 0; pi < sc->args.size(); ++pi) {
            auto* v = generate(sc->args[pi].get()); if (!v) return nullptr;
            v = coerce(v, parentFn->getFunctionType()->getParamType(pi + 1));
            args.push_back(v);
        }
        return builder.CreateCall(parentFn, args,
            parentFn->getReturnType()->isVoidTy() ? "" : "super_call");
    }

    // ════════════════════════════════════════════════════════════
    //  Scalar nodes
    // ════════════════════════════════════════════════════════════

    if (auto* n = dynamic_cast<NumberAST*>(node))
        return llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), n->val);

    if (auto* f = dynamic_cast<FloatAST*>(node))
        return llvm::ConstantFP::get(llvm::Type::getDoubleTy(context), f->val);

    if (auto* v = dynamic_cast<VariableAST*>(node)) {
        const Symbol* sym = symbols.lookup(v->name);
        if (!sym) { addError("Use of undeclared variable '" + v->name + "'"); return nullptr; }
        if (sym->kind == SymbolKind::Function) {
            addError("'" + v->name + "' is a function, not a variable"); return nullptr;
        }
        if (sym->kind == SymbolKind::Object) {
            addError("Cannot use object '" + v->name + "' as a scalar value"); return nullptr;
        }
        symbols.markRead(v->name);
        // For pointer variables, load the pointer itself
        if (sym->kind == SymbolKind::Pointer) {
            llvm::Type* pointeeTy = llvmType(sym->type);
            llvm::Type* ptrTy     = llvm::PointerType::get(pointeeTy, 0);
            return builder.CreateLoad(ptrTy, sym->value, v->name);
        }
        return builder.CreateLoad(llvmType(sym->type), sym->value, v->name);
    }

    if (auto* vi = dynamic_cast<VarDeclInitAST*>(node)) {
        if (symbols.isDeclaredInCurrentScope(vi->name)) {
            addError("Redeclaration of '" + vi->name + "' in same scope"); return nullptr;
        }
        llvm::Type* ty    = llvmType(vi->type);
        auto*       alloc = builder.CreateAlloca(ty, nullptr, vi->name);
        try { symbols.insert(vi->name, astToValueType(vi->type), SymbolKind::Variable, alloc); }
        catch (const std::runtime_error& e) { addError(e.what()); return nullptr; }
        auto* initVal = generate(vi->init.get());
        if (!initVal) return nullptr;
        initVal = coerce(initVal, ty);
        builder.CreateStore(initVal, alloc);
        return alloc;
    }

    // const int x = expr;
    if (auto* cv = dynamic_cast<ConstVarDeclInitAST*>(node)) {
        if (symbols.isDeclaredInCurrentScope(cv->name)) {
            addError("Redeclaration of '" + cv->name + "' in same scope"); return nullptr;
        }
        llvm::Type* ty    = llvmType(cv->type);
        auto*       alloc = builder.CreateAlloca(ty, nullptr, cv->name);
        try { symbols.insert(cv->name, astToValueType(cv->type), SymbolKind::Const, alloc); }
        catch (const std::runtime_error& e) { addError(e.what()); return nullptr; }
        auto* initVal = generate(cv->init.get());
        if (!initVal) return nullptr;
        initVal = coerce(initVal, ty);
        builder.CreateStore(initVal, alloc);
        return alloc;
    }

    if (auto* vd = dynamic_cast<VarDeclAST*>(node)) {
        if (symbols.isDeclaredInCurrentScope(vd->name)) {
            addError("Redeclaration of '" + vd->name + "' in same scope"); return nullptr;
        }
        llvm::Type* ty    = llvmType(vd->type);
        auto*       alloc = builder.CreateAlloca(ty, nullptr, vd->name);
        try { symbols.insert(vd->name, astToValueType(vd->type), SymbolKind::Variable, alloc); }
        catch (const std::runtime_error& e) { addError(e.what()); }
        return alloc;
    }

    if (auto* a = dynamic_cast<AssignAST*>(node)) {
        Symbol* sym = symbols.lookup(a->name);
        if (!sym) { addError("Assignment to undeclared variable '" + a->name + "'"); return nullptr; }
        // Reject writes to const
        if (sym->kind == SymbolKind::Const) {
            addError("Assignment to const variable '" + a->name + "'"); return nullptr;
        }
        auto* val = generate(a->expr.get());
        if (!val) return nullptr;
        val = coerce(val, llvmType(sym->type));
        if (!val) return nullptr;
        builder.CreateStore(val, sym->value);
        return val;
    }

    // ── Switch ─────────────────────────────────────────────────
    if (auto* sw = dynamic_cast<SwitchAST*>(node)) {
        auto* fn      = builder.GetInsertBlock()->getParent();
        auto* mergeBB = llvm::BasicBlock::Create(context, "sw.end", fn);

        auto* condVal = generate(sw->expr.get());
        if (!condVal) return nullptr;
        if (!condVal->getType()->isIntegerTy(32))
            condVal = coerce(condVal, llvm::Type::getInt32Ty(context));

        // Identify default block
        llvm::BasicBlock* defaultBB = mergeBB;
        for (auto& c : sw->cases) {
            if (!c.value) {
                defaultBB = llvm::BasicBlock::Create(context, "sw.default", fn);
                break;
            }
        }

        auto* swInst = builder.CreateSwitch(condVal, defaultBB,
                                             (unsigned)sw->cases.size());

        llvm::BasicBlock* prevFallBB = nullptr;
        for (auto& clause : sw->cases) {
            llvm::BasicBlock* caseBB = clause.value
                ? llvm::BasicBlock::Create(context, "sw.case", fn)
                : defaultBB;

            if (clause.value) {
                auto* caseVal = generate(clause.value.get());
                if (!caseVal) continue;
                if (auto* constInt = llvm::dyn_cast<llvm::ConstantInt>(caseVal)) {
                    swInst->addCase(constInt, caseBB);
                } else {
                    addError("Switch case values must be integer constants");
                    continue;
                }
            }

            if (prevFallBB) {
                builder.SetInsertPoint(prevFallBB);
                if (!prevFallBB->getTerminator()) builder.CreateBr(caseBB);
            }

            builder.SetInsertPoint(caseBB);
            breakStack.push_back(mergeBB);
            symbols.enterScope();
            for (auto& stmt : clause.body)
                generate(stmt.get());
            symbols.exitScope();
            breakStack.pop_back();

            if (clause.hasBreak) {
                if (!builder.GetInsertBlock()->getTerminator())
                    builder.CreateBr(mergeBB);
                prevFallBB = nullptr;
            } else {
                prevFallBB = builder.GetInsertBlock();
            }
        }

        if (prevFallBB) {
            builder.SetInsertPoint(prevFallBB);
            if (!prevFallBB->getTerminator()) builder.CreateBr(mergeBB);
        }

        builder.SetInsertPoint(mergeBB);
        return nullptr;
    }

    if (auto* i = dynamic_cast<IfAST*>(node)) {
        auto* condVal = generate(i->cond.get());
        if (!condVal) return nullptr;
        auto* cond    = toBool(condVal);
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

    if (auto* b = dynamic_cast<BlockAST*>(node)) {
        symbols.enterScope();
        for (auto& stmt : b->statements) {
            generate(stmt.get());
            if (builder.GetInsertBlock()->getTerminator()) break;
        }
        symbols.exitScope();
        return nullptr;
    }

    if (auto* f = dynamic_cast<FunctionAST*>(node)) {
        std::vector<llvm::Type*> paramTypes;
        std::vector<ValueType>   paramVT;
        for (size_t i = 0; i < f->proto->args.size(); ++i) {
            ASTType at = (i < f->proto->argTypes.size()) ? f->proto->argTypes[i] : ASTType::Int;
            paramTypes.push_back(llvmType(at));
            paramVT.push_back(astToValueType(at));
        }
        llvm::Type* retTy = llvmType(f->proto->returnType);
        auto* ft  = llvm::FunctionType::get(retTy, paramTypes, false);
        auto* fn  = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                                           f->proto->name, *module);
        try { symbols.insertFunction(f->proto->name, astToValueType(f->proto->returnType),
                                     paramVT, fn); }
        catch (const std::runtime_error& e) {
            addError(e.what()); fn->eraseFromParent(); return nullptr;
        }

        auto* entry = llvm::BasicBlock::Create(context, "entry", fn);
        builder.SetInsertPoint(entry);
        symbols.enterScope();
        symbols.setCurrentFunction(f->proto->name);

        size_t idx = 0;
        for (auto& arg : fn->args()) {
            const std::string& pname = f->proto->args[idx];
            ASTType at = (idx < f->proto->argTypes.size()) ? f->proto->argTypes[idx] : ASTType::Int;
            auto* alloc = builder.CreateAlloca(llvmType(at), nullptr, pname);
            builder.CreateStore(&arg, alloc);
            try { symbols.insert(pname, astToValueType(at), SymbolKind::Parameter, alloc); }
            catch (const std::runtime_error& e) { addError(e.what()); }
            idx++;
        }

        generate(f->body.get());

        if (!builder.GetInsertBlock()->getTerminator()) {
            if (retTy->isVoidTy())        builder.CreateRetVoid();
            else if (retTy->isDoubleTy()) builder.CreateRet(llvm::ConstantFP::get(retTy, 0.0));
            else                          builder.CreateRet(llvm::ConstantInt::get(retTy, 0));
        }

        symbols.clearCurrentFunction();
        symbols.exitScope();

        std::string errStr;
        llvm::raw_string_ostream errStream(errStr);
        if (llvm::verifyFunction(*fn, &errStream))
            addError("LLVM IR verify failed for '" + f->proto->name + "': " + errStream.str());
        return fn;
    }

    if (auto* c = dynamic_cast<CallAST*>(node)) {
        auto* fn = module->getFunction(c->callee);
        if (!fn) { addError("Call to undefined function '" + c->callee + "'"); return nullptr; }
        if (fn->arg_size() != c->args.size()) {
            addError("Wrong argument count to '" + c->callee + "': expected "
                     + std::to_string(fn->arg_size())
                     + ", got " + std::to_string(c->args.size()));
            return nullptr;
        }
        std::vector<llvm::Value*> args;
        size_t pi = 0;
        for (auto& a : c->args) {
            auto* v = generate(a.get()); if (!v) return nullptr;
            v = coerce(v, fn->getFunctionType()->getParamType(pi++));
            args.push_back(v);
        }
        return builder.CreateCall(fn, args, fn->getReturnType()->isVoidTy() ? "" : "calltmp");
    }

    if (auto* a = dynamic_cast<ArrayDeclAST*>(node)) {
        if (a->size <= 0) { addError("Array '" + a->name + "' has invalid size"); return nullptr; }
        if (symbols.isDeclaredInCurrentScope(a->name)) {
            addError("Redeclaration of array '" + a->name + "' in same scope"); return nullptr;
        }
        llvm::Type* elemTy = llvmType(a->type);
        auto* arrTy  = llvm::ArrayType::get(elemTy, a->size);
        auto* alloc  = builder.CreateAlloca(arrTy, nullptr, a->name);
        try { symbols.insert(a->name, astToValueType(a->type), SymbolKind::Array, alloc, a->size); }
        catch (const std::runtime_error& e) { addError(e.what()); }
        return alloc;
    }

    if (auto* arr = dynamic_cast<ArrayAccessAST*>(node)) {
        const Symbol* sym = symbols.lookup(arr->name);
        if (!sym) { addError("Use of undeclared array '" + arr->name + "'"); return nullptr; }
        if (sym->kind != SymbolKind::Array) { addError("'" + arr->name + "' is not an array"); return nullptr; }
        auto* idx    = generate(arr->index.get()); if (!idx) return nullptr;
        idx = coerce(idx, llvm::Type::getInt32Ty(context));
        auto* alloca = llvm::cast<llvm::AllocaInst>(sym->value);
        auto* arrTy  = alloca->getAllocatedType();
        auto* zero   = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);
        auto* gep    = builder.CreateGEP(arrTy, sym->value, {zero, idx}, arr->name + ".gep");
        return builder.CreateLoad(llvmType(sym->type), gep, arr->name + ".load");
    }

    if (auto* aa = dynamic_cast<ArrayAssignAST*>(node)) {
        const Symbol* sym = symbols.lookup(aa->name);
        if (!sym) { addError("Assignment to undeclared array '" + aa->name + "'"); return nullptr; }
        if (sym->kind != SymbolKind::Array) { addError("'" + aa->name + "' is not an array"); return nullptr; }
        auto* idx = generate(aa->index.get()); if (!idx) return nullptr;
        idx = coerce(idx, llvm::Type::getInt32Ty(context));
        auto* val = generate(aa->expr.get());  if (!val) return nullptr;
        val = coerce(val, llvmType(sym->type));
        auto* alloca = llvm::cast<llvm::AllocaInst>(sym->value);
        auto* arrTy  = alloca->getAllocatedType();
        auto* zero   = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);
        auto* gep    = builder.CreateGEP(arrTy, sym->value, {zero, idx}, aa->name + ".gep");
        builder.CreateStore(val, gep);
        return val;
    }

    if (auto* bin = dynamic_cast<BinaryAST*>(node)) {
        auto* lhs = generate(bin->lhs.get());
        auto* rhs = generate(bin->rhs.get());
        if (!lhs || !rhs) return nullptr;
        if (lhs->getType()->isPointerTy())
            lhs = builder.CreateLoad(llvm::Type::getInt32Ty(context), lhs);
        if (rhs->getType()->isPointerTy())
            rhs = builder.CreateLoad(llvm::Type::getInt32Ty(context), rhs);
        auto [l, r] = promoteToCommon(lhs, rhs);
        bool isFloat = l->getType()->isDoubleTy();
        if (bin->op == "+")  return isFloat ? builder.CreateFAdd(l,r,"fadd") : builder.CreateAdd(l,r,"add");
        if (bin->op == "-")  return isFloat ? builder.CreateFSub(l,r,"fsub") : builder.CreateSub(l,r,"sub");
        if (bin->op == "*")  return isFloat ? builder.CreateFMul(l,r,"fmul") : builder.CreateMul(l,r,"mul");
        if (bin->op == "/") {
            if (!isFloat) {
                auto* zero   = llvm::ConstantInt::get(l->getType(), 0);
                auto* isZero = builder.CreateICmpEQ(r, zero, "divzero");
                auto* safeR  = builder.CreateSelect(isZero,
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
        auto [l, r] = promoteToCommon(lhs, rhs);
        if (log->op == "==") return builder.CreateICmpEQ(l, r);
        if (log->op == "!=") return builder.CreateICmpNE(l, r);
        addError("Unknown logical operator '" + log->op + "'");
        return nullptr;
    }

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

    if (auto* ret = dynamic_cast<ReturnAST*>(node)) {
        auto* fn    = builder.GetInsertBlock()->getParent();
        auto* retTy = fn->getReturnType();
        llvm::Value* val = nullptr;
        if (ret->expr) {
            val = generate(ret->expr.get()); if (!val) return nullptr;
            val = coerce(val, retTy);
        } else {
            if (retTy->isVoidTy())    return builder.CreateRetVoid();
            if (retTy->isDoubleTy())  val = llvm::ConstantFP::get(retTy, 0.0);
            else                      val = llvm::ConstantInt::get(retTy, 0);
        }
        return builder.CreateRet(val);
    }

    if (dynamic_cast<BreakAST*>(node)) {
        if (breakStack.empty()) { addError("'break' outside loop/switch"); return nullptr; }
        return builder.CreateBr(breakStack.back());
    }
    if (dynamic_cast<ContinueAST*>(node)) {
        if (continueStack.empty()) { addError("'continue' outside loop"); return nullptr; }
        return builder.CreateBr(continueStack.back());
    }

    if (auto* inc = dynamic_cast<PostIncAST*>(node)) {
        Symbol* sym = symbols.lookup(inc->name);
        if (!sym) { addError("Use of undeclared variable '" + inc->name + "' in '++'"); return nullptr; }
        llvm::Type* ty   = llvmType(sym->type);
        auto* old        = builder.CreateLoad(ty, sym->value, inc->name);
        llvm::Value* one = ty->isDoubleTy()
                           ? (llvm::Value*)llvm::ConstantFP::get(ty, 1.0)
                           : (llvm::Value*)llvm::ConstantInt::get(ty, 1);
        auto* incremented = ty->isDoubleTy()
                            ? builder.CreateFAdd(old, one, "finc")
                            : builder.CreateAdd(old, one, "inc");
        builder.CreateStore(incremented, sym->value);
        return old;
    }

    if (auto* prog = dynamic_cast<ProgramAST*>(node)) {
        for (auto& item : prog->topLevel)
            generate(item.get());
        return nullptr;
    }

    addError(std::string("Unhandled AST node type: ") + typeid(*node).name());
    return nullptr;
}