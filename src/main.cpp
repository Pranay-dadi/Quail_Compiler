// ============================================================
//  Quail Compiler  v4.0  — Analysis Edition
//
//  New in v4.0:
//    • --cfg           Per-function CFG DOT files
//    • --cfg-all       All functions in one clustered DOT
//    • --call-graph    Call graph DOT file
//    • --ast-graph     AST visualisation DOT file
//    • --ast-stats     AST node statistics breakdown
//    • --complexity    Cyclomatic-complexity report
//    • --dead-code     Unreachable basic-block detection
//    • --graph         Enable all graph outputs at once
//    • --render        Auto-render DOTs to PNG via graphviz
//    • --summary       Compact compilation summary table
//
//  Usage:
//    ./Quail_Compiler [FLAGS]  <file.mc>
//    ./Quail_Compiler --test-all [FLAGS]
// ============================================================

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <cstdlib>
#include <filesystem>
#include <algorithm>
#include <unordered_map>
#include <sys/wait.h>

#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/CodeGen.h"
#include "autocorrect/AutoCorrector.h"
#include "analysis/CFGAnalyzer.h"
#include "analysis/ASTGrapher.h"

namespace fs = std::filesystem;

// ── ANSI colour codes ─────────────────────────────────────────
static const char* RED     = "\033[1;31m";
static const char* GREEN   = "\033[1;32m";
static const char* YELLOW  = "\033[1;33m";
static const char* BLUE    = "\033[1;34m";
static const char* MAGENTA = "\033[1;35m";
static const char* CYAN    = "\033[1;36m";
static const char* BOLD    = "\033[1m";
static const char* DIM     = "\033[2m";
static const char* RESET   = "\033[0m";

// ── Forward declarations ──────────────────────────────────────
static std::string tokStr(TokenType t);
static void printTokenTable(const std::vector<Token>& tokens);
static void printCommentSummary(const std::vector<Token>& tokens);
static void printClassRegistry(const std::unordered_map<std::string,ClassInfo>&);
static void printSymbolTable(const std::vector<SymbolLogEntry>&);
static void printOptReport(const OptStats&, OptLevel, const std::string&, const std::string&, bool);
static bool reportErrors(const std::string&, const std::vector<LexError>&,
                         const std::vector<ParseError>&, const std::vector<CodeGenError>&);

// ── BUG FIX: forward-declare these so compileSinglePass can see them ──
struct CompileOptions;
struct CompileResult;
static void printGeneratedFiles(const CompileResult& r, const std::string& stem);
static void runGraphAnalysis(CodeGen& cg, AST* ast, const std::string& outDir,
                             const std::string& stem, const CompileOptions& opts,
                             CompileResult& res, bool verbose);

// ─────────────────────────────────────────────────────────────
//  Compilation options (bundled for clean passing)
// ─────────────────────────────────────────────────────────────
struct CompileOptions {
    bool debugMode      = false;
    bool buildBin       = false;
    bool autoCorrect    = true;
    bool showIrDiff     = false;
    bool genCFG         = false;   // per-function CFG DOT
    bool genCFGAll      = false;   // all-in-one CFG DOT
    bool genCallGraph   = false;   // call graph DOT
    bool genASTGraph    = false;   // AST DOT
    bool showASTStats   = false;
    bool showComplexity = false;
    bool showDeadCode   = false;
    bool renderGraphs   = false;   // call graphviz `dot`
    bool showSummary    = true;
    OptLevel optLevel   = OptLevel::O2;
};

// ─────────────────────────────────────────────────────────────
//  CompileResult
// ─────────────────────────────────────────────────────────────
struct CompileResult {
    bool        parseOk      = false;
    bool        irOk         = false;
    bool        linkOk       = false;
    int         exitCode     = -1;
    int         errorCount   = 0;
    std::string llPath;
    std::string binPath;
    int         commentCount = 0;
    int         classCount   = 0;
    int         lineCount    = 0;
    int         tokenCount   = 0;
    // Generated graph files
    std::vector<std::string> dotFiles;
    std::vector<std::string> pngFiles;
};

// ─────────────────────────────────────────────────────────────
//  Token → display string
// ─────────────────────────────────────────────────────────────
static std::string tokStr(TokenType t) {
    switch (t) {
        case TokenType::INT:           return "INT";
        case TokenType::FLOAT:         return "FLOAT";
        case TokenType::VOID:          return "VOID";
        case TokenType::RETURN:        return "RETURN";
        case TokenType::IF:            return "IF";
        case TokenType::ELSE:          return "ELSE";
        case TokenType::WHILE:         return "WHILE";
        case TokenType::FOR:           return "FOR";
        case TokenType::BREAK:         return "BREAK";
        case TokenType::CONTINUE:      return "CONTINUE";
        case TokenType::CLASS:         return "CLASS";
        case TokenType::NEW:           return "NEW";
        case TokenType::THIS:          return "THIS";
        case TokenType::PUBLIC:        return "PUBLIC";
        case TokenType::PRIVATE:       return "PRIVATE";
        case TokenType::PRINT:         return "PRINT";
        case TokenType::PRINTLN:       return "PRINTLN";
        case TokenType::SCAN:          return "SCAN";
        case TokenType::IDENT:         return "IDENT";
        case TokenType::NUMBER:        return "NUMBER";
        case TokenType::FLOAT_VAL:     return "FLOAT_VAL";
        case TokenType::STRING_LIT:    return "STRING";
        case TokenType::PLUS:          return "PLUS";
        case TokenType::MINUS:         return "MINUS";
        case TokenType::MUL:           return "MUL";
        case TokenType::DIV:           return "DIV";
        case TokenType::ASSIGN:        return "ASSIGN";
        case TokenType::EQ:            return "EQ";
        case TokenType::NEQ:           return "NEQ";
        case TokenType::INC:           return "INC";
        case TokenType::LT:            return "LT";
        case TokenType::GT:            return "GT";
        case TokenType::LE:            return "LE";
        case TokenType::GE:            return "GE";
        case TokenType::AND:           return "AND";
        case TokenType::OR:            return "OR";
        case TokenType::NOT:           return "NOT";
        case TokenType::DOT:           return "DOT";
        case TokenType::LPAREN:        return "LPAREN";
        case TokenType::RPAREN:        return "RPAREN";
        case TokenType::LBRACE:        return "LBRACE";
        case TokenType::RBRACE:        return "RBRACE";
        case TokenType::LBRACKET:      return "LBRACKET";
        case TokenType::RBRACKET:      return "RBRACKET";
        case TokenType::SEMI:          return "SEMI";
        case TokenType::COMMA:         return "COMMA";
        case TokenType::LINE_COMMENT:  return "LINE_CMT";
        case TokenType::BLOCK_COMMENT: return "BLOCK_CMT";
        case TokenType::EOF_TOK:       return "EOF";
        default:                       return "?";
    }
}

