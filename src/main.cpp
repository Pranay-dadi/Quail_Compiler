// ============================================================
//  Quail Compiler  v2.0
//  Features:
//    • Comment preservation through Lexer → AST → corrected output
//    • Multi-level optimization (--O0 / --O1 / --O2 / --O3)
//    • IR before/after diff when optimization runs
//    • Auto-correction of syntax errors (--no-autocorrect to disable)
//
//  Usage:
//    ./Quail_Compiler [--debug] [--build] [--O0|--O1|--O2|--O3]
//                    [--no-autocorrect] [--show-ir-diff] <file.mc>
//    ./Quail_Compiler --test-all [--build] [--O2] [--testdir d] [--out d]
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
#include <sys/wait.h>

#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/CodeGen.h"

namespace fs = std::filesystem;

// ── ANSI colours ──────────────────────────────────────────────
static const char* RED    = "\033[1;31m";
static const char* GREEN  = "\033[1;32m";
static const char* YELLOW = "\033[1;33m";
static const char* BLUE   = "\033[1;34m";
static const char* MAGENTA= "\033[1;35m";
static const char* CYAN   = "\033[1;36m";
static const char* BOLD   = "\033[1m";
static const char* DIM    = "\033[2m";
static const char* RESET  = "\033[0m";

// ── Token type → string ────────────────────────────────────────
static std::string tokStr(TokenType t) {
    switch (t) {
        case TokenType::INT:          return "INT";
        case TokenType::FLOAT:        return "FLOAT";
        case TokenType::RETURN:       return "RETURN";
        case TokenType::IF:           return "IF";
        case TokenType::ELSE:         return "ELSE";
        case TokenType::WHILE:        return "WHILE";
        case TokenType::FOR:          return "FOR";
        case TokenType::BREAK:        return "BREAK";
        case TokenType::CONTINUE:     return "CONTINUE";
        case TokenType::IDENT:        return "IDENT";
        case TokenType::NUMBER:       return "NUMBER";
        case TokenType::FLOAT_VAL:    return "FLOAT_VAL";
        case TokenType::PLUS:         return "PLUS";
        case TokenType::MINUS:        return "MINUS";
        case TokenType::MUL:          return "MUL";
        case TokenType::DIV:          return "DIV";
        case TokenType::ASSIGN:       return "ASSIGN";
        case TokenType::EQ:           return "EQ";
        case TokenType::NEQ:          return "NEQ";
        case TokenType::INC:          return "INC";
        case TokenType::LT:           return "LT";
        case TokenType::GT:           return "GT";
        case TokenType::LE:           return "LE";
        case TokenType::GE:           return "GE";
        case TokenType::AND:          return "AND";
        case TokenType::OR:           return "OR";
        case TokenType::NOT:          return "NOT";
        case TokenType::LPAREN:       return "LPAREN";
        case TokenType::RPAREN:       return "RPAREN";
        case TokenType::LBRACE:       return "LBRACE";
        case TokenType::RBRACE:       return "RBRACE";
        case TokenType::LBRACKET:     return "LBRACKET";
        case TokenType::RBRACKET:     return "RBRACKET";
        case TokenType::SEMI:         return "SEMI";
        case TokenType::COMMA:        return "COMMA";
        case TokenType::LINE_COMMENT: return "LINE_CMT";
        case TokenType::BLOCK_COMMENT:return "BLOCK_CMT";
        case TokenType::EOF_TOK:      return "EOF";
        default:                      return "?";
    }
}

