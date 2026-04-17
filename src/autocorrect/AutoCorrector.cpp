#include "autocorrect/AutoCorrector.h"
#include <sstream>
#include <algorithm>
#include <iostream>
#include <cctype>

AutoCorrector::AutoCorrector(const std::string&               source,
                             const std::vector<LexError>&     le,
                             const std::vector<ParseError>&   pe,
                             const std::vector<CodeGenError>& ce)
    : lexErrs(le), parseErrs(pe), cgErrs(ce)
{
    std::istringstream ss(source);
    std::string line;
    while (std::getline(ss, line)) lines.push_back(line);
}

std::string AutoCorrector::correct() {
    fixLexErrors();
    fixParseErrors();
    fixCodeGenErrors();
    return joinLines();
}

// ── Best position to insert ';' (before inline comments) ─────
static size_t findSemiInsertPos(const std::string& line) {
    size_t slashSlash  = line.find("//");
    size_t slashStar   = line.find("/*");
    size_t commentStart = std::string::npos;
    if (slashSlash != std::string::npos && slashStar != std::string::npos)
        commentStart = std::min(slashSlash, slashStar);
    else if (slashSlash != std::string::npos) commentStart = slashSlash;
    else if (slashStar  != std::string::npos) commentStart = slashStar;

    if (commentStart != std::string::npos) {
        size_t pos = commentStart;
        while (pos > 0 && (line[pos-1] == ' ' || line[pos-1] == '\t')) --pos;
        return pos;
    }
    size_t end = line.find_last_not_of(" \t\r");
    return (end == std::string::npos) ? line.size() : end + 1;
}

// ── Levenshtein distance for typo detection ───────────────────
static int editDistance(const std::string& a, const std::string& b) {
    int m = (int)a.size(), n = (int)b.size();
    std::vector<std::vector<int>> dp(m+1, std::vector<int>(n+1));
    for (int i = 0; i <= m; i++) dp[i][0] = i;
    for (int j = 0; j <= n; j++) dp[0][j] = j;
    for (int i = 1; i <= m; i++)
        for (int j = 1; j <= n; j++)
            dp[i][j] = (a[i-1] == b[j-1])
                ? dp[i-1][j-1]
                : 1 + std::min({dp[i-1][j], dp[i][j-1], dp[i-1][j-1]});
    return dp[m][n];
}

// All keywords that a typo could be targeting
static const std::vector<std::string> KEYWORDS = {
    "int","float","void","return","if","else","while","for",
    "break","continue","class","this","public","private",
    "print","println","scan","string","const","new","delete",
    "extends","super","switch","case","default"
};

static std::string suggestKeyword(const std::string& tok) {
    // Only suggest for tokens that look like identifiers
    if (tok.empty() || !std::isalpha((unsigned char)tok[0])) return "";
    std::string best;
    int bestDist = 3; // only suggest if within 2 edits
    for (auto& kw : KEYWORDS) {
        int d = editDistance(tok, kw);
        if (d < bestDist) { bestDist = d; best = kw; }
    }
    return best;
}

// ══════════════════════════════════════════════════════════════
//  Lex-error fixes
// ══════════════════════════════════════════════════════════════
void AutoCorrector::fixLexErrors() {
    for (const auto& err : lexErrs) {
        int li = err.line - 1;
        if (li < 0 || li >= (int)lines.size()) continue;

        // Unknown character
        {
            static const std::string tag = "Unknown character '";
            size_t p = err.message.find(tag);
            if (p != std::string::npos) {
                char bad = err.message[p + tag.size()];
                removeCharFromLine(li, bad, "LEX",
                    std::string("Removed unknown character '") + bad + "'");
                continue;
            }
        }

        // Unterminated block comment
        if (err.message.find("Unterminated block comment") != std::string::npos) {
            if (tryMark((int)lines.size() - 1, "block_comment_close")) {
                std::string before = lines.back();
                lines.back() += " */";
                corrections.push_back({(int)lines.size(), "LEX",
                    "Closed unterminated block comment with '*/'",
                    before, lines.back()});
            }
        }

        // Unterminated string literal
        if (err.message.find("Unterminated string literal") != std::string::npos) {
            if (tryMark(li, "string_close")) {
                std::string before = lines[li];
                size_t insertPos = findSemiInsertPos(lines[li]);
                lines[li] = lines[li].substr(0, insertPos) + "\"" + lines[li].substr(insertPos);
                corrections.push_back({err.line, "LEX",
                    "Closed unterminated string literal",
                    before, lines[li]});
            }
        }
    }
}