// ─────────────────────────────────────────────────────────────
//  Token table (--debug)
// ─────────────────────────────────────────────────────────────
static void printTokenTable(const std::vector<Token>& tokens) {
    const int W1 = 30, W2 = 12;
    std::string sep(W1 + W2 + 22, '-');
    std::cout << sep << "\n";
    std::cout << "| " << std::left << std::setw(W1) << "LEXEME"
              << " | " << std::setw(W2) << "TYPE"
              << " | " << std::setw(4) << "LINE"
              << " | CATEGORY\n" << sep << "\n";
    for (const auto& tk : tokens) {
        std::string cat;
        if (tk.type == TokenType::CLASS  || tk.type == TokenType::NEW   ||
            tk.type == TokenType::THIS   || tk.type == TokenType::PUBLIC ||
            tk.type == TokenType::PRIVATE)
            cat = std::string(MAGENTA) + "OOP_KW"  + RESET;
        else if (tk.type == TokenType::PRINT   ||
                 tk.type == TokenType::PRINTLN  ||
                 tk.type == TokenType::SCAN)
            cat = std::string(CYAN)    + "IO_KW"   + RESET;
        else if (tk.type == TokenType::VOID || tk.type == TokenType::INT  ||
                 tk.type == TokenType::FLOAT || tk.type == TokenType::RETURN ||
                 tk.type == TokenType::IF   || tk.type == TokenType::ELSE  ||
                 tk.type == TokenType::WHILE|| tk.type == TokenType::FOR   ||
                 tk.type == TokenType::BREAK|| tk.type == TokenType::CONTINUE)
            cat = std::string(GREEN)   + "KEYWORD" + RESET;
        else if (tk.type == TokenType::IDENT)
            cat = std::string(CYAN)    + "IDENT"   + RESET;
        else if (tk.type == TokenType::NUMBER    ||
                 tk.type == TokenType::FLOAT_VAL ||
                 tk.type == TokenType::STRING_LIT)
            cat = std::string(YELLOW)  + "LITERAL" + RESET;
        else if (tk.type == TokenType::LINE_COMMENT || tk.type == TokenType::BLOCK_COMMENT)
            cat = std::string(DIM)     + "COMMENT" + RESET;
        else if (tk.type == TokenType::DOT)
            cat = std::string(CYAN)    + "MEMBER"  + RESET;
        else
            cat = std::string(RED)     + "OP/PUNCT"+ RESET;

        std::string lex = tk.lexeme;
        if (lex.size() > (size_t)(W1 - 2)) lex = lex.substr(0, W1 - 5) + "...";
        std::cout << "| " << std::left << std::setw(W1) << lex
                  << " | " << std::setw(W2) << tokStr(tk.type)
                  << " | " << std::setw(4)  << tk.line
                  << " | " << cat << "\n";
        if (tk.type == TokenType::EOF_TOK) break;
    }
    std::cout << sep << "\n";
}

static void printCommentSummary(const std::vector<Token>& tokens) {
    int lineCount = 0, blockCount = 0;
    std::cout << "\n" << DIM << BOLD << "── Comments found in source ──\n" << RESET;
    for (const auto& tk : tokens) {
        if (tk.type == TokenType::LINE_COMMENT) {
            std::cout << DIM << "  line " << std::setw(4) << tk.line
                      << "  // " << tk.lexeme << RESET << "\n";
            ++lineCount;
        } else if (tk.type == TokenType::BLOCK_COMMENT) {
            std::cout << DIM << "  line " << std::setw(4) << tk.line
                      << "  /* " << tk.lexeme.substr(0, 60)
                      << (tk.lexeme.size() > 60 ? "..." : "") << " */" << RESET << "\n";
            ++blockCount;
        }
    }
    if (!lineCount && !blockCount)
        std::cout << DIM << "  (none)\n" << RESET;
    else
        std::cout << DIM << "  Total: " << lineCount << " line, "
                  << blockCount << " block comment(s)\n" << RESET;
}

static void printClassRegistry(
        const std::unordered_map<std::string, ClassInfo>& infos)
{
    if (infos.empty()) return;
    std::cout << "\n" << BOLD << MAGENTA
              << "╔══════════════════════════════════════════════════════════╗\n"
              << "║                   CLASS REGISTRY                        ║\n"
              << "╚══════════════════════════════════════════════════════════╝\n"
              << RESET;
    for (auto& [name, info] : infos) {
        std::cout << "\n  " << BOLD << CYAN << "class " << name << RESET << " {\n";
        if (info.fields.empty())
            std::cout << DIM << "    (no fields)\n" << RESET;
        for (auto& [fn, ft] : info.fields)
            std::cout << "    " << YELLOW << SymbolTable::typeName(ft)
                      << RESET << "  " << fn << ";\n";
        std::cout << "  }\n";
    }
    std::cout << "\n";
}

