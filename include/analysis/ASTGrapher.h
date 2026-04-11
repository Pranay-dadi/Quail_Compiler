#pragma once
// ============================================================
//  ASTGrapher.h  —  Quail Compiler
//
//  Walks the AST and produces a Graphviz DOT file.
//  Each AST node type gets a distinct color and shape so the
//  resulting graph is easy to read at a glance.
//
//  Color coding:
//    Dark blue   — Program / Function / Class roots
//    Blue        — Literals, Variables
//    Yellow/Gold — Binary / Logical / Unary expressions
//    Green       — Declarations (var, array, object)
//    Orange      — Control flow (if/while/for/return/break)
//    Teal        — I/O (print/println/scan)
//    Purple      — OOP (class, method, this, member access)
//    Grey        — Comments / unknown nodes
// ============================================================
#pragma once
#include "parser/AST.h"
#include <string>
#include <sstream>

class ASTGrapher {
public:
    // Generate a full DOT string for the given AST root.
    std::string generateDOT(AST* root,
                             const std::string& title = "Quail AST") const;

    // Save directly to a .dot file.
    bool saveDOT(AST* root,
                 const std::string& filename,
                 const std::string& title = "Quail AST") const;

    // Try to render the DOT file to PNG/SVG via graphviz.
    // Returns false if `dot` is not in PATH.
    static bool render(const std::string& dotPath,
                       const std::string& outPath,
                       const std::string& fmt = "png");

    // Quick statistics over the AST (node counts by category).
    struct ASTStats {
        int totalNodes      = 0;
        int expressions     = 0;  // BinaryAST, LogicalAST, UnaryAST, etc.
        int statements      = 0;  // If, While, For, Return, Break, Continue
        int declarations    = 0;  // VarDecl, ArrayDecl, FunctionAST, ClassDecl
        int ioNodes         = 0;  // Print, Scan
        int oopNodes        = 0;  // MemberAccess, MethodCall, This*, ClassDecl
        int commentNodes    = 0;
        int maxDepth        = 0;
    };
    ASTStats computeStats(AST* root) const;
    void     printStats(const ASTStats& s) const;

private:
    mutable int counter_ = 0;   // unique node-ID counter (mutable for const methods)

    // Returns the DOT node-ID string for this node; appends node definition
    // and all child edges to `oss`.
    std::string visitNode(AST* node, std::ostringstream& oss, int depth = 0) const;

    // Generates a fresh unique ID
    std::string mkId() const { return "n" + std::to_string(++counter_); }

    // Per-node visual attributes
    struct Attrs {
        std::string label;      // text shown inside the node
        std::string fill;       // background colour
        std::string shape;      // DOT shape name
        std::string textColor;  // "white" or "black"
        std::string penWidth;   // "1.0" .. "3.0"
    };
    Attrs attrsFor(AST* node) const;

    // Escape a string for use inside a DOT label
    std::string esc(const std::string& s) const;

    // Recursive stats helper
    void statsVisit(AST* node, ASTStats& s, int depth) const;
};