// ============================================================
//  ASTGrapher.cpp  —  Quail Compiler
// ============================================================
#include "analysis/ASTGrapher.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <algorithm>

// ── ANSI helpers (local) ──────────────────────────────────────
namespace { const char* R="\033[0m",*B="\033[1m",*GR="\033[1;32m",*CY="\033[1;36m",*MG="\033[1;35m",*DM="\033[2m"; }

// ─────────────────────────────────────────────────────────────
//  esc()  — escape special DOT characters inside a label
// ─────────────────────────────────────────────────────────────
std::string ASTGrapher::esc(const std::string& s) const {
    std::string r;
    r.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  r += "\\\""; break;
            case '\\': r += "\\\\"; break;
            case '\n': r += "\\n";  break;
            case '\r': break;
            case '<':  r += "\\<";  break;
            case '>':  r += "\\>";  break;
            case '{':  r += "\\{";  break;
            case '}':  r += "\\}";  break;
            case '|':  r += "\\|";  break;
            default:   r += c;
        }
    }
    return r;
}

// ─────────────────────────────────────────────────────────────
//  attrsFor()  — choose color/shape/label for every AST node
// ─────────────────────────────────────────────────────────────
ASTGrapher::Attrs ASTGrapher::attrsFor(AST* node) const {
    Attrs a;
    a.shape     = "box";
    a.textColor = "black";
    a.penWidth  = "1.2";

    // ── Program root ──────────────────────────────────────────
    if (dynamic_cast<ProgramAST*>(node)) {
        a.label = "PROGRAM"; a.fill = "#1a252f";
        a.shape = "doubleoctagon"; a.textColor = "white"; a.penWidth = "3.0";
    }

    // ── Literals ─────────────────────────────────────────────
    else if (auto* n = dynamic_cast<NumberAST*>(node)) {
        a.label = "int\\n" + std::to_string(n->val);
        a.fill = "#aed6f1"; a.shape = "ellipse";
    } else if (auto* f = dynamic_cast<FloatAST*>(node)) {
        std::ostringstream v; v << f->val;
        a.label = "float\\n" + v.str();
        a.fill = "#85c1e9"; a.shape = "ellipse";
    } else if (auto* s = dynamic_cast<StringAST*>(node)) {
        std::string v = s->value;
        if (v.size() > 18) v = v.substr(0,18) + "…";
        a.label = "String\\n\\\"" + esc(v) + "\\\"";
        a.fill = "#7fb3d3"; a.shape = "ellipse";
    } else if (auto* v = dynamic_cast<VariableAST*>(node)) {
        a.label = "Var\\n" + v->name;
        a.fill = "#a9cce3"; a.shape = "ellipse";
    }

    // ── Expressions ───────────────────────────────────────────
    else if (auto* b = dynamic_cast<BinaryAST*>(node)) {
        a.label = "BinOp " + esc(b->op);
        a.fill = "#f9e79f"; a.shape = "diamond"; a.penWidth = "1.8";
    } else if (auto* l = dynamic_cast<LogicalAST*>(node)) {
        a.label = "LogOp " + esc(l->op);
        a.fill = "#f0b27a"; a.shape = "diamond"; a.penWidth = "1.8";
    } else if (auto* u = dynamic_cast<UnaryAST*>(node)) {
        a.label = "Unary\\n" + esc(u->op) + "expr";
        a.fill = "#fdebd0"; a.shape = "diamond";
    } else if (auto* pi = dynamic_cast<PostIncAST*>(node)) {
        a.label = "PostInc\\n" + pi->name + "++";
        a.fill = "#fdebd0"; a.shape = "ellipse";
    }

    // ── Declarations ──────────────────────────────────────────
    else if (auto* d = dynamic_cast<VarDeclAST*>(node)) {
        a.label = "VarDecl\\n" + astTypeName(d->type) + " " + d->name;
        a.fill = "#abebc6";
    } else if (auto* d = dynamic_cast<VarDeclInitAST*>(node)) {
        a.label = "VarDeclInit\\n" + astTypeName(d->type) + " " + d->name + " =";
        a.fill = "#82e0aa";
    } else if (auto* d = dynamic_cast<ArrayDeclAST*>(node)) {
        a.label = "ArrayDecl\\n" + astTypeName(d->type) + " " + d->name +
                  "[" + std::to_string(d->size) + "]";
        a.fill = "#a8d5a2";
    }

    // ── Assignment ────────────────────────────────────────────
    else if (auto* a2 = dynamic_cast<AssignAST*>(node)) {
        a.label = "Assign\\n" + a2->name + " =";
        a.fill = "#fad7a0";
    } else if (auto* aa = dynamic_cast<ArrayAssignAST*>(node)) {
        a.label = "ArrayAssign\\n" + aa->name + "[…] =";
        a.fill = "#f8c471";
    } else if (auto* aa = dynamic_cast<ArrayAccessAST*>(node)) {
        a.label = "ArrayAccess\\n" + aa->name + "[…]";
        a.fill = "#f8d7a0"; a.shape = "ellipse";
    }

    // ── Control flow ──────────────────────────────────────────
    else if (dynamic_cast<IfAST*>(node)) {
        a.label = "IF"; a.fill = "#f0863d";
        a.shape = "diamond"; a.textColor = "white"; a.penWidth = "2.0";
    } else if (dynamic_cast<WhileAST*>(node)) {
        a.label = "WHILE"; a.fill = "#e67e22";
        a.shape = "hexagon"; a.textColor = "white"; a.penWidth = "2.0";
    } else if (dynamic_cast<ForAST*>(node)) {
        a.label = "FOR"; a.fill = "#d68910";
        a.shape = "hexagon"; a.textColor = "white"; a.penWidth = "2.0";
    } else if (dynamic_cast<BreakAST*>(node)) {
        a.label = "BREAK"; a.fill = "#f1948a";
    } else if (dynamic_cast<ContinueAST*>(node)) {
        a.label = "CONTINUE"; a.fill = "#f1948a";
    } else if (dynamic_cast<ReturnAST*>(node)) {
        a.label = "RETURN"; a.fill = "#c0392b";
        a.textColor = "white"; a.penWidth = "2.0";
    } else if (dynamic_cast<BlockAST*>(node)) {
        a.label = "{ Block }"; a.fill = "#e8e8e8";
        a.shape = "rectangle";
    }

    // ── Functions ─────────────────────────────────────────────
    else if (auto* fn = dynamic_cast<FunctionAST*>(node)) {
        std::string nm = fn->proto ? fn->proto->name : "?";
        std::string rt = fn->proto ? astTypeName(fn->proto->returnType) : "?";
        a.label = "Function\\n" + rt + " " + nm + "(…)";
        a.fill = "#1a5276"; a.shape = "tab";
        a.textColor = "white"; a.penWidth = "2.5";
    } else if (auto* p = dynamic_cast<PrototypeAST*>(node)) {
        a.label = "Proto\\n" + p->name + "(" + std::to_string(p->args.size()) + " params)";
        a.fill = "#2874a6"; a.textColor = "white";
    } else if (auto* c = dynamic_cast<CallAST*>(node)) {
        a.label = "Call\\n" + c->callee + "()";
        a.fill = "#2e86c1"; a.textColor = "white";
    }

    // ── I/O ───────────────────────────────────────────────────
    else if (auto* pr = dynamic_cast<PrintAST*>(node)) {
        a.label = pr->newline ? "PRINTLN" : "PRINT";
        a.fill = "#1abc9c"; a.textColor = "white"; a.penWidth = "2.0";
    } else if (auto* sc = dynamic_cast<ScanAST*>(node)) {
        std::string vars;
        for (size_t i = 0; i < sc->varNames.size(); i++) {
            if (i) vars += ", ";
            vars += sc->varNames[i];
        }
        a.label = "SCAN\\n(" + vars + ")";
        a.fill = "#16a085"; a.textColor = "white"; a.penWidth = "2.0";
    }

    // ── OOP ───────────────────────────────────────────────────
    else if (auto* cl = dynamic_cast<ClassDeclAST*>(node)) {
        a.label = "CLASS\\n" + cl->name +
                  " (" + std::to_string(cl->fields.size()) + " fields, " +
                  std::to_string(cl->methods.size()) + " methods)";
        a.fill = "#7d3c98"; a.shape = "tab";
        a.textColor = "white"; a.penWidth = "2.5";
    } else if (auto* od = dynamic_cast<ObjectDeclAST*>(node)) {
        a.label = "ObjDecl\\n" + od->className + " " + od->varName;
        a.fill = "#9b59b6"; a.textColor = "white";
    } else if (auto* ma = dynamic_cast<MemberAccessAST*>(node)) {
        a.label = "MemberAccess\\n" + ma->objName + "." + ma->memberName;
        a.fill = "#a569bd"; a.shape = "ellipse";
    } else if (auto* ma = dynamic_cast<MemberAssignAST*>(node)) {
        a.label = "MemberAssign\\n" + ma->objName + "." + ma->memberName + " =";
        a.fill = "#8e44ad";
    } else if (auto* mc = dynamic_cast<MethodCallAST*>(node)) {
        a.label = "MethodCall\\n" + mc->objName + "." + mc->methodName + "()";
        a.fill = "#6c3483"; a.textColor = "white"; a.penWidth = "2.0";
    } else if (auto* ta = dynamic_cast<ThisAccessAST*>(node)) {
        a.label = "this." + ta->memberName;
        a.fill = "#a569bd"; a.shape = "ellipse";
    } else if (auto* ta = dynamic_cast<ThisAssignAST*>(node)) {
        a.label = "this." + ta->memberName + " =";
        a.fill = "#8e44ad";
    }

    // ── Comments ──────────────────────────────────────────────
    else if (auto* lc = dynamic_cast<LineCommentAST*>(node)) {
        std::string t = lc->text;
        if (t.size() > 25) t = t.substr(0,25) + "…";
        a.label = "// " + esc(t); a.fill = "#d5d8dc";
        a.shape = "note"; a.textColor = "#555555";
    } else if (dynamic_cast<BlockCommentAST*>(node)) {
        a.label = "/* … */"; a.fill = "#d5d8dc";
        a.shape = "note"; a.textColor = "#555555";
    }

    // ── Fallback ──────────────────────────────────────────────
    else {
        a.label = "?\\n" + std::string(typeid(*node).name()).substr(0,20);
        a.fill = "#e0e0e0";
    }

    return a;
}