static void printSymbolTable(const std::vector<SymbolLogEntry>& log) {
    if (log.empty()) {
        std::cout << DIM << "  (symbol table empty)\n" << RESET;
        return;
    }
    std::vector<const SymbolLogEntry*> functions, locals;
    for (const auto& e : log)
        (e.kind == SymbolKind::Function ? functions : locals).push_back(&e);

    std::cout << "\n" << BOLD << CYAN
              << "  ┌─────────────────────────────────────────────────────────┐\n"
              << "  │              FUNCTIONS & METHODS                        │\n"
              << "  └─────────────────────────────────────────────────────────┘\n"
              << RESET;
    if (functions.empty()) {
        std::cout << DIM << "    (none)\n" << RESET;
    } else {
        for (const auto* e : functions) {
            std::string sig = SymbolTable::typeName(e->returnType) + " " + e->name + "(";
            for (size_t i = 0; i < e->paramTypes.size(); ++i) {
                if (i) sig += ", ";
                sig += SymbolTable::typeName(e->paramTypes[i]);
            }
            sig += ")";
            bool isMethod = (e->name.find('_') != std::string::npos);
            std::cout << "  " << (isMethod ? MAGENTA : GREEN) << BOLD
                      << (isMethod ? "  mtd " : "  fn  ") << RESET
                      << BOLD << std::left << std::setw(44) << sig << RESET
                      << DIM << " [global]\n" << RESET;
        }
    }

    std::cout << "\n" << BOLD << CYAN
              << "  ┌─────────────────────────────────────────────────────────┐\n"
              << "  │         VARIABLES, PARAMETERS & OBJECTS                 │\n"
              << "  └─────────────────────────────────────────────────────────┘\n"
              << RESET;
    if (locals.empty()) {
        std::cout << DIM << "    (none)\n" << RESET;
    } else {
        std::string lastOwner = "\x01";
        for (const auto* e : locals) {
            if (e->ownerFunction != lastOwner) {
                lastOwner = e->ownerFunction;
                std::string owner = lastOwner.empty() ? "<global>" : lastOwner;
                std::cout << "\n  " << YELLOW << BOLD << "  in " << owner << "():\n" << RESET;
            }
            const char* kc =
                e->kind == SymbolKind::Parameter ? CYAN    :
                e->kind == SymbolKind::Array     ? MAGENTA :
                e->kind == SymbolKind::Object    ? YELLOW  : BLUE;
            const char* kt =
                e->kind == SymbolKind::Parameter ? "param" :
                e->kind == SymbolKind::Array     ? "array" :
                e->kind == SymbolKind::Object    ? "obj  " : "var  ";
            std::string ts;
            if (e->kind == SymbolKind::Object)
                ts = e->objectClass + " (obj)";
            else {
                ts = SymbolTable::typeName(e->type);
                if (e->kind == SymbolKind::Array)
                    ts += "[" + std::to_string(e->arraySize) + "]";
            }
            std::cout << "    " << kc << BOLD << kt << RESET << "  "
                      << BOLD << std::left << std::setw(20) << e->name << RESET
                      << "  " << std::setw(18) << ts
                      << DIM << " depth=" << e->scopeDepth << RESET << "\n";
        }
    }
    std::cout << "\n";
}

static void printOptReport(const OptStats& s, OptLevel level,
                           const std::string& irBefore,
                           const std::string& irAfter,
                           bool showDiff)
{
    const char* lvlName =
        level == OptLevel::O0 ? "O0 (none)"       :
        level == OptLevel::O1 ? "O1 (basic)"      :
        level == OptLevel::O2 ? "O2 (standard)"   : "O3 (aggressive)";
    std::cout << "\n" << MAGENTA << BOLD
              << "╔══════════════════════════════════════════════════════════╗\n"
              << "║              OPTIMIZATION REPORT                        ║\n"
              << "╚══════════════════════════════════════════════════════════╝\n"
              << RESET << "  Level: " << BOLD << lvlName << RESET << "\n\n";

    const int W = 28, N = 8;
    std::cout << BOLD
              << std::left << std::setw(W) << "Function"
              << std::setw(N) << "Instr↓" << std::setw(N) << "Before" << std::setw(N) << "After"
              << std::setw(N) << "Blks↓"  << std::setw(N) << "Before" << std::setw(N) << "After"
              << "\n" << RESET << std::string(W + N * 6, '-') << "\n";

    for (const auto& fs : s.functions) {
        int id = (int)fs.instrBefore - (int)fs.instrAfter;
        int bd = (int)fs.blocksBefore - (int)fs.blocksAfter;
        auto col = [](int d) -> const char* { return d > 0 ? GREEN : (d < 0 ? RED : RESET); };
        std::cout << std::left << std::setw(W) << fs.name
                  << col(id) << std::setw(N) << id << RESET
                  << std::setw(N) << fs.instrBefore  << std::setw(N) << fs.instrAfter
                  << col(bd) << std::setw(N) << bd << RESET
                  << std::setw(N) << fs.blocksBefore << std::setw(N) << fs.blocksAfter << "\n";
    }

    std::cout << std::string(W + N * 6, '-') << "\n";
    int tid = (int)s.totalInstrBefore  - (int)s.totalInstrAfter;
    int tbd = (int)s.totalBlocksBefore - (int)s.totalBlocksAfter;
    auto col = [](int d) -> const char* { return d > 0 ? GREEN : (d < 0 ? RED : RESET); };
    std::cout << BOLD << std::left << std::setw(W) << "TOTAL"
              << col(tid) << std::setw(N) << tid << RESET << BOLD
              << std::setw(N) << s.totalInstrBefore  << std::setw(N) << s.totalInstrAfter
              << col(tbd) << std::setw(N) << tbd << RESET << BOLD
              << std::setw(N) << s.totalBlocksBefore << std::setw(N) << s.totalBlocksAfter
              << "\n" << RESET;

    if (s.totalInstrBefore > 0)
        std::cout << "\n  " << BOLD << (s.instrReduction() > 0 ? GREEN : RESET)
                  << "Instruction reduction: " << s.instrReduction() << "%"
                  << RESET << "\n";

    if (showDiff && !irBefore.empty() && !irAfter.empty()) {
        auto split = [](const std::string& src) {
            std::vector<std::string> v; std::istringstream ss(src); std::string l;
            while (std::getline(ss, l)) v.push_back(l); return v;
        };
        auto bL = split(irBefore), aL = split(irAfter);
        std::cout << "\n" << BOLD << "── IR diff (before → after) ──\n" << RESET
                  << DIM << "  Before: " << bL.size() << " lines\n"
                  << "  After : " << aL.size() << " lines\n"
                  << "  Removed: " << (int)bL.size() - (int)aL.size() << " lines\n" << RESET;
        std::cout << "\n" << YELLOW << BOLD << "  Optimized IR (first 40 lines):\n" << RESET;
        int shown = 0;
        for (const auto& l : aL) {
            if (shown++ >= 40) {
                std::cout << DIM << "  ... (" << aL.size() - 40 << " more lines)\n" << RESET;
                break;
            }
            std::cout << DIM << std::setw(4) << shown << RESET << "  " << l << "\n";
        }
    }
}

static bool reportErrors(const std::string& filename,
                         const std::vector<LexError>&     lexErrs,
                         const std::vector<ParseError>&   parseErrs,
                         const std::vector<CodeGenError>& cgErrs)
{
    bool any = !lexErrs.empty() || !parseErrs.empty() || !cgErrs.empty();
    if (!any) return false;
    std::cerr << "\n" << RED << BOLD
              << "╔══════════════════════════════════════════════════════════╗\n"
              << "║                   COMPILATION ERRORS                    ║\n"
              << "╚══════════════════════════════════════════════════════════╝\n"
              << RESET << "  File: " << filename << "\n\n";
    int count = 0;
    for (auto& e : lexErrs)   { std::cerr << RED << "[LEX]  " << RESET << filename << ":" << e.line << "  " << e.message << "\n"; ++count; }
    for (auto& e : parseErrs) { std::cerr << RED << "[PARSE]" << RESET << " " << filename << ":" << e.line << "  " << e.message << "\n"; ++count; }
    for (auto& e : cgErrs)    { std::cerr << RED << "[CGEN] " << RESET << e.message << "\n"; ++count; }
    std::cerr << "\n" << RED << BOLD << count << " error" << (count == 1 ? "" : "s")
              << " found." << RESET << " Compilation failed.\n\n";
    return true;
}

