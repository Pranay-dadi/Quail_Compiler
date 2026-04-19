#include "ir/IRBuilder.h"
#include <sstream>

IRBuilder::IRBuilder(const std::string& moduleName) {
    module = std::make_unique<IRModule>();
    module->name = moduleName;
}

// FIX: return unique_ptr ownership via std::move so the caller
// takes ownership cleanly without any double-free.
std::unique_ptr<IRModule> IRBuilder::build(AST* root) {
    auto* prog = dynamic_cast<ProgramAST*>(root);
    if (!prog) {
        addError("build: expected ProgramAST");
        return std::move(module);
    }
    for (auto& item : prog->topLevel) {
        if (auto* fn  = dynamic_cast<FunctionAST*>(item.get()))  genFunction(fn);
        if (auto* cls = dynamic_cast<ClassDeclAST*>(item.get())) genClass(cls);
    }
    return std::move(module);
}

IRValue IRBuilder::genExpr(AST* node) {
    if (!node) return IRValue::makeUndef();

    if (auto* n = dynamic_cast<NumberAST*>(node))  return IRValue::makeInt(n->val);
    if (auto* f = dynamic_cast<FloatAST*>(node))   return IRValue::makeFloat(f->val);
    if (auto* s = dynamic_cast<StringAST*>(node))  return IRValue::makeVar(s->value);

    if (auto* v = dynamic_cast<VariableAST*>(node)) {
        auto it = varMap.find(v->name);
        if (it != varMap.end()) return it->second;
        IRValue src = IRValue::makeVar(v->name);
        IRValue dst = newTemp();
        emit(IRInstruction::makeAssign(dst, src));
        return dst;
    }

    if (auto* b = dynamic_cast<BinaryAST*>(node)) {
        IRValue lv = genExpr(b->lhs.get());
        IRValue rv = genExpr(b->rhs.get());
        bool fp = lv.isFloat || rv.isFloat;
        return emitBinopDag(binopFor(b->op, fp), lv, rv, fp);
    }

    if (auto* l = dynamic_cast<LogicalAST*>(node)) {
        if (l->op == "&&" || l->op == "||") {
            bool isAnd = (l->op == "&&");
            std::string trueLbl  = newLabel(isAnd ? "land_t" : "lor_t");
            std::string falseLbl = newLabel(isAnd ? "land_f" : "lor_f");
            std::string mergeLbl = newLabel(isAnd ? "land_m" : "lor_m");
            IRValue lv = genExpr(l->lhs.get());
            emit(IRInstruction::makeCJump(lv, trueLbl, falseLbl));

            BasicBlock* trueBB = currentFn->addBlock(trueLbl);
            switchTo(trueBB);
            IRValue resT = newTemp();
            if (isAnd) {
                IRValue rv = genExpr(l->rhs.get());
                emit(IRInstruction::makeAssign(resT, rv));
            } else {
                emit(IRInstruction::makeAssign(resT, IRValue::makeInt(1)));
            }
            emit(IRInstruction::makeJump(mergeLbl));

            BasicBlock* falseBB = currentFn->addBlock(falseLbl);
            switchTo(falseBB);
            IRValue resF = newTemp();
            if (isAnd) {
                emit(IRInstruction::makeAssign(resF, IRValue::makeInt(0)));
            } else {
                IRValue rv = genExpr(l->rhs.get());
                emit(IRInstruction::makeAssign(resF, rv));
            }
            emit(IRInstruction::makeJump(mergeLbl));

            BasicBlock* mergeBB = currentFn->addBlock(mergeLbl);
            switchTo(mergeBB);
            IRValue phi = newTemp();
            emit(IRInstruction::makePhi(phi, {{trueLbl, resT},{falseLbl, resF}}));
            return phi;
        }
        IRValue lv = genExpr(l->lhs.get());
        IRValue rv = genExpr(l->rhs.get());
        IROp op = (l->op == "==") ? IROp::EQ : IROp::NEQ;
        return emitBinopDag(op, lv, rv);
    }

    if (auto* u = dynamic_cast<UnaryAST*>(node)) {
        IRValue operand = genExpr(u->operand.get());
        IRValue dst = newTemp(operand.isFloat);
        IROp irop = (u->op == "-") ? (operand.isFloat ? IROp::FNEG : IROp::NEG) : IROp::LNOT;
        emit(IRInstruction::makeUnop(irop, dst, operand));
        return dst;
    }

    if (auto* pi = dynamic_cast<PostIncAST*>(node)) {
        auto it = varMap.find(pi->name);
        IRValue old = (it != varMap.end()) ? it->second : IRValue::makeVar(pi->name);
        IRValue nv  = newTemp();
        emit(IRInstruction::makeBinop(IROp::ADD, nv, old, IRValue::makeInt(1)));
        emit(IRInstruction::makeAssign(IRValue::makeVar(pi->name), nv));
        dag.clear();
        return old;
    }

    if (auto* c = dynamic_cast<CallAST*>(node)) {
        std::vector<IRValue> args;
        for (auto& a : c->args) args.push_back(genExpr(a.get()));
        IRValue dst = newTemp();
        emit(IRInstruction::makeCall(dst, c->callee, args));
        return dst;
    }

    if (auto* mc = dynamic_cast<MethodCallAST*>(node)) {
        auto it = varMap.find(mc->objName);
        IRValue thisPtr = (it != varMap.end()) ? it->second : IRValue::makeVar(mc->objName);
        std::vector<IRValue> args = {thisPtr};
        for (auto& a : mc->args) args.push_back(genExpr(a.get()));
        IRValue dst = newTemp();
        emit(IRInstruction::makeCall(dst, mc->objName + "_" + mc->methodName, args));
        return dst;
    }

    if (auto* ma = dynamic_cast<MemberAccessAST*>(node)) {
        IRValue dst = newTemp();
        IRInstruction ins; ins.op = IROp::LOAD;
        ins.arg1 = IRValue::makeVar(ma->objName + "." + ma->memberName);
        ins.result = dst;
        emit(ins);
        return dst;
    }

    if (auto* aa = dynamic_cast<ArrayAccessAST*>(node)) {
        IRValue idx = genExpr(aa->index.get());
        IRValue dst = newTemp();
        emit(IRInstruction::makeBinop(IROp::ARRAY_LOAD, dst,
                                      IRValue::makeVar(aa->name), idx));
        return dst;
    }

    if (auto* ao = dynamic_cast<AddressOfAST*>(node)) {
        IRValue dst = newTemp();
        emit(IRInstruction::makeUnop(IROp::ADDR_OF, dst, IRValue::makeVar(ao->name)));
        return dst;
    }

    if (auto* dr = dynamic_cast<DerefAST*>(node)) {
        IRValue ptr = genExpr(dr->operand.get());
        IRValue dst = newTemp();
        emit(IRInstruction::makeUnop(IROp::LOAD, dst, ptr));
        return dst;
    }

    if (dynamic_cast<NewExprAST*>(node) || dynamic_cast<NewScalarAST*>(node)) {
        IRValue dst  = newTemp();
        IRValue size = IRValue::makeInt(8);
        IRInstruction ins; ins.op = IROp::ALLOC;
        ins.result = dst; ins.arg1 = size;
        emit(ins);
        return dst;
    }

    if (auto* sl = dynamic_cast<StrLenAST*>(node)) {
        IRValue dst = newTemp();
        emit(IRInstruction::makeCall(dst, "strlen", {IRValue::makeVar(sl->varName)}));
        return dst;
    }
    if (auto* sc = dynamic_cast<StrCmpAST*>(node)) {
        IRValue lv = genExpr(sc->lhs.get()), rv = genExpr(sc->rhs.get());
        IRValue dst = newTemp();
        emit(IRInstruction::makeCall(dst, "strcmp", {lv, rv}));
        return dst;
    }

    if (auto* ta = dynamic_cast<ThisAccessAST*>(node)) {
        IRValue dst = newTemp();
        IRInstruction ins; ins.op = IROp::LOAD;
        ins.arg1 = IRValue::makeVar("this." + ta->memberName);
        ins.result = dst;
        emit(ins);
        return dst;
    }

    return IRValue::makeInt(0);
}

