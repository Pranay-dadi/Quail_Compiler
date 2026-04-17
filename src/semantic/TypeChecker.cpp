#include "semantic/TypeChecker.h"

TypeChecker::TypeChecker() {}

void TypeChecker::addError(int line, const std::string& msg) {
    for (auto& e : errors)
        if (e.line == line && e.message == msg) return;
    errors.push_back({line, msg});
}

// ─────────────────────────────────────────────────────────────
//  collectSignatures — first pass: register all function sigs
// ─────────────────────────────────────────────────────────────
void TypeChecker::collectSignatures(AST* root) {
    auto* prog = dynamic_cast<ProgramAST*>(root);
    if (!prog) return;
    for (auto& item : prog->topLevel) {
        if (auto* fn = dynamic_cast<FunctionAST*>(item.get())) {
            if (!fn->proto) continue;
            funcReturnTypes[fn->proto->name] = fn->proto->returnType;
            funcParamTypes [fn->proto->name] = fn->proto->argTypes;
        } else if (auto* cls = dynamic_cast<ClassDeclAST*>(item.get())) {
            for (auto& m : cls->methods) {
                if (!m->proto) continue;
                std::string mangled = cls->name + "_" + m->proto->name;
                funcReturnTypes[mangled] = m->proto->returnType;
                funcParamTypes [mangled] = m->proto->argTypes;
                // Also register un-mangled for this.method() calls
                funcReturnTypes[m->proto->name] = m->proto->returnType;
                funcParamTypes [m->proto->name] = m->proto->argTypes;
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────
//  typeOf — infer expression type
// ─────────────────────────────────────────────────────────────
ASTType TypeChecker::typeOf(AST* node) {
    if (!node) return ASTType::Unknown;

    if (dynamic_cast<NumberAST*>(node))    return ASTType::Int;
    if (dynamic_cast<FloatAST*>(node))     return ASTType::Float;
    if (dynamic_cast<StringAST*>(node))    return ASTType::Unknown; // i8*
    if (dynamic_cast<PostIncAST*>(node))   return ASTType::Int;
    if (dynamic_cast<VariableAST*>(node))  return ASTType::Int;  // conservative
    if (dynamic_cast<AddressOfAST*>(node)) return ASTType::Unknown; // pointer
    if (dynamic_cast<DerefAST*>(node))     return ASTType::Int;
    if (dynamic_cast<StrLenAST*>(node))    return ASTType::Int;
    if (dynamic_cast<StrCmpAST*>(node))    return ASTType::Int;
    if (dynamic_cast<StrCatAST*>(node))    return ASTType::Unknown;
    if (dynamic_cast<NewExprAST*>(node))   return ASTType::Unknown;
    if (dynamic_cast<NewScalarAST*>(node)) return ASTType::Unknown;

    if (auto* b = dynamic_cast<BinaryAST*>(node)) {
        ASTType l = typeOf(b->lhs.get());
        ASTType r = typeOf(b->rhs.get());
        if (b->op == "<" || b->op == ">" || b->op == "<=" ||
            b->op == ">=" || b->op == "==" || b->op == "!=")
            return ASTType::Int;
        if (l != ASTType::Unknown && r != ASTType::Unknown && !compatible(l, r))
            addError(0, "Type mismatch in binary expression: '"
                     + astTypeName(l) + "' " + b->op + " '" + astTypeName(r) + "'");
        return wider(l, r);
    }

    if (dynamic_cast<LogicalAST*>(node)) return ASTType::Int;

    if (auto* u = dynamic_cast<UnaryAST*>(node)) {
        ASTType t = typeOf(u->operand.get());
        if (u->op == "!" && t == ASTType::Float)
            addError(0, "Unary '!' applied to float; consider comparing with 0.0");
        return t;
    }

    if (auto* c = dynamic_cast<CallAST*>(node)) {
        auto it = funcReturnTypes.find(c->callee);
        if (it == funcReturnTypes.end()) return ASTType::Int;
        if (it->second == ASTType::Void) {
            addError(0, "Function '" + c->callee
                     + "' returns void; its result cannot be used as a value");
            return ASTType::Unknown;
        }
        return it->second;
    }

    if (auto* vi = dynamic_cast<VarDeclInitAST*>(node))      return vi->type;
    if (auto* cv = dynamic_cast<ConstVarDeclInitAST*>(node)) return cv->type;
    if (auto* vd = dynamic_cast<VarDeclAST*>(node))          return vd->type;

    return ASTType::Unknown;
}

// ─────────────────────────────────────────────────────────────
//  checkStmt
// ─────────────────────────────────────────────────────────────
void TypeChecker::checkStmt(AST* node) {
    if (!node || node->isNoOp()) return;

    // VarDeclInit
    if (auto* vi = dynamic_cast<VarDeclInitAST*>(node)) {
        ASTType initT = typeOf(vi->init.get());
        if (initT != ASTType::Unknown && !compatible(vi->type, initT))
            addError(0, "Cannot initialize '" + astTypeName(vi->type) + " "
                     + vi->name + "' with value of type '" + astTypeName(initT) + "'");
        return;
    }

    // ConstVarDeclInit
    if (auto* cv = dynamic_cast<ConstVarDeclInitAST*>(node)) {
        ASTType initT = typeOf(cv->init.get());
        if (initT != ASTType::Unknown && !compatible(cv->type, initT))
            addError(0, "Cannot initialize const '" + astTypeName(cv->type) + " "
                     + cv->name + "' with value of type '" + astTypeName(initT) + "'");
        return;
    }

    // Return
    if (auto* ret = dynamic_cast<ReturnAST*>(node)) {
        if (currentReturnType == ASTType::Void && ret->expr)
            addError(0, "Function '" + currentFunctionName
                     + "' is void but attempts to return a value");
        else if (currentReturnType != ASTType::Void && !ret->expr)
            addError(0, "Function '" + currentFunctionName
                     + "' must return a value of type '"
                     + astTypeName(currentReturnType) + "'");
        else if (ret->expr) {
            ASTType retT = typeOf(ret->expr.get());
            if (retT != ASTType::Unknown && !compatible(currentReturnType, retT))
                addError(0, "Return type mismatch in '" + currentFunctionName
                         + "': expected '" + astTypeName(currentReturnType)
                         + "', got '" + astTypeName(retT) + "'");
        }
        return;
    }

    // If
    if (auto* ifs = dynamic_cast<IfAST*>(node)) {
        typeOf(ifs->cond.get()); // triggers nested errors
        checkStmt(ifs->thenBlock.get());
        if (ifs->elseBlock) checkStmt(ifs->elseBlock.get());
        return;
    }

    // While
    if (auto* w = dynamic_cast<WhileAST*>(node)) {
        typeOf(w->cond.get());
        checkStmt(w->body.get());
        return;
    }

    // For
    if (auto* f = dynamic_cast<ForAST*>(node)) {
        if (f->init) checkStmt(f->init.get());
        if (f->cond) typeOf(f->cond.get());
        if (f->inc)  checkStmt(f->inc.get());
        checkStmt(f->body.get());
        return;
    }

    // Switch
    if (auto* sw = dynamic_cast<SwitchAST*>(node)) {
        typeOf(sw->expr.get());
        for (auto& clause : sw->cases) {
            if (clause.value) {
                ASTType ct = typeOf(clause.value.get());
                if (ct == ASTType::Float)
                    addError(0, "Switch case value must be an integer, not float");
            }
            for (auto& s : clause.body) checkStmt(s.get());
        }
        return;
    }

    // Function call as statement — check arg count
    if (auto* c = dynamic_cast<CallAST*>(node)) {
        auto ptIt = funcParamTypes.find(c->callee);
        if (ptIt != funcParamTypes.end()) {
            const auto& params = ptIt->second;
            if (params.size() != c->args.size())
                addError(0, "Wrong argument count to '" + c->callee
                         + "': expected " + std::to_string(params.size())
                         + ", got " + std::to_string(c->args.size()));
            else {
                for (size_t i = 0; i < params.size(); ++i) {
                    ASTType argT = typeOf(c->args[i].get());
                    if (argT != ASTType::Unknown && !compatible(params[i], argT))
                        addError(0, "Argument " + std::to_string(i+1)
                                 + " to '" + c->callee + "': expected '"
                                 + astTypeName(params[i]) + "', got '"
                                 + astTypeName(argT) + "'");
                }
            }
        }
        return;
    }

    // Array assignment — index must be int
    if (auto* aa = dynamic_cast<ArrayAssignAST*>(node)) {
        ASTType idxT = typeOf(aa->index.get());
        if (idxT == ASTType::Float)
            addError(0, "Array index for '" + aa->name + "' must be integer, not float");
        return;
    }

    // Block
    if (auto* blk = dynamic_cast<BlockAST*>(node)) {
        checkBlock(blk); return;
    }
}

void TypeChecker::checkBlock(BlockAST* block) {
    if (!block) return;
    for (auto& stmt : block->statements)
        checkStmt(stmt.get());
}

void TypeChecker::checkFunction(FunctionAST* fn) {
    if (!fn || !fn->proto) return;
    currentReturnType   = fn->proto->returnType;
    currentFunctionName = fn->proto->name;
    checkBlock(fn->body.get());
    currentReturnType   = ASTType::Void;
    currentFunctionName.clear();
}

void TypeChecker::checkClass(ClassDeclAST* cls) {
    for (auto& method : cls->methods)
        checkFunction(method.get());
}

void TypeChecker::check(AST* root) {
    errors.clear();
    collectSignatures(root);
    auto* prog = dynamic_cast<ProgramAST*>(root);
    if (!prog) return;
    for (auto& item : prog->topLevel) {
        if (auto* fn = dynamic_cast<FunctionAST*>(item.get()))
            checkFunction(fn);
        else if (auto* cls = dynamic_cast<ClassDeclAST*>(item.get()))
            checkClass(cls);
    }
}