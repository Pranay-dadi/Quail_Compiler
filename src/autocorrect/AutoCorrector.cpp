#include "autocorrect/AutoCorrector.h"
#include <sstream>
#include <algorithm>
#include <iostream>

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

// ── Finds the best position to insert ';' (before inline comments) ──
static size_t findSemiInsertPos(const std::string& line) {
    size_t slashSlash = line.find("//");
    size_t slashStar  = line.find("/*");
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

        // Missing semicolon — fix the line BEFORE the reported line
        if (msg.find("Missing ';'") != std::string::npos) {
            int li = err.line - 2;
            if (li < 0) li = 0;
            if (li >= (int)lines.size()) li = (int)lines.size() - 1;
            if (tryMark(li, "semi")) {
                size_t insertPos   = findSemiInsertPos(lines[li]);
                std::string before = lines[li];
                lines[li] = lines[li].substr(0, insertPos) + ";" + lines[li].substr(insertPos);
                corrections.push_back({err.line, "PARSE", "Added missing ';'", before, lines[li]});
            }
            continue;
        }

        int li = err.line - 1;
        if (li < 0) li = 0;
        if (li >= (int)lines.size()) li = (int)lines.size() - 1;

        // Missing closing brace
        if (msg.find("Missing '}'") != std::string::npos) {
            if (tryMark(li, "rbrace"))
                inserts.push_back({li, "}", "Added missing '}'"});
            continue;
        }

        // Expected '{'
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

        // Missing ')'
        if (msg.find("Missing ')'") != std::string::npos) {
            std::string tr = trimRight(lines[li]);
            if (!tr.empty()) {
                if (tryMark(li, "rparen")) {
                    std::string before = lines[li];
                    lines[li] = (tr.back() == ';')
                                ? tr.substr(0, tr.size()-1) + ");"
                                : tr + ")";
                    corrections.push_back({err.line, "PARSE",
                        "Added missing ')'", before, lines[li]});
                }
            }
            continue;
        }

        // Missing ']'
        if (msg.find("Missing ']'") != std::string::npos) {
            std::string tr = trimRight(lines[li]);
            if (!tr.empty()) {
                if (tryMark(li, "rbracket")) {
                    std::string before = lines[li];
                    lines[li] = (tr.back() == ';')
                                ? tr.substr(0, tr.size()-1) + "];"
                                : tr + "]";
                    corrections.push_back({err.line, "PARSE",
                        "Added missing ']'", before, lines[li]});
                }
            }
            continue;
        }

        // Expected '(' after keyword
        if (msg.find("Expected '(' after") != std::string::npos) {
            for (const char* kw : {"if", "while", "for"}) {
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

        // Unexpected token / unrecognised statement
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
                    if (bad.size() == 1 && !std::isalnum((unsigned char)bad[0]) && bad[0] != '_')
                        removeCharFromLine(li, bad[0], "PARSE",
                            "Removed stray '" + bad + "'");
                }
            }
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

    // Find the opening brace of main() — not the first '{' in the file,
    // which would be a class body brace and cause declarations to be
    // injected as class fields rather than as local variables in main().
    int insertAfter = -1;
    for (int i = 0; i < (int)lines.size(); i++) {
        // Match "int main(" or "int main (" anywhere on the line
        if (lines[i].find("main(") != std::string::npos ||
            lines[i].find("main (") != std::string::npos) {
            // Found the main() signature line; now search forward for its '{'
            for (int j = i; j < std::min((int)lines.size(), i + 3); j++) {
                if (lines[j].find('{') != std::string::npos) {
                    insertAfter = j;
                    break;
                }
            }
            break;
        }
    }
    // Fallback: if no main() found (unusual), use the last '{' in the file
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