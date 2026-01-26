#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/CodeGen.h"
#include <fstream>

#if LLVM_ENABLE_ABI_BREAKING_CHECKS
bool llvm::DisableABIBreakingChecks = false;
#endif


int main(int argc, char** argv) {
    std::ifstream file(argv[1]);
    std::string src((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());

    Lexer lex(src);
    auto tokens = lex.tokenize();

    Parser parser(tokens);
    auto ast = parser.parse();

    CodeGen cg;
    cg.generate(ast.get());
    //cg.optimize();
    cg.dumpToFile("out.ll");
    cg.dump();
    system("lli out.ll");

}