// ── Token table (debug) ────────────────────────────────────────
static void printTokenTable(const std::vector<Token>& tokens) {
    const int W1=30, W2=12;
    std::string sep(W1+W2+22, '-');
    std::cout << sep << "\n";
    std::cout << "| " << std::left << std::setw(W1) << "LEXEME"
              << " | " << std::setw(W2) << "TYPE"
              << " | " << std::setw(4) << "LINE"
              << " | CATEGORY\n" << sep << "\n";
    for (const auto& tk : tokens) {
        int tv = static_cast<int>(tk.type);
        std::string cat;
        if      (tv <= static_cast<int>(TokenType::CONTINUE))      cat = std::string(GREEN)   + "KEYWORD"  + RESET;
        else if (tv == static_cast<int>(TokenType::IDENT))         cat = std::string(CYAN)    + "IDENT"    + RESET;
        else if (tv == static_cast<int>(TokenType::NUMBER) ||
                 tv == static_cast<int>(TokenType::FLOAT_VAL))     cat = std::string(YELLOW)  + "LITERAL"  + RESET;
        else if (tv == static_cast<int>(TokenType::LINE_COMMENT) ||
                 tv == static_cast<int>(TokenType::BLOCK_COMMENT)) cat = std::string(DIM)     + "COMMENT"  + RESET;
        else if (tv >= static_cast<int>(TokenType::PLUS))          cat = std::string(RED)     + "OP/PUNCT" + RESET;
        else cat = "OTHER";

        // Truncate comment bodies for display
        std::string lex = tk.lexeme;
        if (lex.size() > (size_t)(W1-2)) lex = lex.substr(0, W1-5) + "...";
        std::cout << "| " << std::left << std::setw(W1) << lex
                  << " | " << std::setw(W2) << tokStr(tk.type)
                  << " | " << std::setw(4)  << tk.line
                  << " | " << cat << "\n";
        if (tk.type == TokenType::EOF_TOK) break;
    }
    std::cout << sep << "\n";
}

// ── Comment summary ────────────────────────────────────────────
static void printCommentSummary(const std::vector<Token>& tokens) {
    int lineCount = 0, blockCount = 0;
    std::cout << "\n" << DIM << BOLD << "── Comments found in source ──\n" << RESET;
    for (const auto& tk : tokens) {
        if (tk.type == TokenType::LINE_COMMENT) {
            std::cout << DIM << "  line " << std::setw(4) << tk.line
                      << "  // " << tk.lexeme << RESET << "\n";
            lineCount++;
        } else if (tk.type == TokenType::BLOCK_COMMENT) {
            std::cout << DIM << "  line " << std::setw(4) << tk.line
                      << "  /* " << tk.lexeme.substr(0, 60)
                      << (tk.lexeme.size() > 60 ? "..." : "") << " */" << RESET << "\n";
            blockCount++;
        }
    }
    if (lineCount + blockCount == 0)
        std::cout << DIM << "  (none)\n" << RESET;
    else
        std::cout << DIM << "  Total: " << lineCount << " line comment(s), "
                  << blockCount << " block comment(s)\n" << RESET;
}