// ─────────────────────────────────────────────────────────────
//  printGeneratedFiles()  — summary of all output artifacts
// ─────────────────────────────────────────────────────────────
static void printGeneratedFiles(const CompileResult& r, const std::string& /*stem*/) {
    std::cout << "\n" << BOLD << CYAN
              << "── Generated Files ──────────────────────────────────────\n" << RESET;
    auto printFile = [&](const std::string& path, const std::string& kind) {
        if (!path.empty() && fs::exists(path)) {
            auto sz = fs::file_size(path);
            std::cout << "  " << CYAN << std::left << std::setw(10) << kind << RESET
                      << "  " << path
                      << DIM << "  (" << sz << " B)" << RESET << "\n";
        }
    };
    printFile(r.llPath,  "IR (.ll)");
    printFile(r.binPath, "binary");
    for (auto& d : r.dotFiles)
        printFile(d, "DOT");
    for (auto& p : r.pngFiles)
        printFile(p, "PNG");
    std::cout << "\n";
}

// ─────────────────────────────────────────────────────────────
//  runGraphAnalysis  — CFG, call graph, AST graphing
//
//  BUG FIXES applied here:
//   1. showASTStats is now handled independently of genASTGraph
//      (was previously nested inside the genASTGraph block, causing
//       stats to be silently skipped when genASTGraph=false)
//   2. AST stats are no longer printed twice when both
//      --ast-stats and --ast-graph are active
// ─────────────────────────────────────────────────────────────
static void runGraphAnalysis(CodeGen&           cg,
                              AST*               ast,
                              const std::string& outDir,
                              const std::string& stem,
                              const CompileOptions& opts,
                              CompileResult&     res,
                              bool               verbose)
{
    llvm::Module* mod = cg.getModule();
    if (!mod) {
        if (verbose)
            std::cerr << RED << "  [Graph] LLVM module unavailable — skipping analysis.\n" << RESET;
        return;
    }

    CFGAnalyzer cfgAnalyzer(mod);
    cfgAnalyzer.analyze();

    // ── Complexity report ─────────────────────────────────────
    if (opts.showComplexity)
        cfgAnalyzer.printComplexityReport();

    // ── Dead code report ──────────────────────────────────────
    if (opts.showDeadCode) {
        std::cout << "\n" << BOLD << CYAN
                  << "── Dead-Code Analysis ────────────────────────────────────\n" << RESET;
        cfgAnalyzer.printDeadCodeReport();
    }

    // ── CFG summary (always when verbose + any CFG graph flag) ─
    if (verbose && (opts.genCFG || opts.genCFGAll))
        cfgAnalyzer.printCFGSummary();

    // ── Per-function CFG DOT ──────────────────────────────────
    if (opts.genCFG) {
        for (auto& fn : cfgAnalyzer.getFunctions()) {
            std::string dotPath = outDir + "/" + stem + "_cfg_" + fn.name + ".dot";
            std::string content = cfgAnalyzer.generateFunctionDOT(fn, /*showInstr=*/true);
            if (cfgAnalyzer.saveDOT(dotPath, content)) {
                res.dotFiles.push_back(dotPath);
                if (verbose)
                    std::cout << YELLOW << "  CFG DOT : " << dotPath << RESET << "\n";
                if (opts.renderGraphs) {
                    std::string pngPath = outDir + "/" + stem + "_cfg_" + fn.name + ".png";
                    if (CFGAnalyzer::render(dotPath, pngPath)) {
                        res.pngFiles.push_back(pngPath);
                        if (verbose) std::cout << GREEN << "  CFG PNG : " << pngPath << RESET << "\n";
                    } else if (verbose) {
                        std::cout << DIM << "  (graphviz not found — install with: sudo apt install graphviz)\n" << RESET;
                    }
                }
            } else if (verbose) {
                std::cerr << RED << "  Failed to write DOT: " << dotPath << RESET << "\n";
            }
        }
    }

    // ── All-functions CFG DOT ─────────────────────────────────
    if (opts.genCFGAll) {
        std::string dotPath = outDir + "/" + stem + "_cfg_all.dot";
        if (cfgAnalyzer.saveDOT(dotPath, cfgAnalyzer.generateAllFunctionsDOT())) {
            res.dotFiles.push_back(dotPath);
            if (verbose) std::cout << YELLOW << "  CFG-all DOT: " << dotPath << RESET << "\n";
            if (opts.renderGraphs) {
                std::string pngPath = outDir + "/" + stem + "_cfg_all.png";
                if (CFGAnalyzer::render(dotPath, pngPath)) {
                    res.pngFiles.push_back(pngPath);
                    if (verbose) std::cout << GREEN << "  CFG-all PNG: " << pngPath << RESET << "\n";
                }
            }
        } else if (verbose) {
            std::cerr << RED << "  Failed to write DOT: " << dotPath << RESET << "\n";
        }
    }

    // ── Call graph DOT ────────────────────────────────────────
    if (opts.genCallGraph) {
        std::string dotPath = outDir + "/" + stem + "_callgraph.dot";
        if (cfgAnalyzer.saveDOT(dotPath, cfgAnalyzer.generateCallGraphDOT())) {
            res.dotFiles.push_back(dotPath);
            if (verbose) std::cout << YELLOW << "  Call-graph DOT: " << dotPath << RESET << "\n";
            if (opts.renderGraphs) {
                std::string pngPath = outDir + "/" + stem + "_callgraph.png";
                if (CFGAnalyzer::render(dotPath, pngPath)) {
                    res.pngFiles.push_back(pngPath);
                    if (verbose) std::cout << GREEN << "  Call-graph PNG: " << pngPath << RESET << "\n";
                }
            }
        } else if (verbose) {
            std::cerr << RED << "  Failed to write DOT: " << dotPath << RESET << "\n";
        }
    }

    // ── BUG FIX: AST stats are now independent of genASTGraph ─
    //  Previously: stats were nested inside `if (opts.genASTGraph)`,
    //  so `--ast-stats` alone never printed anything from this function.
    //  Now: stats run whenever showASTStats is set, regardless of
    //  whether the DOT file is also being generated.
    if (opts.showASTStats && ast) {
        ASTGrapher grapher;
        auto stats = grapher.computeStats(ast);
        grapher.printStats(stats);
    }

    // ── AST DOT ───────────────────────────────────────────────
    if (opts.genASTGraph && ast) {
        ASTGrapher grapher;
        std::string dotPath = outDir + "/" + stem + "_ast.dot";
        if (grapher.saveDOT(ast, dotPath, "Quail AST — " + stem)) {
            res.dotFiles.push_back(dotPath);
            if (verbose) std::cout << YELLOW << "  AST DOT: " << dotPath << RESET << "\n";
            if (opts.renderGraphs) {
                std::string pngPath = outDir + "/" + stem + "_ast.png";
                if (ASTGrapher::render(dotPath, pngPath)) {
                    res.pngFiles.push_back(pngPath);
                    if (verbose) std::cout << GREEN << "  AST PNG: " << pngPath << RESET << "\n";
                } else if (verbose) {
                    std::cout << DIM << "  (graphviz not found — install with: sudo apt install graphviz)\n" << RESET;
                }
            }
        } else if (verbose) {
            std::cerr << RED << "  Failed to write AST DOT: " << dotPath << RESET << "\n";
        }
    }

    // ── Summary of what was produced ─────────────────────────
    if (verbose) {
        int nDot = (int)res.dotFiles.size();
        int nPng = (int)res.pngFiles.size();
        if (nDot > 0 || nPng > 0) {
            std::cout << "\n" << DIM << "  Analysis produced: "
                      << nDot << " DOT file(s)";
            if (nPng > 0) std::cout << ", " << nPng << " PNG file(s)";
            if (nDot > 0 && nPng == 0 && opts.renderGraphs == false)
                std::cout << "  (add --render to auto-generate PNGs)";
            std::cout << RESET << "\n";
        }
    }
}