// ══════════════════════════════════════════════════════════════
//  Parse-error fixes
// ══════════════════════════════════════════════════════════════
void AutoCorrector::fixParseErrors() {
    struct PendingInsert { int afterLine; std::string text; std::string desc; };
    std::vector<PendingInsert> inserts;

    for (const auto& err : parseErrs) {
        const std::string& msg = err.message;

        // ── Missing semicolon ──────────────────────────────────
        if (msg.find("Missing ';'") != std::string::npos) {
            int li = err.line - 2;
            if (li < 0) li = 0;
            if (li >= (int)lines.size()) li = (int)lines.size() - 1;
            if (tryMark(li, "semi")) {
                size_t insertPos   = findSemiInsertPos(lines[li]);
                std::string before = lines[li];
                lines[li] = lines[li].substr(0, insertPos) + ";"
                           + lines[li].substr(insertPos);
                corrections.push_back({err.line, "PARSE",
                    "Added missing ';'", before, lines[li]});
            }
            continue;
        }

        int li = std::max(0, std::min(err.line - 1, (int)lines.size() - 1));

        // ── Missing closing brace ──────────────────────────────
        if (msg.find("Missing '}'") != std::string::npos) {
            if (tryMark(li, "rbrace"))
                inserts.push_back({li, "}", "Added missing '}'"});
            continue;
        }

        // ── Expected '{' ───────────────────────────────────────
        if (msg.find("Expected '{'") != std::string::npos) {
            std::string tr = trimRight(lines[li]);
            if (!tr.empty() && tr.back() != '{') {
                if (tryMark(li, "lbrace")) {
                    std::string before = lines[li];
                    lines[li] = tr + " {";
                    corrections.push_back({err.line, "PARSE",
                        "Added missing '{'", before, lines[li]});
                }
            }
            continue;
        }

        // ── Missing ')' ────────────────────────────────────────
        if (msg.find("Missing ')'") != std::string::npos) {
            std::string tr = trimRight(lines[li]);
            if (!tr.empty() && tryMark(li, "rparen")) {
                std::string before = lines[li];
                lines[li] = (tr.back() == ';')
                    ? tr.substr(0, tr.size()-1) + ");"
                    : tr + ")";
                corrections.push_back({err.line, "PARSE",
                    "Added missing ')'", before, lines[li]});
            }
            continue;
        }

        // ── Missing ']' ────────────────────────────────────────
        if (msg.find("Missing ']'") != std::string::npos) {
            std::string tr = trimRight(lines[li]);
            if (!tr.empty() && tryMark(li, "rbracket")) {
                std::string before = lines[li];
                lines[li] = (tr.back() == ';')
                    ? tr.substr(0, tr.size()-1) + "];"
                    : tr + "]";
                corrections.push_back({err.line, "PARSE",
                    "Added missing ']'", before, lines[li]});
            }
            continue;
        }

        // ── Missing ':' after case label ──────────────────────
        if (msg.find("Expected ':' after case") != std::string::npos ||
            msg.find("Expected ':' after 'default'") != std::string::npos) {
            std::string tr = trimRight(lines[li]);
            if (!tr.empty() && tr.back() != ':' && tryMark(li, "colon")) {
                std::string before = lines[li];
                lines[li] = tr + ":";
                corrections.push_back({err.line, "PARSE",
                    "Added missing ':' after case/default label", before, lines[li]});
            }
            continue;
        }

        // ── Expected '(' after keyword ─────────────────────────
        if (msg.find("Expected '(' after") != std::string::npos) {
            for (const char* kw : {"if", "while", "for", "switch"}) {
                size_t kp = lines[li].find(kw);
                if (kp == std::string::npos) continue;
                size_t after = kp + std::strlen(kw);
                while (after < lines[li].size() && lines[li][after] == ' ') ++after;
                if (after >= lines[li].size() || lines[li][after] != '(') {
                    if (tryMark(li, std::string("lparen_kw_") + kw)) {
                        std::string before = lines[li];
                        lines[li].insert(kp + std::strlen(kw), " (");
                        corrections.push_back({err.line, "PARSE",
                            std::string("Added '(' after '") + kw + "'",
                            before, lines[li]});
                    }
                }
                break;
            }
            continue;
        }

        // ── const without initializer ─────────────────────────
        if (msg.find("const variable '") != std::string::npos
            && msg.find("must be initialized") != std::string::npos) {
            static const std::string tag = "const variable '";
            size_t p = msg.find(tag);
            if (p != std::string::npos) {
                size_t s = p + tag.size();
                size_t e = msg.find("'", s);
                if (e != std::string::npos) {
                    std::string varName = msg.substr(s, e - s);
                    std::string tr = trimRight(lines[li]);
                    if (!tr.empty() && tr.back() == ';'
                        && tryMark(li, "const_init_" + varName)) {
                        std::string before = lines[li];
                        lines[li] = tr.substr(0, tr.size()-1) + " = 0;";
                        corrections.push_back({err.line, "PARSE",
                            "Added default initializer '= 0' to const '" + varName + "'",
                            before, lines[li]});
                    }
                }
            }
            continue;
        }

        // ── Keyword typo suggestion ────────────────────────────
        if (msg.find("Unexpected token '") != std::string::npos ||
            msg.find("Unrecognised statement") != std::string::npos)
        {
            static const std::string tag1 = "Unexpected token '";
            static const std::string tag2 = "starting with '";
            size_t tp = msg.find(tag1);
            std::string tag = tag1;
            if (tp == std::string::npos) { tp = msg.find(tag2); tag = tag2; }
            if (tp != std::string::npos) {
                size_t s = tp + tag.size();
                size_t e = msg.find("'", s);
                if (e != std::string::npos) {
                    std::string bad = msg.substr(s, e - s);
                    // Try keyword suggestion first
                    std::string suggestion = suggestKeyword(bad);
                    if (!suggestion.empty() && tryMark(li, "kw_typo_" + bad)) {
                        size_t wp = lines[li].find(bad);
                        if (wp != std::string::npos) {
                            std::string before = lines[li];
                            lines[li].replace(wp, bad.size(), suggestion);
                            corrections.push_back({err.line, "PARSE",
                                "Corrected typo: '" + bad + "' → '" + suggestion + "'",
                                before, lines[li]});
                        }
                    } else if (bad.size() == 1
                               && !std::isalnum((unsigned char)bad[0])
                               && bad[0] != '_') {
                        removeCharFromLine(li, bad[0], "PARSE",
                            "Removed stray '" + bad + "'");
                    }
                }
            }
            continue;
        }

        // ── Missing '.' after 'this' or 'super' ───────────────
        if (msg.find("Expected '.' after 'this'") != std::string::npos ||
            msg.find("Expected '.' after 'super'") != std::string::npos) {
            // Nothing easy to fix here — just note it
            continue;
        }
    }

    // Apply brace insertions in reverse to keep indices valid
    std::sort(inserts.begin(), inserts.end(),
              [](const PendingInsert& a, const PendingInsert& b) {
                  return a.afterLine > b.afterLine;
              });
    for (auto& ins : inserts)
        insertLineAfter(ins.afterLine, ins.text, "PARSE", ins.desc);
}