void IRBuilder::genStmt(AST* node) {
    if (!node || node->isNoOp()) return;

    if (auto* vi = dynamic_cast<VarDeclInitAST*>(node)) {
        IRValue val = genExpr(vi->init.get());
        emit(IRInstruction::makeAssign(IRValue::makeVar(vi->name), val));
        dag.clear(); return;
    }
    if (auto* cv = dynamic_cast<ConstVarDeclInitAST*>(node)) {
        IRValue val = genExpr(cv->init.get());
        emit(IRInstruction::makeAssign(IRValue::makeVar(cv->name), val));
        return;
    }
    if (auto* vd = dynamic_cast<VarDeclAST*>(node)) {
        IRValue zero = (vd->type == ASTType::Float)
            ? IRValue::makeFloat(0.0) : IRValue::makeInt(0);
        emit(IRInstruction::makeAssign(IRValue::makeVar(vd->name), zero));
        return;
    }
    if (auto* a = dynamic_cast<AssignAST*>(node)) {
        IRValue val = genExpr(a->expr.get());
        emit(IRInstruction::makeAssign(IRValue::makeVar(a->name), val));
        dag.clear(); return;
    }
    if (auto* aa = dynamic_cast<ArrayAssignAST*>(node)) {
        IRValue idx = genExpr(aa->index.get());
        IRValue val = genExpr(aa->expr.get());
        IRInstruction ins; ins.op = IROp::ARRAY_STORE;
        ins.arg1 = IRValue::makeVar(aa->name); ins.arg2 = idx; ins.result = val;
        emit(ins); dag.clear(); return;
    }
    if (auto* ad = dynamic_cast<ArrayDeclAST*>(node)) {
        IRInstruction ins; ins.op = IROp::ALLOC;
        ins.result = IRValue::makeVar(ad->name);
        ins.arg1   = IRValue::makeInt(ad->size);
        emit(ins); varMap[ad->name] = ins.result; return;
    }
    if (auto* sv = dynamic_cast<StringVarDeclAST*>(node)) {
        IRValue val = sv->init ? genExpr(sv->init.get()) : IRValue::makeVar("\"\"");
        emit(IRInstruction::makeAssign(IRValue::makeVar(sv->name), val));
        return;
    }
    if (auto* ma = dynamic_cast<MemberAssignAST*>(node)) {
        IRValue val = genExpr(ma->expr.get());
        IRInstruction ins; ins.op = IROp::STORE;
        ins.arg1 = IRValue::makeVar(ma->objName + "." + ma->memberName);
        ins.arg2 = val; emit(ins); dag.clear(); return;
    }
    if (auto* ta = dynamic_cast<ThisAssignAST*>(node)) {
        IRValue val = genExpr(ta->expr.get());
        IRInstruction ins; ins.op = IROp::STORE;
        ins.arg1 = IRValue::makeVar("this." + ta->memberName);
        ins.arg2 = val; emit(ins); return;
    }
    if (auto* da = dynamic_cast<DerefAssignAST*>(node)) {
        IRValue ptr = genExpr(da->ptr.get());
        IRValue val = genExpr(da->expr.get());
        IRInstruction ins; ins.op = IROp::STORE;
        ins.arg1 = ptr; ins.arg2 = val; emit(ins); dag.clear(); return;
    }
    if (auto* del = dynamic_cast<DeleteAST*>(node)) {
        IRInstruction ins; ins.op = IROp::FREE;
        ins.arg1 = IRValue::makeVar(del->ptrName); emit(ins); return;
    }
    if (auto* ret = dynamic_cast<ReturnAST*>(node)) {
        if (ret->expr) emit(IRInstruction::makeReturn(genExpr(ret->expr.get())));
        else           emit(IRInstruction::makeReturnVoid());
        return;
    }
    if (auto* ifs = dynamic_cast<IfAST*>(node)) {
        std::string thenLbl = newLabel("if_t"), elseLbl = newLabel("if_e"),
                    mergeLbl = newLabel("if_m");
        IRValue cond = genExpr(ifs->cond.get());
        emit(IRInstruction::makeCJump(cond, thenLbl, elseLbl));
        switchTo(currentFn->addBlock(thenLbl));
        genStmt(ifs->thenBlock.get());
        if (!currentBB->isTerminated()) emit(IRInstruction::makeJump(mergeLbl));
        switchTo(currentFn->addBlock(elseLbl));
        if (ifs->elseBlock) genStmt(ifs->elseBlock.get());
        if (!currentBB->isTerminated()) emit(IRInstruction::makeJump(mergeLbl));
        switchTo(currentFn->addBlock(mergeLbl));
        return;
    }
    if (auto* w = dynamic_cast<WhileAST*>(node)) {
        std::string condLbl = newLabel("wh_c"), bodyLbl = newLabel("wh_b"),
                    endLbl  = newLabel("wh_e");
        emit(IRInstruction::makeJump(condLbl));
        switchTo(currentFn->addBlock(condLbl));
        IRValue cond = genExpr(w->cond.get());
        emit(IRInstruction::makeCJump(cond, bodyLbl, endLbl));
        switchTo(currentFn->addBlock(bodyLbl));
        breakTargets.push_back(endLbl); continueTargets.push_back(condLbl);
        genStmt(w->body.get());
        breakTargets.pop_back(); continueTargets.pop_back();
        if (!currentBB->isTerminated()) emit(IRInstruction::makeJump(condLbl));
        switchTo(currentFn->addBlock(endLbl));
        return;
    }
    if (auto* f = dynamic_cast<ForAST*>(node)) {
        if (f->init) genStmt(f->init.get());
        std::string condLbl = newLabel("fo_c"), bodyLbl = newLabel("fo_b"),
                    incLbl  = newLabel("fo_i"), endLbl  = newLabel("fo_e");
        emit(IRInstruction::makeJump(condLbl));
        switchTo(currentFn->addBlock(condLbl));
        if (f->cond) {
            IRValue c = genExpr(f->cond.get());
            emit(IRInstruction::makeCJump(c, bodyLbl, endLbl));
        } else {
            emit(IRInstruction::makeJump(bodyLbl));
        }
        switchTo(currentFn->addBlock(bodyLbl));
        breakTargets.push_back(endLbl); continueTargets.push_back(incLbl);
        genStmt(f->body.get());
        breakTargets.pop_back(); continueTargets.pop_back();
        if (!currentBB->isTerminated()) emit(IRInstruction::makeJump(incLbl));
        switchTo(currentFn->addBlock(incLbl));
        if (f->inc) genExpr(f->inc.get());
        emit(IRInstruction::makeJump(condLbl));
        switchTo(currentFn->addBlock(endLbl));
        return;
    }
    if (dynamic_cast<BreakAST*>(node)) {
        if (!breakTargets.empty())
            emit(IRInstruction::makeJump(breakTargets.back()));
        return;
    }
    if (dynamic_cast<ContinueAST*>(node)) {
        if (!continueTargets.empty())
            emit(IRInstruction::makeJump(continueTargets.back()));
        return;
    }
    if (auto* blk = dynamic_cast<BlockAST*>(node)) { genBlock(blk); return; }
    if (auto* od = dynamic_cast<ObjectDeclAST*>(node)) {
        IRInstruction ins; ins.op = IROp::ALLOC;
        ins.result = IRValue::makeVar(od->varName);
        ins.arg1   = IRValue::makeInt(8);
        emit(ins); varMap[od->varName] = ins.result; return;
    }
    if (auto* pr = dynamic_cast<PrintAST*>(node)) {
        std::vector<IRValue> args;
        for (auto& e : pr->exprs) args.push_back(genExpr(e.get()));
        emit(IRInstruction::makeCallVoid(pr->newline ? "println" : "print", args));
        return;
    }
    if (auto* sc = dynamic_cast<ScanAST*>(node)) {
        for (auto& v : sc->varNames)
            emit(IRInstruction::makeCallVoid("scan", {IRValue::makeVar(v)}));
        return;
    }
    // Expression statement (method call, post-inc, etc.)
    genExpr(node);
}