// ═════════════════════════════════════════════════════════════
//  compileSinglePass
// ═════════════════════════════════════════════════════════════
static CompileResult compileSinglePass(const std::string& displayPath,
                                       const std::string& source,
                                       const std::string& outDir,
                                       const std::string& stem,
                                       bool  verbose,
                                       const CompileOptions& opts)
{
    CompileResult res;
    res.llPath  = outDir + "/" + stem + ".ll";
    res.binPath = outDir + "/" + stem;
    std::string objPath = outDir + "/" + stem + ".o";

    // Source line count
    res.lineCount = (int)std::count(source.begin(), source.end(), '\n') + 1;

    // ── LEXER ─────────────────────────────────────────────────
    Lexer lexer(source, true);
    auto tokens    = lexer.tokenize();
    auto lexErrors = lexer.getErrors();
    res.tokenCount = (int)tokens.size() - 1; // exclude EOF

    for (const auto& t : tokens)
        if (t.type == TokenType::LINE_COMMENT || t.type == TokenType::BLOCK_COMMENT)
            ++res.commentCount;

    if (verbose && opts.debugMode) {
        std::cout << "\n" << BLUE << BOLD << "[LEXICAL ANALYSIS]\n" << RESET;
        printTokenTable(tokens);
        printCommentSummary(tokens);
        std::cout << "\n";
    } else if (verbose) {
        std::cout << "--- [TOKEN STREAM] ---\n";
        for (const auto& tk : tokens) {
            if (tk.type == TokenType::LINE_COMMENT)
                std::cout << DIM << "[COMMENT]    line:" << std::setw(4) << tk.line
                          << "  // " << tk.lexeme << RESET << "\n";
            else if (tk.type == TokenType::BLOCK_COMMENT)
                std::cout << DIM << "[COMMENT]    line:" << std::setw(4) << tk.line
                          << "  /* " << tk.lexeme.substr(0, 40)
                          << (tk.lexeme.size() > 40 ? "..." : "") << " */" << RESET << "\n";
            else
                std::cout << "[TOKEN] " << std::left << std::setw(12) << tokStr(tk.type)
                          << "  line:" << std::setw(4) << tk.line
                          << "  \"" << tk.lexeme << "\"\n";
            if (tk.type == TokenType::EOF_TOK) break;
        }
        std::cout << "  " << DIM << "(" << res.commentCount
                  << " comment token(s) preserved)" << RESET << "\n"
                  << "----------------------\n\n";
    }

    // ── PARSER ────────────────────────────────────────────────
    Parser parser(tokens);
    auto ast         = parser.parse();
    auto parseErrors = parser.getErrors();

    if (!lexErrors.empty() || !parseErrors.empty()) {
        res.errorCount = (int)lexErrors.size() + (int)parseErrors.size();
        if (verbose) reportErrors(displayPath, lexErrors, parseErrors, {});
        return res;
    }
    if (!ast) {
        if (verbose) std::cerr << RED << "Parser returned null AST\n" << RESET;
        return res;
    }
    res.parseOk = true;

    if (verbose) {
        if (opts.debugMode) {
            std::cout << BLUE << BOLD << "[AST — with OOP + I/O nodes]\n" << RESET;
            ast->print(0);
        } else {
            std::cout << GREEN << "Frontend OK\n" << RESET;
            std::cout << "--- [AST] ---\n";
            ast->print(0);
            std::cout << "-------------\n\n";
        }
    }

    // NOTE: AST stats are now handled inside runGraphAnalysis so they
    // are never printed twice. The old pre-codegen block was removed.

    // ── CODEGEN ───────────────────────────────────────────────
    CodeGen cg;
    cg.generate(ast.get());
    auto cgErrors = cg.getErrors();

    if (!cgErrors.empty()) {
        res.errorCount = (int)cgErrors.size();
        if (verbose) reportErrors(displayPath, {}, {}, cgErrors);
        return res;
    }

    res.classCount = (int)cg.getClassInfos().size();

    if (verbose) {
        if (!cg.getClassInfos().empty())
            printClassRegistry(cg.getClassInfos());
        std::cout << "\n" << BOLD << CYAN
                  << "╔══════════════════════════════════════════════════════════╗\n"
                  << "║                   SYMBOL TABLE                          ║\n"
                  << "╚══════════════════════════════════════════════════════════╝\n"
                  << RESET;
        printSymbolTable(cg.getSymbolLog());
    }

    // ── OPTIMIZATION ──────────────────────────────────────────
    std::string irBefore;
    if (opts.optLevel != OptLevel::O0 && verbose)
        irBefore = cg.getIRString();

    if (opts.optLevel != OptLevel::O0) {
        if (verbose) {
            const char* lvl = opts.optLevel == OptLevel::O1 ? "O1" :
                              opts.optLevel == OptLevel::O2 ? "O2" : "O3";
            std::cout << "\n" << MAGENTA << BOLD
                      << "── Running optimizer (" << lvl << ") ──\n" << RESET;
        }
        cg.optimize(opts.optLevel);
        if (verbose) {
            std::string irAfter = cg.getIRString();
            printOptReport(cg.getOptStats(), opts.optLevel, irBefore, irAfter, opts.showIrDiff);
        }
    } else if (verbose) {
        std::cout << DIM << "\n  (Optimization disabled: --O0)\n" << RESET;
    }

    // ── EMIT IR ───────────────────────────────────────────────
    cg.dumpToFile(res.llPath);
    res.irOk = fs::exists(res.llPath) && fs::file_size(res.llPath) > 0;

    if (verbose && res.irOk) {
        std::cout << "\n--- [LLVM IR"
                  << (opts.optLevel != OptLevel::O0 ? " (optimized)" : "") << "] ---\n";
        cg.dump();
        std::cout << YELLOW << "\n→ IR written to: " << res.llPath << RESET << "\n";
    }

    // ── GRAPH / ANALYSIS OUTPUTS ──────────────────────────────
    // BUG FIX: added showASTStats to the anyGraph check.
    // Previously, passing only --ast-stats would set showASTStats=true
    // but anyGraph remained false, so runGraphAnalysis was never called
    // and no stats appeared.
    bool anyGraph = opts.genCFG        || opts.genCFGAll    ||
                    opts.genCallGraph  || opts.genASTGraph   ||
                    opts.showComplexity|| opts.showDeadCode  ||
                    opts.showASTStats;   // ← was missing before

    if (anyGraph) {
        if (verbose)
            std::cout << "\n" << BOLD << MAGENTA
                      << "╔══════════════════════════════════════════════════════════╗\n"
                      << "║             PROGRAM ANALYSIS                            ║\n"
                      << "╚══════════════════════════════════════════════════════════╝\n"
                      << RESET;
        runGraphAnalysis(cg, ast.get(), outDir, stem, opts, res, verbose);
    }

    // ── BUILD (optional) ──────────────────────────────────────
    if (opts.buildBin && res.irOk) {
        std::string llcCmd   = "llc -relocation-model=pic "
                               + res.llPath + " -filetype=obj -o " + objPath + " 2>/dev/null";
        std::string clangCmd = "clang " + objPath + " -o " + res.binPath + " 2>/dev/null";
        if (verbose) {
            std::cout << "\n" << BOLD << "Building...\n" << RESET
                      << "  $ " << llcCmd << "\n";
        }
        bool llcOk   = (std::system(llcCmd.c_str())   == 0);
        if (verbose) std::cout << "  $ " << clangCmd << "\n";
        bool clangOk = llcOk && (std::system(clangCmd.c_str()) == 0);
        res.linkOk = clangOk;
        if (res.linkOk) {
            if (verbose)
                std::cout << GREEN << "→ Executable: " << res.binPath << RESET << "\n\n";
            int raw = std::system(res.binPath.c_str());
            res.exitCode = WEXITSTATUS(raw);
            if (verbose)
                std::cout << YELLOW << "Exit code: " << res.exitCode << RESET << "\n";
        } else if (verbose) {
            std::cerr << RED << "[BUILD] llc/clang failed.\n" << RESET;
        }
    } else if (!opts.buildBin && verbose) {
        std::cout << "\n" << BOLD << "Next steps:\n" << RESET
                  << "  llc "   << res.llPath << " -filetype=obj -o " << objPath << "\n"
                  << "  clang " << objPath    << " -o " << res.binPath << " -no-pie\n"
                  << "  " << res.binPath << " ; echo $?\n";
    }

    // ── Print generated files ─────────────────────────────────
    if (verbose && (!res.dotFiles.empty() || !res.pngFiles.empty()))
        printGeneratedFiles(res, stem);

    return res;
}