// ── Optimization report ────────────────────────────────────────
static void printOptReport(const OptStats& s, OptLevel level, const std::string& irBefore,
                           const std::string& irAfter, bool showDiff)
{
    const char* lvlName =
        level == OptLevel::O0 ? "O0 (none)" :
        level == OptLevel::O1 ? "O1 (basic)" :
        level == OptLevel::O2 ? "O2 (standard)" : "O3 (aggressive)";

    std::cout << "\n" << MAGENTA << BOLD
              << "╔══════════════════════════════════════════════════════════╗\n"
              << "║              OPTIMIZATION REPORT                        ║\n"
              << "╚══════════════════════════════════════════════════════════╝\n"
              << RESET;
    std::cout << "  Level: " << BOLD << lvlName << RESET << "\n\n";

    // Per-function table
    const int W=24, N=8;
    std::cout << BOLD
              << std::left << std::setw(W) << "Function"
              << std::setw(N) << "Instr↓"
              << std::setw(N) << "Before"
              << std::setw(N) << "After"
              << std::setw(N) << "Blks↓"
              << std::setw(N) << "Before"
              << std::setw(N) << "After"
              << "\n" << RESET;
    std::cout << std::string(W + N*6, '-') << "\n";

    for (const auto& fs : s.functions) {
        int instrDrop = (int)fs.instrBefore - (int)fs.instrAfter;
        int blkDrop   = (int)fs.blocksBefore - (int)fs.blocksAfter;
        auto col = [&](int d) -> const char* { return d > 0 ? GREEN : (d < 0 ? RED : RESET); };
        std::cout << std::left << std::setw(W) << fs.name
                  << col(instrDrop) << std::setw(N) << instrDrop << RESET
                  << std::setw(N) << fs.instrBefore
                  << std::setw(N) << fs.instrAfter
                  << col(blkDrop)  << std::setw(N) << blkDrop   << RESET
                  << std::setw(N) << fs.blocksBefore
                  << std::setw(N) << fs.blocksAfter  << "\n";
    }

    std::cout << std::string(W + N*6, '-') << "\n";
    int totalInstrDrop = (int)s.totalInstrBefore - (int)s.totalInstrAfter;
    int totalBlkDrop   = (int)s.totalBlocksBefore - (int)s.totalBlocksAfter;
    auto col = [&](int d) -> const char* { return d > 0 ? GREEN : (d < 0 ? RED : RESET); };
    std::cout << BOLD << std::left << std::setw(W) << "TOTAL"
              << col(totalInstrDrop) << std::setw(N) << totalInstrDrop << RESET << BOLD
              << std::setw(N) << s.totalInstrBefore
              << std::setw(N) << s.totalInstrAfter
              << col(totalBlkDrop)  << std::setw(N) << totalBlkDrop << RESET << BOLD
              << std::setw(N) << s.totalBlocksBefore
              << std::setw(N) << s.totalBlocksAfter  << "\n" << RESET;

    if (s.totalInstrBefore > 0) {
        int pct = s.instrReduction();
        std::cout << "\n  " << BOLD
                  << (pct > 0 ? GREEN : RESET)
                  << "Instruction reduction: " << pct << "%"
                  << RESET << "\n";
    }

    // IR diff
    if (showDiff && !irBefore.empty() && !irAfter.empty()) {
        std::cout << "\n" << BOLD << "── IR diff (before → after) ──\n" << RESET;

        // Split into lines and show side-by-side line counts
        auto split = [](const std::string& s) {
            std::vector<std::string> v;
            std::istringstream ss(s); std::string l;
            while (std::getline(ss, l)) v.push_back(l);
            return v;
        };
        auto bLines = split(irBefore);
        auto aLines = split(irAfter);

        std::cout << DIM
                  << "  Before: " << bLines.size() << " lines\n"
                  << "  After : " << aLines.size() << " lines\n"
                  << "  Reduction: "
                  << (int)bLines.size() - (int)aLines.size() << " lines removed\n"
                  << RESET;

        // Show first 40 lines of optimized IR
        std::cout << "\n" << YELLOW << BOLD << "  Optimized IR (first 40 lines):\n" << RESET;
        int shown = 0;
        for (const auto& l : aLines) {
            if (shown++ >= 40) { std::cout << DIM << "  ... (" << aLines.size()-40 << " more lines)\n" << RESET; break; }
            std::cout << DIM << std::setw(4) << shown << RESET << "  " << l << "\n";
        }
    }
}

// ── Error report ──────────────────────────────────────────────
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
              << RESET;
    std::cerr << "  File: " << filename << "\n\n";
    int count = 0;
    for (auto& e : lexErrs)   { std::cerr << RED << "[LEX]  " << RESET << filename << ":" << e.line << "  " << e.message << "\n"; count++; }
    for (auto& e : parseErrs) { std::cerr << RED << "[PARSE]" << RESET << " " << filename << ":" << e.line << "  " << e.message << "\n"; count++; }
    for (auto& e : cgErrs)    { std::cerr << RED << "[CGEN] " << RESET << e.message << "\n"; count++; }
    std::cerr << "\n" << RED << BOLD << count << " error" << (count==1?"":"s") << " found." << RESET << " Compilation failed.\n\n";
    return true;
}

// ═══════════════════════════════════════════════════════════════
//  Core compile (single pass on given source text)
// ═══════════════════════════════════════════════════════════════
struct CompileResult {
    bool        parseOk  = false;
    bool        irOk     = false;
    bool        linkOk   = false;
    int         exitCode = -1;
    int         errorCount = 0;
    std::string llPath;
    std::string binPath;
    int         commentCount = 0;
};

