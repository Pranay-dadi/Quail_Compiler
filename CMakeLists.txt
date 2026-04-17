cmake_minimum_required(VERSION 3.18)
project(Quail_Compiler)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── LLVM ──────────────────────────────────────────────────────
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "LLVM include dirs: ${LLVM_INCLUDE_DIRS}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# ── Project headers ───────────────────────────────────────────
include_directories(${CMAKE_SOURCE_DIR}/include)

# ── Sources ───────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/lexer/Lexer.cpp
    src/parser/Parser.cpp
    src/semantic/SymbolTable.cpp
    src/semantic/TypeChecker.cpp
    src/utils/Logger.cpp
    src/codegen/CodeGen.cpp
    src/autocorrect/AutoCorrector.cpp
    src/analysis/CFGAnalyzer.cpp
    src/analysis/ASTGrapher.cpp
)

add_executable(Quail_Compiler ${SOURCES})

# ── LLVM target libraries ─────────────────────────────────────
# Core targets (always included)
set(QUAIL_LLVM_COMPONENTS
    core support irreader passes analysis
    transformutils scalaropts instcombine ipo vectorize
    objcarcopts target
    x86codegen x86asmparser)

# Check if AArch64 target is available in this LLVM build
execute_process(
    COMMAND ${LLVM_TOOLS_BINARY_DIR}/llvm-config --targets-built
    OUTPUT_VARIABLE LLVM_TARGETS_BUILT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)

if(LLVM_TARGETS_BUILT MATCHES "AArch64")
    message(STATUS "AArch64 target available — ARM cross-compilation enabled (--arm flag)")
    list(APPEND QUAIL_LLVM_COMPONENTS aarch64codegen aarch64asmparser)
    add_definitions(-DQUAIL_HAS_AARCH64)
else()
    message(STATUS "AArch64 NOT available in this LLVM build")
    message(STATUS "  To enable: rebuild LLVM with -DLLVM_TARGETS_TO_BUILD=\"X86;AArch64\"")
    message(STATUS "  The --arm flag will be accepted but llc will use default target")
endif()

llvm_map_components_to_libnames(LLVM_LIBS ${QUAIL_LLVM_COMPONENTS})
target_link_libraries(Quail_Compiler PRIVATE ${LLVM_LIBS})

# ── Post-build: copy test files ───────────────────────────────
add_custom_command(TARGET Quail_Compiler POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_BINARY_DIR}/test
    COMMENT "Copying test .mc files to build/test/")

add_custom_command(TARGET Quail_Compiler POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/out
    COMMENT "Creating out/ directory")

# ── Optional: graphviz check ──────────────────────────────────
find_program(GRAPHVIZ_DOT dot)
if(GRAPHVIZ_DOT)
    message(STATUS "Graphviz found: ${GRAPHVIZ_DOT}  (--render flag will work)")
else()
    message(STATUS "Graphviz NOT found — install: sudo apt install graphviz")
    message(STATUS "  DOT files still generated; render manually:")
    message(STATUS "  dot -Tpng out/file_cfg_main.dot -o cfg.png")
endif()

message(STATUS "")
message(STATUS "Build: cd build && cmake .. && make -j\$(nproc)")
message(STATUS "Usage: ./Quail_Compiler [OPTIONS] <file.mc>")
message(STATUS "  New features: --arm, switch/case, const, string, new/delete,")
message(STATUS "                extends/super, pointers (&/*), --ast-stats, --complexity")
message(STATUS "")