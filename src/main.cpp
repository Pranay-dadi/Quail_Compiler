#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "utils/Logger.h"

std::string tokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::INT:      return "INT";
        case TokenType::FLOAT:    return "FLOAT";
        case TokenType::RETURN:   return "RETURN";
        case TokenType::IF:       return "IF";
        case TokenType::ELSE:     return "ELSE";
        case TokenType::WHILE:    return "WHILE";
        case TokenType::FOR:      return "FOR";
        case TokenType::IDENT:    return "IDENTIFIER";
        case TokenType::NUMBER:   return "NUMBER_LITERAL";
        case TokenType::PLUS:     return "PLUS";
        case TokenType::MINUS:    return "MINUS";
        case TokenType::ASSIGN:   return "ASSIGN";
        case TokenType::SEMI:     return "SEMICOLON";
        case TokenType::LPAREN:   return "LPAREN";
        case TokenType::RPAREN:   return "RPAREN";
        case TokenType::LBRACE:   return "LBRACE";
        case TokenType::RBRACE:   return "RBRACE";
        case TokenType::EOF_TOK:  return "EOF";
        default:                  return "OTHER/OPERATOR";
    }
}


int main(int argc, char* argv[]) {
    // 1. Requirement: Support for "Standard" vs "Debug" mode
    bool debugMode = (argc > 1 && std::string(argv[1]) == "--debug");

    // 2. Requirement: Load source from a file (e.g., test.quail)
    std::string filename = "src/test/sample.mc"; 
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "\033[1;31mError: Could not open " << filename << "\033[0m" << std::endl;
        // New Diagnostic: Print the current working directory
        std::cout << "Make sure the 'src' folder is visible from the directory above." << std::endl;
        return 1;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    // 3. Lexical Analysis
    Logger::log(Stage::LEXER, "Starting tokenization...");
    Lexer lexer(source);
    std::vector<Token> tokens = lexer.tokenize();

    // 4. Observability: Token Stream Extraction (Requirement 1 & 4)
    if (debugMode) {
        std::cout << "\n\033[1;34m[A. LEXICAL ANALYSIS: TOKEN STREAM]\033[0m" << std::endl;
        std::cout << "--------------------------------------------------" << std::endl;
        std::cout << "| " << std::left << std::setw(15) << "LEXEME" << "| " << std::left << std::setw(15) << "TYPE ID" << "| " << "CATEGORY" << std::endl;
        std::cout << "--------------------------------------------------" << std::endl;

        for (const auto& t : tokens) {
            std::string category = "OTHER";
    
            // Cast the enum to int once to make the logic clean
            int typeVal = static_cast<int>(t.type); 

            if (typeVal <= 6) 
                category = "\033[1;32mKEYWORD\033[0m";
            else if (typeVal == 7) 
                category = "\033[1;36mIDENTIFIER\033[0m";
            else if (typeVal == 8 || typeVal == 9) 
                category = "\033[1;33mLITERAL\033[0m";
            else if (typeVal >= 10 && typeVal <= 23) 
                category = "\033[1;31mOPERATOR\033[0m";

            // Output the row
            std::cout << "| " << std::left << std::setw(15) << t.lexeme << "| " << std::left << std::setw(15) << typeVal << "| " << category << std::endl;
        }
    std::cout << "--------------------------------------------------" << std::endl;
    }

    std::cout << "--- [LEXICAL ANALYZER: TOKEN STREAM] ---\n";
    for (const auto& t : tokens) {
        std::cout << "[TOKEN] Type: " << std::left << std::setw(15) 
                  << tokenTypeToString(t.type) 
                  << " | Lexeme: \"" << t.lexeme << "\"" << std::endl;
        if (t.type == TokenType::EOF_TOK) break;
    }
    std::cout << "-------------------------------------------\n\n";

    // 5. Parsing (Requirement 2: AST Visualization)
    Logger::log(Stage::PARSER, "Starting syntactic analysis...");
    Parser parser(tokens);
    auto ast = parser.parse();

    if (ast) {

        std::cout << "\033[1;32mFrontend Analysis Successful!\033[0m" << std::endl;
        ast->print(0);
        
        if (debugMode) {
            Logger::log(Stage::PARSER, "Generating AST Visualization (Graphviz)...");
            // This is where you will eventually call: ast->generateDOT("ast.dot");
        }
    } else {
        std::cerr << "\033[1;31mFrontend Analysis Failed.\033[0m" << std::endl;
        return 1;
    }

    return 0;
}