static CompileResult compileSinglePass(const std::string& displayPath,
                                       const std::string& source,
                                       const std::string& outDir,
                                       const std::string& stem,
                                       bool  debugMode,
                                       bool  buildBinaries,
                                       bool  verbose,
                                       OptLevel optLevel,
                                       bool showIrDiff)
{
    CompileResult res;
    res.llPath  = outDir + "/" + stem + ".ll";
    res.binPath = outDir + "/" + stem;
    std::string objPath = outDir + "/" + stem + ".o";

    // ── LEXER (keepComments=true) ─────────────────────────────
    Lexer lexer(source, true);
    auto tokens   = lexer.tokenize();
    auto lexErrors = lexer.getErrors();

    // Count comments
    for (const auto& t : tokens)
        if (t.type == TokenType::LINE_COMMENT || t.type == TokenType::BLOCK_COMMENT)
            res.commentCount++;

    if (verbose && debugMode) {
        std::cout << "\n" << BLUE << BOLD << "[LEXICAL ANALYSIS]" << RESET << "\n";
        printTokenTable(tokens);
        printCommentSummary(tokens);
        std::cout << "\n";
    } else if (verbose) {
        // Condensed token stream
        std::cout << "--- [TOKEN STREAM] ---\n";
        for (const auto& tk : tokens) {
            if (tk.type == TokenType::LINE_COMMENT) {
                std::cout << DIM << "[COMMENT]    line:" << std::setw(4) << tk.line
                          << "  // " << tk.lexeme << RESET << "\n";
            } else if (tk.type == TokenType::BLOCK_COMMENT) {
                std::cout << DIM << "[COMMENT]    line:" << std::setw(4) << tk.line
                          << "  /* " << tk.lexeme.substr(0,40) << (tk.lexeme.size()>40?"...":"") << " */"
                          << RESET << "\n";
            } else {
                std::cout << "[TOKEN] " << std::left << std::setw(12) << tokStr(tk.type)
                          << "  line:" << std::setw(4) << tk.line
                          << "  \"" << tk.lexeme << "\"\n";
            }
            if (tk.type == TokenType::EOF_TOK) break;
        }
        std::cout << "  " << DIM << "(" << res.commentCount << " comment token(s) preserved)" << RESET << "\n";
        std::cout << "----------------------\n\n";
    }

    // ── PARSER ────────────────────────────────────────────────
    Parser parser(tokens);
    auto ast = parser.parse();
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
        if (debugMode) {
            std::cout << BLUE << BOLD << "[AST — comments preserved as nodes]\n" << RESET;
            ast->print(0);
        } else {
            std::cout << GREEN << "Frontend OK\n" << RESET;
            std::cout << "--- [AST] ---\n";
            ast->print(0);
            std::cout << "-------------\n\n";
        }
    }

    // ── CODEGEN ───────────────────────────────────────────────
    CodeGen cg;
    cg.generate(ast.get());
    auto cgErrors = cg.getErrors();

    if (!cgErrors.empty()) {
        res.errorCount = (int)cgErrors.size();
        if (verbose) reportErrors(displayPath, {}, {}, cgErrors);
        return res;
    }

    // ── OPTIMIZATION ──────────────────────────────────────────
    std::string irBefore;
    if (optLevel != OptLevel::O0 && verbose)
        irBefore = cg.getIRString();

    if (optLevel != OptLevel::O0) {
        if (verbose) {
            const char* lvl = optLevel==OptLevel::O1?"O1":optLevel==OptLevel::O2?"O2":"O3";
            std::cout << "\n" << MAGENTA << BOLD
                      << "── Running optimizer (" << lvl << ") ──\n" << RESET;
        }
        cg.optimize(optLevel);

        if (verbose) {
            std::string irAfter = cg.getIRString();
            printOptReport(cg.getOptStats(), optLevel, irBefore, irAfter, showIrDiff);
        }
    } else if (verbose) {
        std::cout << DIM << "\n  (Optimization disabled: --O0)\n" << RESET;
    }

    // ── EMIT IR ───────────────────────────────────────────────
    cg.dumpToFile(res.llPath);
    res.irOk = fs::exists(res.llPath) && fs::file_size(res.llPath) > 0;

    if (verbose && res.irOk) {
        std::cout << "\n--- [LLVM IR" << (optLevel!=OptLevel::O0?" (optimized)":"") << "] ---\n";
        cg.dump();
        std::cout << YELLOW << "\n→ IR written to: " << res.llPath << RESET << "\n";
    }

    // ── BUILD (optional) ──────────────────────────────────────
    if (buildBinaries && res.irOk) {
        std::string llcCmd   = "llc "   + res.llPath + " -filetype=obj -o " + objPath + " 2>/dev/null";
        std::string clangCmd = "clang " + objPath    + " -o " + res.binPath            + " 2>/dev/null";
        if (verbose) { std::cout << "\n" << BOLD << "Building...\n" << RESET << "  $ " << llcCmd << "\n"; }
        bool llcOk = (std::system(llcCmd.c_str()) == 0);
        if (verbose) std::cout << "  $ " << clangCmd << "\n";
        bool clangOk = llcOk && (std::system(clangCmd.c_str()) == 0);
        res.linkOk = clangOk;
        if (res.linkOk) {
            if (verbose) std::cout << GREEN << "→ Executable: " << res.binPath << RESET << "\n\n";
            int raw = std::system(res.binPath.c_str());
            res.exitCode = WEXITSTATUS(raw);
            if (verbose) std::cout << YELLOW << "Exit code: " << res.exitCode << RESET << "\n";
        } else if (verbose) {
            std::cerr << RED << "[BUILD] llc/clang failed.\n" << RESET;
        }
    } else if (!buildBinaries && verbose) {
        std::cout << "\n" << BOLD << "Next steps:\n" << RESET
                  << "  llc "   << res.llPath << " -filetype=obj -o " << objPath << "\n"
                  << "  clang " << objPath    << " -o " << res.binPath << "\n"
                  << "  " << res.binPath << " ; echo $?\n";
    }

    return res;
}