// ═════════════════════════════════════════════════════════════
//  compileOne — auto-correction wrapper
// ═════════════════════════════════════════════════════════════
static CompileResult compileOne(const std::string& srcPath,
                                const std::string& outDir,
                                bool verbose,
                                const CompileOptions& opts)
{
    fs::path p(srcPath);
    std::string stem = p.stem().string();

    std::ifstream file(srcPath);
    if (!file.is_open()) {
        if (verbose) std::cerr << RED << "Cannot open: " << srcPath << RESET << "\n";
        return {};
    }
    std::stringstream buf; buf << file.rdbuf();
    std::string source = buf.str();

    // Quick error probe pass (no keepComments, no display)
    Lexer lx1(source, false);
    auto toks1      = lx1.tokenize();
    auto lexErrs1   = lx1.getErrors();
    Parser px1(toks1);
    auto ast1       = px1.parse();
    auto parseErrs1 = px1.getErrors();
    std::vector<CodeGenError> cgErrs1;
    if (lexErrs1.empty() && parseErrs1.empty() && ast1) {
        CodeGen cg1; cg1.generate(ast1.get());
        cgErrs1 = cg1.getErrors();
    }

    bool hasErrors = !lexErrs1.empty() || !parseErrs1.empty() || !cgErrs1.empty();

    if (!hasErrors)
        return compileSinglePass(srcPath, source, outDir, stem, verbose, opts);

    if (!opts.autoCorrect) {
        if (verbose) reportErrors(srcPath, lexErrs1, parseErrs1, cgErrs1);
        CompileResult bad;
        bad.errorCount = (int)lexErrs1.size() + (int)parseErrs1.size() + (int)cgErrs1.size();
        return bad;
    }

    if (verbose) {
        std::cout << BOLD << "\n══ PASS 1: Errors detected ══\n" << RESET;
        reportErrors(srcPath, lexErrs1, parseErrs1, cgErrs1);
        std::cout << "\n" << YELLOW << BOLD
                  << "══ AUTO-CORRECTION PHASE ══\n" << RESET
                  << "  Attempting to fix "
                  << (lexErrs1.size() + parseErrs1.size() + cgErrs1.size())
                  << " error(s)...\n";
    }

    AutoCorrector corrector(source, lexErrs1, parseErrs1, cgErrs1);
    std::string corrected = corrector.correct();
    const auto& fixes     = corrector.getCorrections();

    if (verbose) {
        if (fixes.empty()) {
            std::cout << YELLOW << "  No automatic fixes could be applied.\n" << RESET;
        } else {
            std::cout << "\n" << YELLOW << BOLD
                      << "╔══════════════════════════════════════════════════════════╗\n"
                      << "║              AUTO-CORRECTION REPORT                     ║\n"
                      << "╚══════════════════════════════════════════════════════════╝\n"
                      << RESET << "  " << fixes.size() << " fix(es) applied.\n\n";
            for (size_t i = 0; i < fixes.size(); ++i) {
                const auto& f = fixes[i];
                std::cout << CYAN << "  [" << (i+1) << "] [" << f.kind << "] line "
                          << f.line << " — " << f.description << RESET << "\n";
                if (!f.before.empty())
                    std::cout << "       " << RED   << "- " << f.before << RESET << "\n";
                if (!f.after.empty())
                    std::cout << "       " << GREEN << "+ " << f.after  << RESET << "\n";
            }
        }
    }

    std::string corrPath = outDir + "/" + stem + "_corrected.mc";
    { std::ofstream o(corrPath); if (o) o << corrected; }

    if (verbose) {
        std::cout << "\n" << YELLOW << "  Corrected file: " << corrPath << RESET << "\n";
        std::cout << "\n" << BLUE << BOLD << "══ CORRECTED SOURCE ══\n" << RESET;
        std::istringstream crs(corrected); std::string cln; int ln = 1;
        while (std::getline(crs, cln)) {
            bool isFix = false;
            for (const auto& f : fixes) if (f.line == ln) { isFix = true; break; }
            std::cout << (isFix ? std::string(GREEN) : std::string(DIM))
                      << std::setw(4) << ln++ << RESET << " │ " << cln << "\n";
        }
        std::cout << "\n" << BOLD << "══ PASS 2: Compiling corrected source ══\n" << RESET;
    }

    auto r2 = compileSinglePass(corrPath, corrected, outDir,
                                stem + "_corrected", verbose, opts);

    if (r2.parseOk && r2.irOk && verbose)
        std::cout << "\n" << GREEN << BOLD
                  << "╔══════════════════════════════════════════════════════════╗\n"
                  << "║   AUTO-CORRECTION + COMPILATION: SUCCESS               ║\n"
                  << "╚══════════════════════════════════════════════════════════╝\n"
                  << RESET
                  << "  Original : " << srcPath  << "\n"
                  << "  Corrected: " << corrPath << "\n"
                  << "  IR       : " << r2.llPath << "\n\n";
    return r2;
}

