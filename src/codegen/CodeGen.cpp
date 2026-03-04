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

// ── Coerce value to target type ───────────────────────────────
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
    addError("Type mismatch: cannot coerce types");
    return val;
}

// ── Promote both operands to common type ──────────────────────
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
//  OOP helpers — GEP construction
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

// ══════════════════════════════════════════════════════════════
//  OOP — method generation
//
//  Methods are compiled as:
//    define RetType @ClassName_methodName(%ClassName* %this_arg, params...) { ... }
//
//  Inside the body:
//    currentClassName  = class name (for this.field lookups)
//    currentThisAlloca = alloca of %ClassName* (holds the this pointer)
// ══════════════════════════════════════════════════════════════

void CodeGen::generateMethod(FunctionAST* f,
                              const std::string& className,
                              llvm::StructType*  structTy)
{
    // Build LLVM parameter list: (ClassName* this_arg, param0, param1, ...)
    auto* structPtrTy = llvm::PointerType::get(structTy, 0);

    std::vector<llvm::Type*> paramTypes;
    std::vector<ValueType>   paramVT;
    paramTypes.push_back(structPtrTy);          // implicit this*
    paramVT.push_back(ValueType::Unknown);      // placeholder

    for (size_t i = 0; i < f->proto->args.size(); ++i) {
        ASTType at = (i < f->proto->argTypes.size())
                     ? f->proto->argTypes[i]
                     : ASTType::Int;
        paramTypes.push_back(llvmType(at));
        paramVT.push_back(astToValueType(at));
    }

    llvm::Type* retTy      = llvmType(f->proto->returnType);
    auto*       ft         = llvm::FunctionType::get(retTy, paramTypes, false);
    std::string mangledName = className + "_" + f->proto->name;

    auto* fn = llvm::Function::Create(ft,
                   llvm::Function::ExternalLinkage, mangledName, *module);

    // Register method as a function symbol at global scope
    std::vector<ValueType> regParams(paramVT.begin() + 1, paramVT.end());
    try {
        symbols.insertFunction(mangledName,
                               astToValueType(f->proto->returnType),
                               regParams, fn);
    } catch (...) { /* redefinition guard — ignore duplicate */ }

    auto* entry = llvm::BasicBlock::Create(context, "entry", fn);
    builder.SetInsertPoint(entry);
    symbols.enterScope();
    symbols.setCurrentFunction(mangledName);

    // ── Alloca for 'this' pointer ──────────────────────────────
    auto argIt = fn->args().begin();
    argIt->setName("this_arg");

    auto* thisPtrAlloca = builder.CreateAlloca(structPtrTy, nullptr, "this.addr");
    builder.CreateStore(&*argIt, thisPtrAlloca);
    currentThisAlloca = thisPtrAlloca;
    ++argIt;

    // ── Alloca for explicit parameters ─────────────────────────
    size_t idx = 0;
    for (auto it = argIt; it != fn->args().end(); ++it, ++idx) {
        // Guard: proto->args may be shorter than LLVM's arg list if something
        // went wrong during codegen setup — avoid out-of-bounds UB.
        if (idx >= f->proto->args.size()) {
            addError("generateMethod '" + mangledName +
                     "': LLVM arg count exceeds prototype arg count");
            break;
        }
        const std::string& pname = f->proto->args[idx];
        ASTType at = (idx < f->proto->argTypes.size())
                     ? f->proto->argTypes[idx]
                     : ASTType::Int;
        auto* alloc = builder.CreateAlloca(llvmType(at), nullptr, pname);
        builder.CreateStore(&*it, alloc);
        try {
            symbols.insert(pname, astToValueType(at), SymbolKind::Parameter, alloc);
        } catch (const std::runtime_error& e) { addError(e.what()); }
        (void)idx; // idx is incremented by the for-header; suppress unused-value warning
    }

    // ── Generate body ──────────────────────────────────────────
    generate(f->body.get());

    // ── Auto return if missing ─────────────────────────────────
    if (!builder.GetInsertBlock()->getTerminator()) {
        if (retTy->isVoidTy())       builder.CreateRetVoid();
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

// ══════════════════════════════════════════════════════════════
//  Main code generation dispatcher
// ══════════════════════════════════════════════════════════════

llvm::Value* CodeGen::generate(AST* node) {
    if (!node) {
        addError("[CodeGen] Internal: null AST node");
        return nullptr;
    }

    // ── Comments ───────────────────────────────────────────────
    if (dynamic_cast<LineCommentAST*>(node))  return nullptr;
    if (dynamic_cast<BlockCommentAST*>(node)) return nullptr;

    // ════════════════════════════════════════════════════════════
    //  OOP — Class declaration
    //  Registers the struct type and generates all methods.
    // ════════════════════════════════════════════════════════════
    if (auto* cls = dynamic_cast<ClassDeclAST*>(node)) {
        // Build LLVM struct field types
        std::vector<llvm::Type*> fieldLLVMTypes;
        ClassInfo info;
        info.name = cls->name;

        for (auto& f : cls->fields) {
            fieldLLVMTypes.push_back(llvmType(f.type));
            info.fields.push_back({f.name, astToValueType(f.type)});
        }

        auto* structTy = llvm::StructType::create(context, fieldLLVMTypes, cls->name);
        info.llvmType  = structTy;
        classTypes[cls->name]  = structTy;
        classInfos[cls->name]  = info;

        // Generate each method
        currentClassName = cls->name;
        for (auto& method : cls->methods)
            generateMethod(method.get(), cls->name, structTy);
        currentClassName.clear();

        return nullptr;
    }

    // ════════════════════════════════════════════════════════════
    //  OOP — Object declaration  (ClassName varName;)
    // ════════════════════════════════════════════════════════════
    if (auto* od = dynamic_cast<ObjectDeclAST*>(node)) {
        auto it = classTypes.find(od->className);
        if (it == classTypes.end()) {
            addError("Unknown class '" + od->className + "'");
            return nullptr;
        }
        if (symbols.isDeclaredInCurrentScope(od->varName)) {
            addError("Redeclaration of '" + od->varName + "' in same scope");
            return nullptr;
        }
        auto* alloc = builder.CreateAlloca(it->second, nullptr, od->varName);
        // Zero-initialize all fields (mirrors Java/C# default field values).
        // Without this, any field read before an explicit setter call yields UB.
        builder.CreateStore(llvm::Constant::getNullValue(it->second), alloc);
        try {
            symbols.insert(od->varName, ValueType::Unknown, SymbolKind::Object,
                           alloc, 0, od->className);
        } catch (const std::runtime_error& e) { addError(e.what()); }
        return alloc;
    }

    // ════════════════════════════════════════════════════════════
    //  OOP — Member access  (obj.field  in expression)
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

    // ════════════════════════════════════════════════════════════
    //  OOP — Member assign  (obj.field = expr;)
    // ════════════════════════════════════════════════════════════
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
        ValueType ft  = it->second.fieldType(ma->memberName);
        val = coerce(val, llvmType(ft));
        auto* gep = fieldGEPFromPtr(it->second.llvmType, sym->value, idx,
                                    ma->objName + "." + ma->memberName + ".ptr");
        builder.CreateStore(val, gep);
        return val;
    }

    // ════════════════════════════════════════════════════════════
    //  OOP — Method call  (obj.method(args)  or  this.method(args))
    // ════════════════════════════════════════════════════════════
    if (auto* mc = dynamic_cast<MethodCallAST*>(node)) {
        std::string className;
        llvm::Value* thisPtr = nullptr;

        if (mc->objName == "this") {
            // Self-call inside a method
            if (currentClassName.empty() || !currentThisAlloca) {
                addError("'this' method call outside of a method");
                return nullptr;
            }
            className = currentClassName;
            auto* structPtrTy = llvm::PointerType::get(classTypes[className], 0);
            thisPtr = builder.CreateLoad(structPtrTy, currentThisAlloca, "this");
        } else {
            const Symbol* sym = symbols.lookup(mc->objName);
            if (!sym) { addError("Use of undeclared object '" + mc->objName + "'"); return nullptr; }
            if (sym->kind != SymbolKind::Object) {
                addError("'" + mc->objName + "' is not an object"); return nullptr;
            }
            className = sym->objectClass;
            thisPtr   = sym->value;   // already a %ClassName*
        }

        std::string mangledName = className + "_" + mc->methodName;
        auto* fn = module->getFunction(mangledName);
        if (!fn) {
            addError("Method '" + mc->methodName + "' not found in class '" + className + "'");
            return nullptr;
        }

        // Build argument list: (this, arg0, arg1, ...)
        size_t expectedArgs = fn->arg_size() - 1; // exclude implicit this
        if (expectedArgs != mc->args.size()) {
            addError("Wrong argument count to '" + className + "::" + mc->methodName +
                     "': expected " + std::to_string(expectedArgs) +
                     ", got " + std::to_string(mc->args.size()));
            return nullptr;
        }

        std::vector<llvm::Value*> args;
        args.push_back(thisPtr);
        for (size_t pi = 0; pi < mc->args.size(); ++pi) {
            auto* v = generate(mc->args[pi].get());
            if (!v) return nullptr;
            llvm::Type* expectedTy = fn->getFunctionType()->getParamType(pi + 1);
            v = coerce(v, expectedTy);
            args.push_back(v);
        }

        return builder.CreateCall(fn, args,
               fn->getReturnType()->isVoidTy() ? "" : "call_" + mc->methodName);
    }

    // ════════════════════════════════════════════════════════════
    //  OOP — this.field access  (inside a method)
    // ════════════════════════════════════════════════════════════
    if (auto* ta = dynamic_cast<ThisAccessAST*>(node)) {
        if (!currentThisAlloca || currentClassName.empty()) {
            addError("'this' used outside of a method");
            return nullptr;
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
        ValueType ft      = it->second.fieldType(ta->memberName);
        return builder.CreateLoad(llvmType(ft), gep, ta->memberName);
    }

    // ════════════════════════════════════════════════════════════
    //  OOP — this.field = expr  (inside a method)
    // ════════════════════════════════════════════════════════════
    if (auto* ta = dynamic_cast<ThisAssignAST*>(node)) {
        if (!currentThisAlloca || currentClassName.empty()) {
            addError("'this' assignment outside of a method");
            return nullptr;
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

    // ════════════════════════════════════════════════════════════
    //  Existing non-OOP nodes below (unchanged from v2.0)
    // ════════════════════════════════════════════════════════════

    // ── Integer literal ────────────────────────────────────────
    if (auto* n = dynamic_cast<NumberAST*>(node))
        return llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), n->val);

    // ── Float literal ──────────────────────────────────────────
    if (auto* f = dynamic_cast<FloatAST*>(node))
        return llvm::ConstantFP::get(llvm::Type::getDoubleTy(context), f->val);

    // ── Variable reference ─────────────────────────────────────
    if (auto* v = dynamic_cast<VariableAST*>(node)) {
        const Symbol* sym = symbols.lookup(v->name);
        if (!sym) { addError("Use of undeclared variable '" + v->name + "'"); return nullptr; }
        if (sym->kind == SymbolKind::Function) {
            addError("'" + v->name + "' is a function, not a variable"); return nullptr;
        }
        if (sym->kind == SymbolKind::Object) {
            // Using an object as a value is not directly supported yet
            addError("Cannot use object '" + v->name + "' as a scalar value"); return nullptr;
        }
        return builder.CreateLoad(llvmType(sym->type), sym->value, v->name);
    }

    // ── Variable declaration with initializer (in-place, no child scope) ─────
    // Handles:  int name = expr;
    // This keeps 'name' alive in the current scope so subsequent statements
    // (e.g. return name;) can see it — unlike the old BlockAST wrapper.
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

    // ── Variable declaration ───────────────────────────────────
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

    // ── Assignment ─────────────────────────────────────────────
    if (auto* a = dynamic_cast<AssignAST*>(node)) {
        Symbol* sym = symbols.lookup(a->name);
        if (!sym) { addError("Assignment to undeclared variable '" + a->name + "'"); return nullptr; }
        auto* val = generate(a->expr.get());
        if (!val) return nullptr;
        val = coerce(val, llvmType(sym->type));
        if (!val) return nullptr;
        builder.CreateStore(val, sym->value);
        return val;
    }

    // ── If / If-Else ───────────────────────────────────────────
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
        try {
            symbols.insertFunction(f->proto->name, astToValueType(f->proto->returnType), paramVT, fn);
        } catch (const std::runtime_error& e) { addError(e.what()); fn->eraseFromParent(); return nullptr; }

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

    // ── Function call ──────────────────────────────────────────
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

    // ── Array declaration ──────────────────────────────────────
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

    // ── Array access ───────────────────────────────────────────
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

    // ── Array assign ───────────────────────────────────────────
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

    // ── Binary operators ───────────────────────────────────────
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
                auto* zero  = llvm::ConstantInt::get(l->getType(), 0);
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
            if (retTy->isVoidTy())    return builder.CreateRetVoid();
            if (retTy->isDoubleTy())  val = llvm::ConstantFP::get(retTy, 0.0);
            else                      val = llvm::ConstantInt::get(retTy, 0);
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

void CodeGen::dumpToFile(const std::string& filename) {
    std::error_code EC;
    llvm::raw_fd_ostream out(filename, EC);
    if (EC) { addError("Cannot write '" + filename + "': " + EC.message()); return; }
    module->print(out, nullptr);
}

void CodeGen::dump() {
    module->print(llvm::outs(), nullptr);
}