cmake_minimum_required(VERSION 3.16)
project(Quail_Compiler LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(
    ${LLVM_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
)

add_definitions(${LLVM_DEFINITIONS})

file(GLOB_RECURSE SOURCES
    src/*.cpp
)

add_executable(Quail_Compiler ${SOURCES})

llvm_map_components_to_libnames(LLVM_LIBS
    core
    support
    irreader
    analysis
    passes
)

target_link_libraries(Quail_Compiler ${LLVM_LIBS})
