cmake_minimum_required(VERSION 3.18)
project(Quail_Compiler)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── LLVM ─────────────────────────────────────────────────────────────────────
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "LLVM include dirs: ${LLVM_INCLUDE_DIRS}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# ── Project headers ───────────────────────────────────────────────────────────
include_directories(${CMAKE_SOURCE_DIR}/include)

# ── Sources ───────────────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/lexer/Lexer.cpp
    src/parser/Parser.cpp
    src/semantic/SymbolTable.cpp
    src/utils/Logger.cpp
    src/codegen/CodeGen.cpp
     src/autocorrect/AutoCorrector.cpp
)

add_executable(Quail_Compiler ${SOURCES})

# ── Link LLVM libraries ────────────────────────────────────────────────────────
llvm_map_components_to_libnames(LLVM_LIBS
    core
    support
    irreader
    passes
    analysis
    transformutils
    scalaropts
    instcombine
    ipo
    vectorize
    objcarcopts
    target 
    x86codegen 
    x86asmparser
)
target_link_libraries(Quail_Compiler PRIVATE ${LLVM_LIBS})

# ── Post-build: copy test files to build dir so --test-all works from build/ ──
add_custom_command(TARGET Quail_Compiler POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_BINARY_DIR}/test
    COMMENT "Copying test .mc files to build/test/"
)

# ── Create output directory in build dir ──────────────────────────────────────
add_custom_command(TARGET Quail_Compiler POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/out
    COMMENT "Creating out/ directory"
)

message(STATUS "")
message(STATUS "Build complete. From the build/ directory:")
message(STATUS "  Single file: ./Quail_Compiler [--debug] [--build] <file.mc>")
message(STATUS "  All tests  : ./Quail_Compiler --test-all [--build]")
message(STATUS "")