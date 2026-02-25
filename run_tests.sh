#!/bin/sh
# Run from the project root: sh run_tests.sh
# Examples:
#   sh run_tests.sh                        # IR only
#   sh run_tests.sh --build                # compile + link + run
#   sh run_tests.sh test/02_if_else.mc     # single file
#   sh run_tests.sh --debug test/02_if_else.mc

# ── Locate project root (where this script lives) ────────────
SCRIPT_DIR="$(pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
COMPILER="$BUILD_DIR/Quail_Compiler"
OUT_DIR="$BUILD_DIR/out"
TEST_DIR="$BUILD_DIR/test"

# ── Build if binary missing ──────────────────────────────────
if [ ! -f "$COMPILER" ]; then
    echo ">>> Compiler binary not found. Building..."
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR" || exit 1
    cmake ..
    make
    cd "$SCRIPT_DIR" || exit 1
fi

if [ ! -f "$COMPILER" ]; then
    echo "ERROR: Build failed. Is LLVM installed?"
    exit 1
fi

# ── Parse arguments ──────────────────────────────────────────
BUILD_FLAG=""
DEBUG_FLAG=""
SINGLE_FILE=""

for arg in "$@"; do
    case "$arg" in
        --build) BUILD_FLAG="--build" ;;
        --debug) DEBUG_FLAG="--debug" ;;
        *.mc)    SINGLE_FILE="$arg"   ;;
    esac
done

# ── Dispatch ─────────────────────────────────────────────────
if [ -n "$SINGLE_FILE" ]; then
    echo ">>> Compiling: $SINGLE_FILE"
    "$COMPILER" $DEBUG_FLAG $BUILD_FLAG --out "$OUT_DIR" "$SINGLE_FILE"

elif [ -n "$BUILD_FLAG" ]; then
    echo ">>> Test suite: compile + link + run all..."
    "$COMPILER" --test-all --build \
        --testdir "$TEST_DIR" \
        --out "$OUT_DIR"

else
    echo ">>> Test suite: IR generation only..."
    "$COMPILER" --test-all \
        --testdir "$TEST_DIR" \
        --out "$OUT_DIR"
fi