// ══════════════════════════════════════════════════════════════
//  CodeGen-error fixes
// ══════════════════════════════════════════════════════════════
void AutoCorrector::fixCodeGenErrors() {
    std::set<std::string> toDecl;
    static const std::vector<std::string> cgTags = {
        "Use of undeclared variable '",
        "Assignment to undeclared variable '",
        "Use of undeclared array '",
        "Assignment to undeclared array '"
    };
    for (const auto& err : cgErrs) {
        // Const write attempt — suggest fix: remove const
        if (err.message.find("Assignment to const variable '") != std::string::npos) {
            static const std::string tag = "Assignment to const variable '";
            size_t p = err.message.find(tag);
            if (p != std::string::npos) {
                size_t s = p + tag.size();
                size_t e = err.message.find("'", s);
                if (e != std::string::npos) {
                    std::string varName = err.message.substr(s, e - s);
                    // Find the const declaration line and change const → int
                    for (int li = 0; li < (int)lines.size(); li++) {
                        if (lines[li].find("const") != std::string::npos
                            && lines[li].find(varName) != std::string::npos
                            && tryMark(li, "remove_const_" + varName)) {
                            std::string before = lines[li];
                            size_t cp = lines[li].find("const");
                            lines[li].replace(cp, 5, "int  "); // keep same spacing
                            corrections.push_back({li+1, "CGEN",
                                "Changed const to int for '" + varName + "' (assigned later)",
                                before, lines[li]});
                            break;
                        }
                    }
                }
            }
            continue;
        }

        for (const auto& tag : cgTags) {
            size_t p = err.message.find(tag);
            if (p != std::string::npos) {
                size_t s = p + tag.size();
                size_t e = err.message.find("'", s);
                if (e != std::string::npos)
                    toDecl.insert(err.message.substr(s, e - s));
                break;
            }
        }
    }

    if (toDecl.empty()) return;

    // Find the opening brace of main()
    int insertAfter = -1;
    for (int i = 0; i < (int)lines.size(); i++) {
        if (lines[i].find("main(") != std::string::npos ||
            lines[i].find("main (") != std::string::npos) {
            for (int j = i; j < std::min((int)lines.size(), i + 3); j++) {
                if (lines[j].find('{') != std::string::npos) {
                    insertAfter = j;
                    break;
                }
            }
            break;
        }
    }
    // Fallback: use the last '{' in the file
    if (insertAfter < 0) {
        for (int i = (int)lines.size() - 1; i >= 0; i--) {
            if (lines[i].find('{') != std::string::npos) { insertAfter = i; break; }
        }
    }
    if (insertAfter < 0) insertAfter = 0;

    std::vector<std::string> decls(toDecl.rbegin(), toDecl.rend());
    for (const auto& name : decls) {
        if (tryMark(insertAfter, "cgdecl_" + name)) {
            std::string decl = "    int " + name + ";  /* auto-declared */";
            insertLineAfter(insertAfter, decl, "CGEN",
                "Declared undeclared variable 'int " + name + "'");
        }
    }
}