// ─────────────────────────────────────────────────────────────
//  visitNode()  — recursively emit node + edges
// ─────────────────────────────────────────────────────────────
std::string ASTGrapher::visitNode(AST* node, std::ostringstream& oss,
                                   int depth) const {
    if (!node) return "";

    std::string id = mkId();
    Attrs a = attrsFor(node);

    oss << "  " << id << " [\n"
        << "    label=\""   << a.label     << "\"\n"
        << "    shape="     << a.shape     << "\n"
        << "    style=\"filled,rounded\"\n"
        << "    fillcolor=\"" << a.fill    << "\"\n"
        << "    fontcolor=\"" << a.textColor << "\"\n"
        << "    penwidth="  << a.penWidth  << "\n"
        << "    fontname=\"Arial\"\n"
        << "    fontsize=10\n"
        << "  ];\n";

    // Helper: visit one child and add an edge
    auto child = [&](AST* c, const std::string& lbl = "") {
        if (!c) return;
        std::string cid = visitNode(c, oss, depth + 1);
        oss << "  " << id << " -> " << cid;
        if (!lbl.empty())
            oss << " [label=\"" << lbl << "\", fontsize=8, color=\"#888888\"]";
        else
            oss << " [color=\"#555555\", arrowsize=0.7]";
        oss << ";\n";
    };

    // ── Dispatch children by type ─────────────────────────────
    if (auto* prog = dynamic_cast<ProgramAST*>(node)) {
        for (auto& item : prog->topLevel) child(item.get());

    } else if (auto* blk = dynamic_cast<BlockAST*>(node)) {
        int i = 0;
        for (auto& stmt : blk->statements)
            child(stmt.get(), std::to_string(i++));

    } else if (auto* fn = dynamic_cast<FunctionAST*>(node)) {
        if (fn->proto) child(fn->proto.get(), "proto");
        if (fn->body)  child(fn->body.get(),  "body");

    } else if (auto* cls = dynamic_cast<ClassDeclAST*>(node)) {
        for (auto& m : cls->methods)
            child(m.get(), "method");

    } else if (auto* n = dynamic_cast<IfAST*>(node)) {
        child(n->cond.get(),      "cond");
        child(n->thenBlock.get(), "then");
        if (n->elseBlock) child(n->elseBlock.get(), "else");

    } else if (auto* w = dynamic_cast<WhileAST*>(node)) {
        child(w->cond.get(), "cond");
        child(w->body.get(), "body");

    } else if (auto* f = dynamic_cast<ForAST*>(node)) {
        if (f->init) child(f->init.get(), "init");
        if (f->cond) child(f->cond.get(), "cond");
        if (f->inc)  child(f->inc.get(),  "inc");
        if (f->body) child(f->body.get(), "body");

    } else if (auto* b = dynamic_cast<BinaryAST*>(node)) {
        child(b->lhs.get(), "lhs");
        child(b->rhs.get(), "rhs");

    } else if (auto* l = dynamic_cast<LogicalAST*>(node)) {
        child(l->lhs.get(), "lhs");
        child(l->rhs.get(), "rhs");

    } else if (auto* u = dynamic_cast<UnaryAST*>(node)) {
        child(u->operand.get());

    } else if (auto* r = dynamic_cast<ReturnAST*>(node)) {
        if (r->expr) child(r->expr.get(), "val");

    } else if (auto* a = dynamic_cast<AssignAST*>(node)) {
        child(a->expr.get(), "rhs");

    } else if (auto* vi = dynamic_cast<VarDeclInitAST*>(node)) {
        child(vi->init.get(), "init");

    } else if (auto* aa = dynamic_cast<ArrayAssignAST*>(node)) {
        child(aa->index.get(), "idx");
        child(aa->expr.get(),  "val");

    } else if (auto* acc = dynamic_cast<ArrayAccessAST*>(node)) {
        child(acc->index.get(), "idx");

    } else if (auto* c = dynamic_cast<CallAST*>(node)) {
        for (size_t i = 0; i < c->args.size(); i++)
            child(c->args[i].get(), "arg" + std::to_string(i));

    } else if (auto* mc = dynamic_cast<MethodCallAST*>(node)) {
        for (size_t i = 0; i < mc->args.size(); i++)
            child(mc->args[i].get(), "arg" + std::to_string(i));

    } else if (auto* ma = dynamic_cast<MemberAssignAST*>(node)) {
        child(ma->expr.get(), "rhs");

    } else if (auto* ta = dynamic_cast<ThisAssignAST*>(node)) {
        child(ta->expr.get(), "rhs");

    } else if (auto* pr = dynamic_cast<PrintAST*>(node)) {
        for (size_t i = 0; i < pr->exprs.size(); i++)
            child(pr->exprs[i].get(), "arg" + std::to_string(i));
    }

    return id;
}

