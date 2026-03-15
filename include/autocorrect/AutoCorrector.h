#pragma once
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/CodeGen.h"
#include <string>
#include <vector>
#include <set>

struct Correction {
    int         line;
    std::string kind;
    std::string description;
    std::string before;
    std::string after;
};

class AutoCorrector {
public:
    AutoCorrector(const std::string&               source,
                  const std::vector<LexError>&     lexErrors,
                  const std::vector<ParseError>&   parseErrors,
                  const std::vector<CodeGenError>& cgErrors);

    std::string correct();

    const std::vector<Correction>& getCorrections() const { return corrections; }
    bool madeChanges() const { return !corrections.empty(); }

private:
    std::vector<std::string>  lines;
    std::vector<LexError>     lexErrs;
    std::vector<ParseError>   parseErrs;
    std::vector<CodeGenError> cgErrs;
    std::vector<Correction>   corrections;

    void fixLexErrors();
    void fixParseErrors();
    void fixCodeGenErrors();

    std::string joinLines()  const;
    std::string trimRight(const std::string& s) const;
    bool        endsWith(const std::string& s, char c) const;
    bool        appendToLine(int lineIdx, const std::string& appendStr,
                             const std::string& kind, const std::string& desc);
    void        insertLineAfter(int lineIdx, const std::string& newLine,
                                const std::string& kind, const std::string& desc);
    bool        removeCharFromLine(int lineIdx, char ch,
                                   const std::string& kind, const std::string& desc);

    std::set<std::pair<int,std::string>> applied;
    bool tryMark(int lineIdx, const std::string& key);
};