// ═════════════════════════════════════════════════════════════
//  Batch test suite
// ═════════════════════════════════════════════════════════════
static void runTestSuite(const std::string& testDir,
                         const std::string& outDir,
                         const CompileOptions& opts)
{
    std::vector<std::string> files;
    for (auto& e : fs::directory_iterator(testDir))
        if (e.path().extension() == ".mc")
            files.push_back(e.path().string());
    std::sort(files.begin(), files.end());
    if (files.empty()) {
        std::cout << YELLOW << "No .mc files in: " << testDir << RESET << "\n";
        return;
    }
    fs::create_directories(outDir);

    const char* lvl = opts.optLevel == OptLevel::O0 ? "O0" :
                      opts.optLevel == OptLevel::O1 ? "O1" :
                      opts.optLevel == OptLevel::O2 ? "O2" : "O3";

    std::cout << "\n" << BOLD
              << "╔══════════════════════════════════════════════════════════╗\n"
              << "║       QUAIL COMPILER v4.0  —  BATCH TEST SUITE          ║\n"
              << "╚══════════════════════════════════════════════════════════╝\n"
              << RESET
              << "  Test dir : " << testDir  << "\n"
              << "  Out dir  : " << outDir   << "\n"
              << "  Tests    : " << files.size() << "\n"
              << "  Opt      : " << lvl << "\n"
              << "  AutoFix  : " << (opts.autoCorrect ? "yes" : "no") << "\n"
              << "  Graphs   : " << ((opts.genCFG||opts.genCFGAll||opts.genCallGraph||opts.genASTGraph) ? "yes" : "no") << "\n\n";

    const int NW = 36, SW = 8;
    std::cout << BOLD << std::left
              << std::setw(NW) << "File"
              << std::setw(SW) << "Parse"
              << std::setw(SW) << "IR"
              << std::setw(6)  << "Cmts"
              << std::setw(6)  << "Cls"
              << std::setw(6)  << "Dots"
              << std::setw(10) << "Link"
              << std::setw(SW) << "Exit"
              << "IR path\n" << RESET
              << std::string(NW + SW * 3 + 6 + 6 + 6 + 10 + 30, '-') << "\n";

    int passed = 0, failed = 0;
    for (auto& srcPath : files) {
        std::string name = fs::path(srcPath).filename().string();
        std::cout << std::left << std::setw(NW) << name << std::flush;
        CompileResult r = compileOne(srcPath, outDir, /*verbose=*/false, opts);
        std::cout << (r.parseOk ? std::string(GREEN)+"OK  "+RESET : std::string(RED)+"FAIL"+RESET) << "    ";
        std::cout << (r.irOk    ? std::string(GREEN)+"OK  "+RESET : std::string(RED)+"FAIL"+RESET) << "    ";
        std::cout << DIM << std::setw(6) << r.commentCount
                        << std::setw(6) << r.classCount
                        << std::setw(6) << r.dotFiles.size() << RESET;
        if (opts.buildBin)
            std::cout << (r.linkOk ? std::string(GREEN)+"linked    "+RESET
                                   : std::string(RED)  +"FAIL      "+RESET);
        else
            std::cout << std::setw(10) << "skipped";
        if (r.exitCode >= 0) std::cout << YELLOW << std::setw(SW) << r.exitCode << RESET;
        else                 std::cout << std::setw(SW) << "n/a";
        if (r.irOk) std::cout << r.llPath;
        if (r.errorCount > 0)
            std::cout << "  " << RED << "(" << r.errorCount << " err)" << RESET;
        std::cout << "\n";
        if (r.parseOk && r.irOk) ++passed; else ++failed;
    }

    std::cout << std::string(NW + SW * 3 + 6 + 6 + 6 + 10 + 30, '-') << "\n"
              << BOLD << "Results: " << GREEN << passed << " passed" << RESET
              << "  /  " << (failed ? std::string(RED) : std::string(GREEN))
              << failed << " failed" << RESET
              << "  out of " << files.size() << "\n\n";
}

