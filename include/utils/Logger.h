#pragma once
#include <iostream>

enum class Stage {
    LEXER, PARSER, SEMANTIC, CODEGEN, OPT
};

class Logger {
public:
    static void log(Stage s, const std::string& msg) {
        std::cout << "[" << name(s) << "] " << msg << std::endl;
    }
private:
    static std::string name(Stage s) {
        switch (s) {
            case Stage::LEXER: return "LEXER";
            case Stage::PARSER: return "PARSER";
            case Stage::SEMANTIC: return "SEMANTIC";
            case Stage::CODEGEN: return "CODEGEN";
            case Stage::OPT: return "OPT";
        }
        return "";
    }
};