// ─────────────────────────────────────────────────────────────
//  generateDOT()
// ─────────────────────────────────────────────────────────────
std::string ASTGrapher::generateDOT(AST* root,
                                     const std::string& title) const {
    counter_ = 0;
    std::ostringstream body;
    visitNode(root, body, 0);

    std::ostringstream dot;
    dot << "digraph AST {\n";
    dot << "  graph [\n"
        << "    label=\"" << esc(title) << "\"\n"
        << "    fontsize=16\n"
        << "    labelloc=t\n"
        << "    bgcolor=\"#fafafa\"\n"
        << "    rankdir=TB\n"
        << "    nodesep=0.5\n"
        << "    ranksep=0.7\n"
        << "  ];\n"
        << "  edge [arrowsize=0.7];\n\n"
        << "  // ── Legend ──────────────────────────────────\n"
        << "  legend [shape=note, style=filled, fillcolor=\"#fffbe6\","
        << " fontsize=8, fontname=Arial, label=\""
        << "LEGEND\\l"
        << "  Dark blue  = Program / Function / Class\\l"
        << "  Light blue = Literals, Variables\\l"
        << "  Yellow     = Expressions (BinOp, LogOp)\\l"
        << "  Green      = Declarations\\l"
        << "  Orange     = Control flow\\l"
        << "  Teal       = I/O (print, scan)\\l"
        << "  Purple     = OOP (class, method, this)\\l"
        << "  Grey/Note  = Comments\\l"
        << "\"];\n\n"
        << body.str()
        << "}\n";
    return dot.str();
}

