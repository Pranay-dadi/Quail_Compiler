#pragma once
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/CodeGen.h"
#include <string>
#include <vector>
#include <set>

// ──────────────────────────────────────────────────────────────
//  AutoCorrector
//  Pass the raw source, plus all accumulated error lists.
//  Call correct() → returns the patched source string.
//  Call getCorrections() for a human-readable change log.
// ──────────────────────────────────────────────────────────────

struct Correction {
    int         line;         // 1-based line that was changed (0 = new line inserted)
    std::string kind;         // "LEX", "PARSE", "CGEN"
    std::string description;  // what was done
    std::string before;       // original line text  (empty for insertions)
    std::string after;        // patched line text
};

class AutoCorrector {
public:
    AutoCorrector(const std::string&              source,
                  const std::vector<LexError>&    lexErrors,
                  const std::vector<ParseError>&  parseErrors,
                  const std::vector<CodeGenError>& cgErrors);

    // Returns corrected source code
    std::string correct();

    const std::vector<Correction>& getCorrections() const { return corrections; }
    bool madeChanges() const { return !corrections.empty(); }

private:
    std::vector<std::string>  lines;
    std::vector<LexError>     lexErrs;
    std::vector<ParseError>   parseErrs;
    std::vector<CodeGenError> cgErrs;
    std::vector<Correction>   corrections;

    // ── per-phase fixers ──────────────────────────────────────
    void fixLexErrors();
    void fixParseErrors();
    void fixCodeGenErrors();

    // ── helpers ───────────────────────────────────────────────
    std::string joinLines()  const;
    std::string trimRight(const std::string& s) const;
    bool        endsWith(const std::string& s, char c) const;
    // Returns true and records a correction if the line was changed
    bool        appendToLine(int lineIdx,
                             const std::string& appendStr,
                             const std::string& kind,
                             const std::string& desc);
    void        insertLineAfter(int lineIdx,
                                const std::string& newLine,
                                const std::string& kind,
                                const std::string& desc);
    // Remove a single character from a line
    bool        removeCharFromLine(int lineIdx, char ch,
                                   const std::string& kind,
                                   const std::string& desc);

    // ── already-fixed guards (avoid double-patching same location) ──
    std::set<std::pair<int,std::string>> applied;   // {lineIdx, fix-key}
    bool tryMark(int lineIdx, const std::string& key);
};