// ═══════════════════════════════════════════════════════════════
//  compileOne  — with optional auto-correction
// ═══════════════════════════════════════════════════════════════
static CompileResult compileOne(const std::string& srcPath,
                                const std::string& outDir,
                                bool debugMode,
                                bool buildBinaries,
                                bool verbose,
                                OptLevel optLevel,
                                bool autoCorrect,
                                bool showIrDiff)
{
    fs::path p(srcPath);
    std::string stem = p.stem().string();

    // Read source
    std::ifstream file(srcPath);
    if (!file.is_open()) {
        if (verbose) std::cerr << RED << "Cannot open: " << srcPath << RESET << "\n";
        return {};
    }
    std::stringstream buf; buf << file.rdbuf();
    std::string source = buf.str();

    // Quick pre-check: do we have errors at all?
    {
        Lexer lx(source, false);       // strip-comments mode for error detection
        auto toks = lx.tokenize();
        Parser px(toks);
        auto ast = px.parse();
        bool hasErrors = lx.hasErrors() || px.hasErrors();
        if (!hasErrors && ast) {
            CodeGen cg0; cg0.generate(ast.get());
            hasErrors = cg0.hasErrors();
        }

        if (!hasErrors || !autoCorrect) {
            // Compile normally (keeping comments this time)
            return compileSinglePass(srcPath, source, outDir, stem,
                                     debugMode, buildBinaries, verbose,
                                     optLevel, showIrDiff);
        }
    }

    // ── AUTO-CORRECTION NEEDED ────────────────────────────────
    if (verbose)
        std::cout << BOLD << "\n══ PASS 1: Identifying errors ══\n" << RESET;

    // Collect errors (comment-stripped for parser accuracy)
    Lexer lx1(source, false);
    auto toks1    = lx1.tokenize();
    auto lexErrs1 = lx1.getErrors();
    Parser px1(toks1);
    auto ast1       = px1.parse();
    auto parseErrs1 = px1.getErrors();
    std::vector<CodeGenError> cgErrs1;
    if (lexErrs1.empty() && parseErrs1.empty() && ast1) {
        CodeGen cg1; cg1.generate(ast1.get());
        cgErrs1 = cg1.getErrors();
    }
    if (verbose) reportErrors(srcPath, lexErrs1, parseErrs1, cgErrs1);

    // ── Inline auto-corrector (re-implemented here without separate file) ─
    if (verbose)
        std::cout << "\n" << YELLOW << BOLD
                  << "══ AUTO-CORRECTION PHASE ══\n" << RESET
                  << "Fixing " << (lexErrs1.size()+parseErrs1.size()+cgErrs1.size())
                  << " error(s)...\n";

    // Split source into lines
    std::vector<std::string> lines;
    { std::istringstream ss(source); std::string ln; while (std::getline(ss, ln)) lines.push_back(ln); }

    struct Fix { int line; std::string kind, desc, before, after; };
    std::vector<Fix> fixes;
    auto trimR = [](const std::string& s) {
        size_t e = s.find_last_not_of(" \t\r"); return e==std::string::npos?"":s.substr(0,e+1); };

    // Apply parse error fixes
    for (const auto& err : parseErrs1) {
        int li = std::max(0, err.line - 1);
        if (li >= (int)lines.size()) li = (int)lines.size()-1;
        const std::string& m = err.message;

        if (m.find("Missing ';'") != std::string::npos) {
            std::string tr = trimR(lines[li]);
            if (!tr.empty() && tr.back()!=';' && tr.back()!='{' && tr.back()!='}') {
                std::string bef = lines[li];
                // Insert ; before any trailing inline comment
                size_t cmt = tr.find("//");
                if (cmt != std::string::npos)
                    lines[li] = trimR(tr.substr(0, cmt)) + "; " + tr.substr(cmt); // fallback below
                else if (tr.find("/*") != std::string::npos) {
                    size_t cp = tr.find("/*");
                    lines[li] = tr.substr(0,cp-1)+";"+" "+tr.substr(cp);
                } else
                    lines[li] = tr + ";";
                fixes.push_back({err.line,"PARSE","Added ';'",bef,lines[li]});
            }
        }
        else if (m.find("Missing '}'") != std::string::npos) {
            lines.insert(lines.begin()+li+1, "}");
            fixes.push_back({err.line,"PARSE","Inserted missing '}'","","}"});
        }
        else if (m.find("Missing ')'") != std::string::npos) {
            std::string tr = trimR(lines[li]);
            if (!tr.empty()) {
                std::string bef = lines[li];
                lines[li] = tr.back()==';' ? tr.substr(0,tr.size()-1)+");" : tr+")";
                fixes.push_back({err.line,"PARSE","Added ')'",bef,lines[li]});
            }
        }
        else if (m.find("Expected '{'") != std::string::npos) {
            std::string tr = trimR(lines[li]);
            if (!tr.empty() && tr.back()!='{') {
                std::string bef = lines[li];
                lines[li] = tr + " {";
                fixes.push_back({err.line,"PARSE","Added '{'",bef,lines[li]});
            }
        }
    }

    // Apply lex error fixes
    for (const auto& err : lexErrs1) {
        int li = std::max(0, err.line-1);
        static const std::string tag = "Unknown character '";
        size_t p = err.message.find(tag);
        if (p != std::string::npos) {
            char bad = err.message[p+tag.size()];
            std::string& ln = lines[li];
            size_t cp = ln.find(bad);
            if (cp != std::string::npos) {
                std::string bef = ln; ln.erase(cp,1);
                fixes.push_back({err.line,"LEX","Removed '"+std::string(1,bad)+"'",bef,ln});
            }
        }
        if (err.message.find("Unterminated block comment") != std::string::npos) {
            std::string bef = lines.back(); lines.back() += " */";
            fixes.push_back({(int)lines.size(),"LEX","Closed block comment",bef,lines.back()});
        }
    }

    // Codegen fixes: declare undeclared variables
    for (const auto& err : cgErrs1) {
        static const std::vector<std::string> cgTags = {
            "Use of undeclared variable '","Assignment to undeclared variable '"};
        for (const auto& tag : cgTags) {
            size_t p = err.message.find(tag);
            if (p != std::string::npos) {
                size_t s = p+tag.size(), e = err.message.find("'",s);
                if (e != std::string::npos) {
                    std::string name = err.message.substr(s, e-s);
                    // Find first opening brace
                    for (int i=0;i<(int)lines.size();i++) {
                        if (lines[i].find('{') != std::string::npos) {
                            std::string decl = "    int " + name + ";  /* auto-declared */";
                            lines.insert(lines.begin()+i+1, decl);
                            fixes.push_back({i+1,"CGEN","Declared 'int "+name+"'","",decl});
                            break;
                        }
                    }
                }
                break;
            }
        }
    }

    // Rebuild corrected source
    std::string corrected;
    for (size_t i=0;i<lines.size();i++) { corrected += lines[i]; if (i+1<lines.size()) corrected+="\n"; }

    // Print correction report
    if (verbose && !fixes.empty()) {
        std::cout << "\n" << YELLOW << BOLD
                  << "╔══════════════════════════════════════════════════════════╗\n"
                  << "║              AUTO-CORRECTION REPORT                     ║\n"
                  << "╚══════════════════════════════════════════════════════════╝\n"
                  << RESET
                  << "  " << fixes.size() << " fix(es) applied.\n\n";
        for (size_t i=0;i<fixes.size();i++) {
            const auto& f = fixes[i];
            std::cout << CYAN << "  [" << (i+1) << "] [" << f.kind << "] line " << f.line
                      << " — " << f.desc << RESET << "\n";
            if (!f.before.empty()) std::cout << "       " << RED   << "- " << f.before << RESET << "\n";
            if (!f.after.empty())  std::cout << "       " << GREEN << "+ " << f.after  << RESET << "\n";
        }
    }

    // Write corrected file
    std::string corrPath = outDir + "/" + stem + "_corrected.mc";
    { std::ofstream o(corrPath); if (o) o << corrected; }
    if (verbose) {
        std::cout << "\n" << YELLOW << "  Corrected file: " << corrPath << RESET << "\n";

        // Show corrected source with line numbers
        std::cout << "\n" << BLUE << BOLD
                  << "══ CORRECTED SOURCE ══\n" << RESET;
        std::istringstream crs(corrected); std::string cln; int ln=1;
        while (std::getline(crs, cln)) {
            bool isFix = false;
            for (auto& f : fixes) if (f.line==ln) { isFix=true; break; }
            std::cout << (isFix ? std::string(GREEN) : std::string(DIM))
                      << std::setw(4) << ln++ << RESET << " │ " << cln << "\n";
        }
    }

    // ── PASS 2: compile corrected source ──────────────────────
    if (verbose)
        std::cout << "\n" << BOLD << "══ PASS 2: Compiling corrected source ══\n" << RESET;

    auto r2 = compileSinglePass(corrPath, corrected, outDir, stem + "_corrected",
                                debugMode, buildBinaries, verbose,
                                optLevel, showIrDiff);

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

// ── Batch test suite ───────────────────────────────────────────
static void runTestSuite(const std::string& testDir,
                         const std::string& outDir,
                         bool buildBinaries,
                         bool autoCorrect,
                         OptLevel optLevel)
{
    std::vector<std::string> files;
    for (auto& e : fs::directory_iterator(testDir))
        if (e.path().extension() == ".mc")
            files.push_back(e.path().string());
    std::sort(files.begin(), files.end());
    if (files.empty()) { std::cout << YELLOW << "No .mc files in: " << testDir << RESET << "\n"; return; }
    fs::create_directories(outDir);

    const char* lvl = optLevel==OptLevel::O0?"O0":optLevel==OptLevel::O1?"O1":optLevel==OptLevel::O2?"O2":"O3";
    std::cout << "\n" << BOLD
              << "╔══════════════════════════════════════════════════════════╗\n"
              << "║       QUAIL COMPILER  —  BATCH TEST SUITE               ║\n"
              << "╚══════════════════════════════════════════════════════════╝\n" << RESET
              << "  Test dir: " << testDir << "\n  Out dir : " << outDir
              << "\n  Tests   : " << files.size()
              << "\n  Opt     : " << lvl
              << "\n  AutoFix : " << (autoCorrect?"yes":"no") << "\n\n";

    const int NW=36, SW=8;
    std::cout << BOLD << std::left
              << std::setw(NW)    << "File"
              << std::setw(SW)    << "Parse"
              << std::setw(SW)    << "IR"
              << std::setw(6)     << "Cmts"
              << std::setw(10)    << "Link"
              << std::setw(SW)    << "Exit"
              << "IR path\n" << RESET
              << std::string(NW+SW*3+6+10+30,'-') << "\n";

    int passed=0, failed=0;
    for (auto& srcPath : files) {
        std::string name = fs::path(srcPath).filename().string();
        std::cout << std::left << std::setw(NW) << name << std::flush;
        CompileResult r = compileOne(srcPath, outDir, false, buildBinaries, false,
                                     optLevel, autoCorrect, false);
        std::cout << (r.parseOk?std::string(GREEN)+"OK  "+RESET:std::string(RED)+"FAIL"+RESET) << "    ";
        std::cout << (r.irOk   ?std::string(GREEN)+"OK  "+RESET:std::string(RED)+"FAIL"+RESET) << "    ";
        std::cout << DIM << std::setw(6) << r.commentCount << RESET;
        if (buildBinaries) std::cout << (r.linkOk?std::string(GREEN)+"linked    "+RESET:std::string(RED)+"FAIL      "+RESET);
        else               std::cout << std::setw(10) << "skipped";
        if (r.exitCode>=0) std::cout << YELLOW << std::setw(SW) << r.exitCode << RESET;
        else               std::cout << std::setw(SW) << "n/a";
        if (r.irOk) std::cout << r.llPath;
        if (r.errorCount>0) std::cout << "  " << RED << "(" << r.errorCount << " err)" << RESET;
        std::cout << "\n";
        if (r.parseOk && r.irOk) passed++; else failed++;
    }
    std::cout << std::string(NW+SW*3+6+10+30,'-') << "\n"
              << BOLD << "Results: " << GREEN << passed << " passed" << RESET << "  /  "
              << (failed?std::string(RED):std::string(GREEN)) << failed << " failed"
              << RESET << "  out of " << files.size() << "\n\n";
}

// ── main ───────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    bool        debugMode   = false;
    bool        buildBin    = false;
    bool        testAll     = false;
    bool        autoCorrect = true;
    bool        showIrDiff  = false;
    OptLevel    optLevel    = OptLevel::O2;   // default: O2
    std::string testDir     = "test";
    std::string outDir      = "out";
    std::string inputFile;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if      (a == "--debug")          debugMode   = true;
        else if (a == "--build")          buildBin    = true;
        else if (a == "--test-all")       testAll     = true;
        else if (a == "--no-autocorrect") autoCorrect = false;
        else if (a == "--show-ir-diff")   showIrDiff  = true;
        else if (a == "--O0")             optLevel    = OptLevel::O0;
        else if (a == "--O1")             optLevel    = OptLevel::O1;
        else if (a == "--O2")             optLevel    = OptLevel::O2;
        else if (a == "--O3")             optLevel    = OptLevel::O3;
        else if (a == "--testdir" && i+1<argc) testDir = argv[++i];
        else if (a == "--out"     && i+1<argc) outDir  = argv[++i];
        else if (a[0] != '-')             inputFile   = a;
    }

    if (testAll) {
        runTestSuite(testDir, outDir, buildBin, autoCorrect, optLevel);
        return 0;
    }

    if (inputFile.empty()) {
        std::cout << BOLD << "Quail Compiler v2.0\n\n" << RESET
                  << "Usage:\n"
                  << "  Single file : ./Quail_Compiler [OPTIONS] <file.mc>\n"
                  << "  All tests   : ./Quail_Compiler --test-all [OPTIONS]\n\n"
                  << "OPTIONS:\n"
                  << "  --debug           Show token table + full AST\n"
                  << "  --build           Compile to native binary and run\n"
                  << "  --O0              No optimization\n"
                  << "  --O1              Basic optimizations (mem2reg, instcombine, GVN)\n"
                  << "  --O2              Standard optimizations (default)\n"
                  << "  --O3              Aggressive optimizations\n"
                  << "  --show-ir-diff    Show IR before/after optimization diff\n"
                  << "  --no-autocorrect  Disable automatic error correction\n"
                  << "  --testdir <dir>   Test directory (default: test/)\n"
                  << "  --out <dir>       Output directory (default: out/)\n";
        return 1;
    }

    fs::create_directories(outDir);
    fs::path sp(inputFile);
    const char* lvl = optLevel==OptLevel::O0?"O0":optLevel==OptLevel::O1?"O1":optLevel==OptLevel::O2?"O2":"O3";

    std::cout << "\n" << BOLD
              << "╔══════════════════════════════════════════════════════╗\n"
              << "║  Quail Compiler v2.0  →  "
              << std::left << std::setw(27) << sp.filename().string() << "║\n"
              << "║  Optimization: " << lvl
              << "  │  Comments: ON  │  AutoFix: "
              << (autoCorrect?"ON ":"OFF") << "    ║\n"
              << "╚══════════════════════════════════════════════════════╝\n"
              << RESET << "\n";

    CompileResult r = compileOne(inputFile, outDir, debugMode, buildBin, true,
                                 optLevel, autoCorrect, showIrDiff);

    if (r.errorCount > 0 || !r.parseOk || !r.irOk) return 1;
    std::cout << "\n" << GREEN << BOLD << "Compilation successful.\n" << RESET;
    return 0;
}