void IRBuilder::genBlock(BlockAST* blk) {
    if (!blk) return;
    for (auto& s : blk->statements) genStmt(s.get());
}

void IRBuilder::genFunction(FunctionAST* fn) {
    if (!fn || !fn->proto) return;
    currentFn = module->addFunction(fn->proto->name);
    currentFn->returnType = astTypeName(fn->proto->returnType);
    for (size_t i = 0; i < fn->proto->args.size(); i++) {
        std::string t = (i < fn->proto->argTypes.size())
            ? astTypeName(fn->proto->argTypes[i]) : "int";
        currentFn->params.push_back({t, fn->proto->args[i]});
    }
    BasicBlock* entry = currentFn->addBlock("entry");
    entry->isEntry = true;
    switchTo(entry);
    varMap.clear();
    for (auto& [type, name] : currentFn->params)
        varMap[name] = IRValue::makeVar(name, type == "float");

    genBlock(fn->body.get());

    if (currentBB && !currentBB->isTerminated()) {
        if (fn->proto->returnType == ASTType::Void)
            emit(IRInstruction::makeReturnVoid());
        else
            emit(IRInstruction::makeReturn(IRValue::makeInt(0)));
    }
    for (auto& bb : currentFn->blocks) {
        auto* term = bb->terminator();
        if (term && (term->op == IROp::RETURN || term->op == IROp::RETURN_VOID))
            bb->isExit = true;
    }
    currentFn->rebuildEdges();
    currentFn->markReachable();
}

void IRBuilder::genClass(ClassDeclAST* cls) {
    for (auto& m : cls->methods) genFunction(m.get());
}