// ══════════════════════════════════════════════════════════════
//  Helpers
// ══════════════════════════════════════════════════════════════
std::string AutoCorrector::joinLines() const {
    std::string out;
    for (size_t i = 0; i < lines.size(); ++i) {
        out += lines[i];
        if (i + 1 < lines.size()) out += "\n";
    }
    return out;
}

std::string AutoCorrector::trimRight(const std::string& s) const {
    size_t e = s.find_last_not_of(" \t\r");
    return (e == std::string::npos) ? "" : s.substr(0, e + 1);
}

bool AutoCorrector::endsWith(const std::string& s, char c) const {
    return !s.empty() && s.back() == c;
}

bool AutoCorrector::appendToLine(int li, const std::string& app,
                                  const std::string& kind,
                                  const std::string& desc)
{
    std::string before = lines[li];
    lines[li] = trimRight(lines[li]) + app;
    corrections.push_back({li + 1, kind, desc, before, lines[li]});
    return true;
}

void AutoCorrector::insertLineAfter(int li, const std::string& newLine,
                                     const std::string& kind,
                                     const std::string& desc)
{
    lines.insert(lines.begin() + li + 1, newLine);
    corrections.push_back({li + 1, kind, desc, "", newLine});
}

bool AutoCorrector::removeCharFromLine(int li, char ch,
                                        const std::string& kind,
                                        const std::string& desc)
{
    std::string& line = lines[li];
    size_t p = line.find(ch);
    if (p == std::string::npos) return false;
    std::string before = line;
    line.erase(p, 1);
    corrections.push_back({li + 1, kind, desc, before, line});
    return true;
}

bool AutoCorrector::tryMark(int li, const std::string& key) {
    auto k = std::make_pair(li, key);
    if (applied.count(k)) return false;
    applied.insert(k);
    return true;
}