// ═════════════════════════════════════════════════════════════
//  main
// ═════════════════════════════════════════════════════════════
int main(int argc, char* argv[]) {
    CompileOptions opts;
    bool        testAll   = false;
    std::string testDir   = "test";
    std::string outDir    = "out";
    std::string inputFile;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        // ── Mode flags ────────────────────────────────────────
        if      (a == "--debug")          opts.debugMode     = true;
        else if (a == "--build")          opts.buildBin      = true;
        else if (a == "--test-all")       testAll            = true;
        else if (a == "--no-autocorrect") opts.autoCorrect   = false;
        else if (a == "--show-ir-diff")   opts.showIrDiff    = true;
        // ── Optimization ──────────────────────────────────────
        else if (a == "--O0")             opts.optLevel      = OptLevel::O0;
        else if (a == "--O1")             opts.optLevel      = OptLevel::O1;
        else if (a == "--O2")             opts.optLevel      = OptLevel::O2;
        else if (a == "--O3")             opts.optLevel      = OptLevel::O3;
        // ── Graph & analysis flags ────────────────────────────
        else if (a == "--cfg")            opts.genCFG        = true;
        else if (a == "--cfg-all")        opts.genCFGAll     = true;
        else if (a == "--call-graph")     opts.genCallGraph  = true;
        else if (a == "--ast-graph")      opts.genASTGraph   = true;
        else if (a == "--ast-stats")      opts.showASTStats  = true;
        else if (a == "--complexity")     opts.showComplexity= true;
        else if (a == "--dead-code")      opts.showDeadCode  = true;
        else if (a == "--render")         opts.renderGraphs  = true;
        else if (a == "--graph") {
            // Convenience: enable all graph + analysis outputs
            opts.genCFG         = true;
            opts.genCFGAll      = true;
            opts.genCallGraph   = true;
            opts.genASTGraph    = true;
            opts.showASTStats   = true;
            opts.showComplexity = true;
            opts.showDeadCode   = true;
        }
        // ── Paths ─────────────────────────────────────────────
        else if (a == "--testdir" && i+1 < argc) testDir = argv[++i];
        else if (a == "--out"     && i+1 < argc) outDir  = argv[++i];
        else if (a[0] != '-')             inputFile = a;
    }

    if (testAll) {
        runTestSuite(testDir, outDir, opts);
        return 0;
    }

    if (inputFile.empty()) {
        std::cout << BOLD << "Quail Compiler v4.0  (Analysis + OOP + I/O edition)\n\n" << RESET
                  << "Usage:\n"
                  << "  Single file : ./Quail_Compiler [OPTIONS] <file.mc>\n"
                  << "  All tests   : ./Quail_Compiler --test-all [OPTIONS]\n\n"
                  << "COMPILATION OPTIONS:\n"
                  << "  --debug           Token table + full AST + class registry\n"
                  << "  --build           Compile to native binary and run\n"
                  << "  --O0/O1/O2/O3     Optimization level (default: O2)\n"
                  << "  --show-ir-diff    Print IR before and after optimization\n"
                  << "  --no-autocorrect  Disable automatic syntax error correction\n\n"
                  << "ANALYSIS & GRAPH OPTIONS:\n"
                  << "  --cfg             Per-function CFG DOT file\n"
                  << "  --cfg-all         All functions in one clustered CFG DOT\n"
                  << "  --call-graph      Whole-program call graph DOT\n"
                  << "  --ast-graph       AST visualisation DOT file\n"
                  << "  --ast-stats       AST node count by category\n"
                  << "  --complexity      Cyclomatic complexity per function\n"
                  << "  --dead-code       Unreachable basic-block detection\n"
                  << "  --render          Auto-render DOT → PNG (needs graphviz)\n"
                  << "  --graph           Enable ALL of the above analysis outputs\n\n"
                  << "PATH OPTIONS:\n"
                  << "  --testdir <dir>   Test directory (default: test/)\n"
                  << "  --out <dir>       Output directory (default: out/)\n\n"
                  << "EXAMPLES:\n"
                  << "  # Full analysis with PNG output:\n"
                  << "  ./Quail_Compiler --graph --render test/30_oop_complex.mc\n\n"
                  << "  # CFG only, rendered:\n"
                  << "  ./Quail_Compiler --cfg --render --O2 test/11_recursion_fib.mc\n\n"
                  << "  # AST stats only:\n"
                  << "  ./Quail_Compiler --ast-stats test/15_bubble_sort.mc\n\n"
                  << "  # Complexity report:\n"
                  << "  ./Quail_Compiler --complexity test/15_bubble_sort.mc\n\n"
                  << "  # Build + run + all graphs:\n"
                  << "  ./Quail_Compiler --build --graph --render test/21_class_basic.mc\n\n"
                  << "GRAPHVIZ:\n"
                  << "  Install:  sudo apt install graphviz\n"
                  << "  Manual:   dot -Tpng out/file_cfg_main.dot -o cfg.png\n"
                  << "            dot -Tsvg out/file_callgraph.dot -o cg.svg\n";
        return 1;
    }

    fs::create_directories(outDir);
    fs::path sp(inputFile);
    const char* lvl = opts.optLevel == OptLevel::O0 ? "O0" :
                      opts.optLevel == OptLevel::O1 ? "O1" :
                      opts.optLevel == OptLevel::O2 ? "O2" : "O3";

    std::string displayName = sp.filename().string();
    if (displayName.size() > 25) displayName = displayName.substr(0, 23) + "..";

    bool anyGraph = opts.genCFG || opts.genCFGAll || opts.genCallGraph ||
                    opts.genASTGraph || opts.showComplexity || opts.showDeadCode ||
                    opts.showASTStats;

    std::cout << "\n" << BOLD
              << "╔══════════════════════════════════════════════════════╗\n"
              << "║  Quail Compiler v4.0  →  "
              << std::left << std::setw(27) << displayName << "║\n"
              << "║  Opt: " << lvl
              << "  │  AutoFix: " << (opts.autoCorrect ? "ON " : "OFF")
              << "  │  Graphs: " << (anyGraph ? "ON " : "OFF")
              << "  │  Render: " << (opts.renderGraphs ? "ON " : "OFF") << " ║\n"
              << "╚══════════════════════════════════════════════════════╝\n"
              << RESET << "\n";

    CompileResult r = compileOne(inputFile, outDir, /*verbose=*/true, opts);

    if (r.errorCount > 0 || !r.parseOk || !r.irOk) return 1;
    std::cout << "\n" << GREEN << BOLD << "Compilation successful.\n" << RESET;
    return 0;
}