// ─────────────────────────────────────────────────────────────
//  saveDOT()
// ─────────────────────────────────────────────────────────────
bool ASTGrapher::saveDOT(AST* root, const std::string& filename,
                          const std::string& title) const {
    std::ofstream f(filename);
    if (!f.is_open()) return false;
    f << generateDOT(root, title);
    return true;
}

bool ASTGrapher::render(const std::string& dotPath,
                         const std::string& outPath,
                         const std::string& fmt) {
    std::string cmd = "dot -T" + fmt + " \"" + dotPath +
                      "\" -o \"" + outPath + "\" 2>/dev/null";
    return std::system(cmd.c_str()) == 0;
}

// ─────────────────────────────────────────────────────────────
//  computeStats() / statsVisit()
// ─────────────────────────────────────────────────────────────
void ASTGrapher::statsVisit(AST* node, ASTStats& s, int depth) const {
    if (!node) return;
    s.totalNodes++;
    s.maxDepth = std::max(s.maxDepth, depth);

    // categorize
    if (dynamic_cast<BinaryAST*>(node)  ||
        dynamic_cast<LogicalAST*>(node) ||
        dynamic_cast<UnaryAST*>(node)   ||
        dynamic_cast<PostIncAST*>(node) ||
        dynamic_cast<NumberAST*>(node)  ||
        dynamic_cast<FloatAST*>(node)   ||
        dynamic_cast<StringAST*>(node)  ||
        dynamic_cast<VariableAST*>(node)||
        dynamic_cast<ArrayAccessAST*>(node)) s.expressions++;

    else if (dynamic_cast<IfAST*>(node)    ||
             dynamic_cast<WhileAST*>(node) ||
             dynamic_cast<ForAST*>(node)   ||
             dynamic_cast<BreakAST*>(node) ||
             dynamic_cast<ContinueAST*>(node)||
             dynamic_cast<ReturnAST*>(node) ||
             dynamic_cast<BlockAST*>(node)) s.statements++;

    else if (dynamic_cast<VarDeclAST*>(node)     ||
             dynamic_cast<VarDeclInitAST*>(node)  ||
             dynamic_cast<ArrayDeclAST*>(node)    ||
             dynamic_cast<FunctionAST*>(node)     ||
             dynamic_cast<ClassDeclAST*>(node)) s.declarations++;

    else if (dynamic_cast<PrintAST*>(node) ||
             dynamic_cast<ScanAST*>(node)) s.ioNodes++;

    else if (dynamic_cast<ClassDeclAST*>(node)    ||
             dynamic_cast<ObjectDeclAST*>(node)   ||
             dynamic_cast<MemberAccessAST*>(node) ||
             dynamic_cast<MemberAssignAST*>(node) ||
             dynamic_cast<MethodCallAST*>(node)   ||
             dynamic_cast<ThisAccessAST*>(node)   ||
             dynamic_cast<ThisAssignAST*>(node)) s.oopNodes++;

    else if (dynamic_cast<LineCommentAST*>(node)  ||
             dynamic_cast<BlockCommentAST*>(node)) s.commentNodes++;

    // Recurse into children
    if (auto* prog = dynamic_cast<ProgramAST*>(node))
        for (auto& c : prog->topLevel) statsVisit(c.get(), s, depth+1);
    else if (auto* blk = dynamic_cast<BlockAST*>(node))
        for (auto& c : blk->statements) statsVisit(c.get(), s, depth+1);
    else if (auto* fn = dynamic_cast<FunctionAST*>(node)) {
        if (fn->proto) statsVisit(fn->proto.get(), s, depth+1);
        if (fn->body)  statsVisit(fn->body.get(),  s, depth+1);
    } else if (auto* cls = dynamic_cast<ClassDeclAST*>(node))
        for (auto& m : cls->methods) statsVisit(m.get(), s, depth+1);
    else if (auto* n = dynamic_cast<IfAST*>(node)) {
        statsVisit(n->cond.get(), s, depth+1);
        statsVisit(n->thenBlock.get(), s, depth+1);
        if (n->elseBlock) statsVisit(n->elseBlock.get(), s, depth+1);
    } else if (auto* w = dynamic_cast<WhileAST*>(node)) {
        statsVisit(w->cond.get(), s, depth+1);
        statsVisit(w->body.get(), s, depth+1);
    } else if (auto* f = dynamic_cast<ForAST*>(node)) {
        if (f->init) statsVisit(f->init.get(), s, depth+1);
        if (f->cond) statsVisit(f->cond.get(), s, depth+1);
        if (f->inc)  statsVisit(f->inc.get(),  s, depth+1);
        if (f->body) statsVisit(f->body.get(), s, depth+1);
    } else if (auto* b = dynamic_cast<BinaryAST*>(node)) {
        statsVisit(b->lhs.get(), s, depth+1);
        statsVisit(b->rhs.get(), s, depth+1);
    } else if (auto* l = dynamic_cast<LogicalAST*>(node)) {
        statsVisit(l->lhs.get(), s, depth+1);
        statsVisit(l->rhs.get(), s, depth+1);
    } else if (auto* u = dynamic_cast<UnaryAST*>(node))
        statsVisit(u->operand.get(), s, depth+1);
    else if (auto* r = dynamic_cast<ReturnAST*>(node))
        if (r->expr) statsVisit(r->expr.get(), s, depth+1);
    else if (auto* a = dynamic_cast<AssignAST*>(node))
        statsVisit(a->expr.get(), s, depth+1);
    else if (auto* vi = dynamic_cast<VarDeclInitAST*>(node))
        statsVisit(vi->init.get(), s, depth+1);
    else if (auto* c = dynamic_cast<CallAST*>(node))
        for (auto& arg : c->args) statsVisit(arg.get(), s, depth+1);
    else if (auto* mc = dynamic_cast<MethodCallAST*>(node))
        for (auto& arg : mc->args) statsVisit(arg.get(), s, depth+1);
    else if (auto* pr = dynamic_cast<PrintAST*>(node))
        for (auto& e : pr->exprs) statsVisit(e.get(), s, depth+1);
    else if (auto* aa = dynamic_cast<ArrayAssignAST*>(node)) {
        statsVisit(aa->index.get(), s, depth+1);
        statsVisit(aa->expr.get(),  s, depth+1);
    } else if (auto* ma = dynamic_cast<MemberAssignAST*>(node))
        statsVisit(ma->expr.get(), s, depth+1);
    else if (auto* ta = dynamic_cast<ThisAssignAST*>(node))
        statsVisit(ta->expr.get(), s, depth+1);
}

ASTGrapher::ASTStats ASTGrapher::computeStats(AST* root) const {
    ASTStats s;
    statsVisit(root, s, 0);
    return s;
}

void ASTGrapher::printStats(const ASTStats& s) const {
    std::cout << "\n" << B << CY
              << "── AST Statistics ────────────────────────────────────────\n" << R
              << "  Total nodes    : " << B << s.totalNodes   << R << "\n"
              << "  Expressions    : " << B << s.expressions  << R << "\n"
              << "  Statements     : " << B << s.statements   << R << "\n"
              << "  Declarations   : " << B << s.declarations << R << "\n"
              << "  I/O nodes      : " << B << s.ioNodes      << R << "\n"
              << "  OOP nodes      : " << B << s.oopNodes     << R << "\n"
              << "  Comment nodes  : " << B << s.commentNodes << R << "\n"
              << "  Max tree depth : " << B << s.maxDepth     << R << "\n\n";
}