# Quail Compiler v5.0 — Implementation & Technical Documentation

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Directory Structure](#directory-structure)
3. [Compilation Pipeline](#compilation-pipeline)
4. [Lexer](#lexer)
5. [Parser](#parser)
6. [AST Node Catalogue](#ast-node-catalogue)
7. [Semantic Analysis — TypeChecker](#semantic-analysis--typechecker)
8. [Symbol Table](#symbol-table)
9. [Code Generation (LLVM)](#code-generation-llvm)
10. [Optimization (LLVM)](#optimization-llvm)
11. [Auto-Corrector](#auto-corrector)
12. [Native IR Layer](#native-ir-layer)
13. [SSA Construction](#ssa-construction)
14. [Optimization Passes (Native IR)](#optimization-passes-native-ir)
15. [Register Allocator](#register-allocator)
16. [CFG Analyzer](#cfg-analyzer)
17. [AST Grapher](#ast-grapher)
18. [IR Printer](#ir-printer)
19. [Main Driver](#main-driver)
20. [Build System](#build-system)
21. [Known Limitations & Extension Points](#known-limitations--extension-points)

---

## Architecture Overview

```
source.mc
    │
    ▼  ── FRONTEND ──────────────────────────────────────────
    │  Lexer          tokenize()  →  vector<Token>
    │  Parser         parse()     →  unique_ptr<AST>
    │  TypeChecker    check()     →  vector<TypeError>
    │
    ├──── QUAIL NATIVE IR (independent of LLVM) ─────────────
    │  IRBuilder      build()     →  IRModule (three-address)
    │  SSAPass        toSSA()     →  phi-nodes + renaming
    │  OptPasses      run()       →  IRModule (optimized)
    │  SSAPass        fromSSA()   →  copies replace phis
    │  RegisterAllocator allocate() → register assignment
    │  IRPrinter      printModule() → console output
    │
    ▼  ── LLVM BACKEND ──────────────────────────────────────
    │  CodeGen        generate()  →  llvm::Module
    │  CodeGen        optimize()  →  llvm::Module (LLVM passes)
    │  CodeGen        dumpToFile()→  <stem>.ll
    │
    ├──── ANALYSIS (from LLVM module) ───────────────────────
    │  CFGAnalyzer    analyze()   →  DOT files, complexity
    │  ASTGrapher     saveDOT()   →  AST DOT file
    │
    ▼  ── NATIVE TARGET (optional --build) ──────────────────
       llc   <stem>.ll  →  <stem>.o
       clang <stem>.o   →  <stem>  (native binary)
```

---

## Directory Structure

```
Quail_Compiler/
├── CMakeLists.txt
├── include/
│   ├── analysis/
│   │   ├── ASTGrapher.h
│   │   └── CFGAnalyzer.h
│   ├── autocorrect/
│   │   └── AutoCorrector.h
│   ├── codegen/
│   │   └── CodeGen.h
│   ├── ir/
│   │   ├── IRBuilder.h      ← AST → three-address IR
│   │   ├── IRCFG.h          ← BasicBlock, IRFunction, IRModule
│   │   ├── IRInstruction.h  ← IROp enum + IRInstruction struct
│   │   ├── IRIntegration.h  ← runIRPipeline() helper
│   │   ├── IRPipeline.h     ← orchestrates all IR stages
│   │   ├── IRPrinter.h      ← formatted console output
│   │   └── IRValue.h        ← operand representation
│   ├── lexer/
│   │   ├── Lexer.h
│   │   └── Token.h
│   ├── optimizer/
│   │   ├── OptimizationPasses.h
│   │   ├── RegisterAllocator.h
│   │   └── SSAPass.h
│   ├── parser/
│   │   ├── AST.h
│   │   └── Parser.h
│   ├── semantic/
│   │   ├── SymbolTable.h
│   │   └── TypeChecker.h
│   └── utils/
│       └── Logger.h
├── src/                      ← mirrors include/; one .cpp per .h
├── test/                     ← 30 .mc test files + sample*.mc
└── run_tests.sh
```

---

## Compilation Pipeline

`main.cpp` orchestrates everything inside `compileSinglePass()`. The call chain for a single file is:

```
compileSinglePass()
  1. Lexer::tokenize()
  2. Parser::parse()
  3. [if --ir*] runIRPipeline()          ← Quail native IR
  4. TypeChecker::check()
  5. CodeGen::generate()
  6. CodeGen::optimize()
  7. CodeGen::dumpToFile()               → stem.ll
  8. [if --cfg/--ast-graph/...] runGraphAnalysis()
  9. [if --build] shell: llc + clang + run
```

When errors are detected in the probe pass, `compileOne()` invokes `AutoCorrector`, saves `<stem>_corrected.mc`, and re-runs `compileSinglePass()` on the corrected source.

---

## Lexer

**File:** `src/lexer/Lexer.cpp`

Single-pass character-by-character scanner. State is kept in three fields: `pos` (byte offset), `line` (current source line), and `keepComments` (flag).

### Token types (TokenType enum)

Defined in `include/lexer/Token.h`. Key groupings:

- **Type keywords**: `INT FLOAT VOID CONST STRING_TYPE`
- **Control flow**: `IF ELSE WHILE FOR BREAK CONTINUE SWITCH CASE DEFAULT RETURN`
- **OOP**: `CLASS NEW DELETE THIS PUBLIC PRIVATE EXTENDS SUPER OVERRIDE`
- **I/O**: `PRINT PRINTLN SCAN`
- **Literals**: `NUMBER FLOAT_VAL STRING_LIT IDENT`
- **Operators**: `PLUS MINUS MUL DIV ASSIGN EQ NEQ INC LT GT LE GE AND OR NOT AMPERSAND ARROW DOT`
- **Delimiters**: `LPAREN RPAREN LBRACE RBRACE LBRACKET RBRACKET SEMI COMMA COLON`
- **Comments**: `LINE_COMMENT BLOCK_COMMENT` (preserved when `keepComments=true`)
- **Sentinel**: `EOF_TOK`

### Special handling

| Input | Behaviour |
|-------|-----------|
| `// text` | Emits `LINE_COMMENT` with trimmed body |
| `/* ... */` | Emits `BLOCK_COMMENT`; unterminated → `LexError` |
| `"..."` | Emits `STRING_LIT`; escape sequences `\n \t \" \\ \0` processed |
| `++` | Single `INC` token (not two `PLUS`) |
| `->` | Single `ARROW` token |
| `&&` / `\|\|` | Two-character operators |
| `&` alone | `AMPERSAND` (address-of) |
| `|` alone | `LexError` ("did you mean \|\|?") |
| Unknown char | `LexError`; scanning continues |

### Error recovery

Errors are pushed to `vector<LexError>`. The lexer never throws; it records the error and advances `pos` by one character.

---

## Parser

**File:** `src/parser/Parser.cpp`

Recursive-descent parser. Produces a `ProgramAST` containing a flat list of top-level items (functions and class declarations).

### Grammar entry points

| Method | Parses |
|--------|--------|
| `parse()` | Top-level: `function*` and `class*` |
| `function()` | `retType name ( params ) block` |
| `parseClass()` | `class name [extends parent] { members }` |
| `block()` | `{ statement* }` |
| `statement()` | All statement forms |
| `parseExpression(minPrec)` | Pratt parser with precedence climbing |
| `primary()` | Atoms: literals, identifiers, calls, `this`, `super`, `new`, `&`, `*`, `(expr)` |

### Operator precedence table

| Precedence | Operators |
|-----------|-----------|
| 70 | `* /` |
| 60 | `+ -` |
| 50 | `< > <= >=` |
| 45 | `== !=` |
| 30 | `&&` |
| 20 | `\|\|` |

`&&` and `||` produce `LogicalAST`; all others produce `BinaryAST`.

### Look-ahead disambiguation

The parser uses saved-position backtracking (`savedPos`) in three places:

1. `this.field = expr` vs `this.method()`
2. `ptr->field` vs `ptr->method()`
3. `obj.field = expr` vs `obj.method()`

In each case the parser speculatively consumes tokens; if the next token is not `ASSIGN` it restores `pos` and falls through to the expression path.

### Class name registry

`classNames` (`set<string>`) is populated as `ClassDeclAST` nodes are built. This allows the statement parser to recognize `ClassName varName;` as an object declaration before the identifier is consumed.

### Error recovery

`addError()` deduplicates by `(line, message)`. `syncStatement()` advances to the next `;` or `}`. `syncFunction()` advances to the next top-level function or class signature.

---

## AST Node Catalogue

All nodes inherit from `struct AST`. Every node implements `virtual void print(int indent)`.

### Literals and variables

| Node | Fields |
|------|--------|
| `NumberAST` | `int val` |
| `FloatAST` | `double val` |
| `StringAST` | `string value` |
| `VariableAST` | `string name` |

### Declarations

| Node | Fields |
|------|--------|
| `VarDeclAST` | `string name, ASTType type` |
| `VarDeclInitAST` | `name, type, unique_ptr<AST> init` |
| `ConstVarDeclInitAST` | same as above |
| `ArrayDeclAST` | `name, int size, ASTType type` |
| `PtrVarDeclAST` | `name, ASTType baseType` |
| `PtrVarDeclInitAST` | `name, baseType, unique_ptr<AST> init` |
| `StringVarDeclAST` | `name, unique_ptr<AST> init` |

### Expressions

| Node | Fields |
|------|--------|
| `BinaryAST` | `string op, lhs, rhs` |
| `LogicalAST` | `string op, lhs, rhs` |
| `UnaryAST` | `string op, unique_ptr<AST> operand` |
| `PostIncAST` | `string name` |
| `AssignAST` | `string name, unique_ptr<AST> expr` |
| `ArrayAssignAST` | `name, index, expr` |
| `ArrayAccessAST` | `name, unique_ptr<AST> index` |
| `CallAST` | `string callee, vector<unique_ptr<AST>> args` |

### Pointer / heap nodes

| Node | Fields |
|------|--------|
| `AddressOfAST` | `string name` |
| `DerefAST` | `unique_ptr<AST> operand` |
| `DerefAssignAST` | `ptr, expr` |
| `NewExprAST` | `string className` |
| `NewScalarAST` | `ASTType type` |
| `DeleteAST` | `string ptrName` |

### OOP nodes

| Node | Fields |
|------|--------|
| `ClassDeclAST` | `name, parentName, vector<ClassField> fields, vector<unique_ptr<FunctionAST>> methods` |
| `ObjectDeclAST` | `className, varName` |
| `MemberAccessAST` | `objName, memberName` |
| `MemberAssignAST` | `objName, memberName, expr` |
| `MethodCallAST` | `objName, methodName, args` |
| `ThisAccessAST` | `memberName` |
| `ThisAssignAST` | `memberName, expr` |
| `ArrowAccessAST` | `objPtrName, memberName` |
| `ArrowAssignAST` | `objPtrName, memberName, expr` |
| `SuperCallAST` | `methodName, args` |

### Control flow and I/O

| Node | Fields |
|------|--------|
| `IfAST` | `cond, thenBlock, elseBlock` |
| `WhileAST` | `cond, body` |
| `ForAST` | `init, cond, inc, body` |
| `SwitchAST` | `expr, vector<CaseClause> cases` |
| `CaseClause` | `unique_ptr<AST> value (null=default), vector<stmts>, bool hasBreak` |
| `BreakAST` | — |
| `ContinueAST` | — |
| `ReturnAST` | `unique_ptr<AST> expr` |
| `PrintAST` | `vector<unique_ptr<AST>> exprs, bool newline` |
| `ScanAST` | `vector<string> varNames` |
| `FunctionAST` | `unique_ptr<PrototypeAST> proto, unique_ptr<BlockAST> body` |
| `BlockAST` | `vector<unique_ptr<AST>> statements` |
| `ProgramAST` | `vector<unique_ptr<AST>> topLevel` |

### String operations

| Node | Fields |
|------|--------|
| `StrLenAST` | `string varName` |
| `StrCmpAST` | `lhs, rhs` |
| `StrCatAST` | `string dest, src` |

---

## Semantic Analysis — TypeChecker

**File:** `src/semantic/TypeChecker.cpp`

Two-pass visitor over the AST.

**Pass 1 — `collectSignatures()`**: Registers all function and method signatures (`funcReturnTypes`, `funcParamTypes`) using their AST prototypes. Methods are registered under both the mangled name (`ClassName_method`) and the short name.

**Pass 2 — `check()`**: Walks every function and class, calling `checkStmt()` and `typeOf()` on each node.

### `typeOf(AST*)` — type inference

Returns `ASTType` conservatively (defaults to `Int` for unknowns like pointer-typed results). Key rules:

- Binary arithmetic: result is `wider(lhs, rhs)`; emits error if types are incompatible
- Comparison operators always return `Int`
- Call to void function used as value → error
- `!` on `Float` → warning-level error

### Checks performed

| Check | Error message pattern |
|-------|-----------------------|
| Incompatible binary operands | `"Type mismatch in binary expression"` |
| Return type mismatch | `"Return type mismatch in 'fn'"` |
| Void function return with value | `"is void but attempts to return a value"` |
| Non-void function missing return value | `"must return a value"` |
| Wrong argument count to function call | `"Wrong argument count to 'fn'"` |
| Argument type mismatch | `"Argument N to 'fn': expected ..."` |
| Float used as array index | `"Array index ... must be integer"` |
| Float used as switch case value | `"Switch case value must be an integer"` |
| Unary `!` on float | `"Unary '!' applied to float"` |

---

## Symbol Table

**File:** `src/semantic/SymbolTable.cpp`

Scoped symbol table backed by `vector<unordered_map<string, Symbol>>`. The outermost scope (index 0) is the global scope for functions.

### Symbol fields

```cpp
struct Symbol {
    string      name;
    SymbolKind  kind;       // Variable, Array, Function, Parameter,
                            // Object, Const, Pointer
    ValueType   type;       // Int, Float, Void, Unknown
    llvm::Value* value;     // alloca or function pointer
    int         arraySize;
    int         definedAtDepth;
    string      ownerFunction;
    string      objectClass;   // for kind==Object
    int         readCount;     // usage tracking for unused-var warnings
    int         line;
};
```

### Key operations

| Method | Behaviour |
|--------|-----------|
| `enterScope()` | Push new `unordered_map` |
| `exitScope()` | Pop top scope (throws if at global) |
| `insert(name, ...)` | Insert into top scope; throws on redeclaration |
| `insertFunction(name, ...)` | Inserts into **global** scope (index 0) |
| `lookup(name)` | Walks scopes from innermost outward |
| `markRead(name)` | Increments `readCount` on the found symbol |
| `collectUnusedWarnings()` | Returns all symbols with `readCount == 0` except functions and objects |

A `SymbolLogEntry` (flat copy without LLVM pointer) is appended to `log` on every insertion. The log is used for the `--debug` symbol table display.

---

## Code Generation (LLVM)

**File:** `src/codegen/CodeGen.cpp`

`CodeGen::generate(AST*)` is a large recursive visitor implemented as a chain of `dynamic_cast` checks (no virtual dispatch on AST nodes). It builds LLVM IR into a single `llvm::Module` using `llvm::IRBuilder<>`.

### Class layout

Each `ClassDeclAST` produces:

1. An `llvm::StructType` with one `i32` or `double` element per field (inherited fields first).
2. A `ClassInfo` entry in `classInfos` recording field names, types, and the LLVM struct type.
3. One LLVM function per method, named `ClassName_method`, with `%ClassName*` as the first parameter (implicit `this`).
4. For subclasses: thin wrapper shims that upcast `this` and tail-call the parent implementation.

### Object allocation

Objects declared as `ClassName varName;` are `alloca %ClassName` on the stack. `new ClassName` calls `malloc` with `sizeof(%ClassName)` computed via a null-pointer GEP trick:

```cpp
auto* nullPtr = ConstantPointerNull::get(PointerType::get(structTy, 0));
auto* sizeGEP = builder.CreateGEP(structTy, nullPtr, {one}, "sizeof_gep");
auto* sizeVal = builder.CreatePtrToInt(sizeGEP, Int64Ty, "sizeof");
```

### Type promotion

`promoteToCommon(lhs, rhs)` promotes integer operands to double when either operand is floating-point. `coerce(val, targetTy)` inserts explicit casts (`SIToFP`, `FPToSI`, `ZExt`, `ICmpNE`) as needed.

### Short-circuit evaluation

`&&` and `||` are compiled to CFG-based short-circuit with a merge block and PHI node:

```
&&: if !lhs → merge(false);  else evaluate rhs → merge(rhs)
||: if lhs  → merge(true);   else evaluate rhs → merge(rhs)
```

### Runtime function declarations

Declared lazily on first use via `getOrDeclare*()` helpers:

| C function | Used for |
|-----------|---------|
| `printf` | `print` / `println` |
| `scanf` | `scan` |
| `malloc` | `new` |
| `free` | `delete` |
| `strlen` | `strlen(s)` |
| `strcmp` | `strcmp(a,b)` |
| `strcat` | `strcat(dest,src)` |
| `strcpy` | (available but not yet surface-level) |

Format strings are cached in `fmtCache` as private global constants to avoid duplicates.

### Break/continue stacks

`breakStack` and `continueStack` hold `BasicBlock*` targets. `break` creates `CreateBr(breakStack.back())`. `continue` creates `CreateBr(continueStack.back())`.

### Unused variable warnings

After `generate()`, `collectUnusedWarnings()` delegates to `SymbolTable::collectUnusedWarnings()` which scans all scopes for symbols with `readCount == 0`.

---

## Optimization (LLVM)

**File:** `src/codegen/CodeGen.cpp` — `CodeGen::optimize()`

Uses LLVM's new pass manager (`PassBuilder`).

| Level | Passes |
|-------|--------|
| O0 | None |
| O1 | `PromotePass`, `InstCombinePass`, `ReassociatePass`, `GVNPass`, `SimplifyCFGPass` |
| O2 | `PassBuilder::buildPerModuleDefaultPipeline(O2)` |
| O3 | `PassBuilder::buildPerModuleDefaultPipeline(O3)` |

`OptStats` records instruction and block counts before/after per function for the `--show-ir-diff` report.

---

## Auto-Corrector

**File:** `src/autocorrect/AutoCorrector.cpp`

Takes the original source split into lines plus three error vectors. All fixes operate on `vector<string> lines` in-place. A `set<pair<int,string>> applied` prevents double-application of the same fix to the same line.

### Fix strategies

**`fixLexErrors()`**
- Unknown character: `removeCharFromLine(li, bad_char)`
- Unterminated string: insert `"` at `findSemiInsertPos(line)` (before any inline comment)
- Unterminated block comment: append `*/` to the last line

**`fixParseErrors()`**
- Missing `;`: insert at `findSemiInsertPos(line-2)` (one line before the error line)
- Missing `}`: queue for deferred insertion (sorted in reverse order to maintain indices)
- Missing `{`: append to keyword line
- Missing `)` / `]`: append to line
- Missing `:` after `case`/`default`: append to line
- Const without init: append `= 0` before `;`
- Keyword typo: Levenshtein distance ≤ 2 → replace with closest keyword

**`fixCodeGenErrors()`**
- Undeclared variable: collect names → insert `int name;` after `main()` opening brace
- Write to const: find declaration line with `const` → replace `const` with `int  `

**`findSemiInsertPos(line)`**: Finds the start of any inline comment and inserts before it (after stripping trailing whitespace), so the fix does not corrupt comment text.

**`suggestKeyword(tok)`**: Compares `tok` against a fixed list of 24 keywords using Levenshtein distance. Returns the closest keyword if distance ≤ 2.

---

## Native IR Layer

### IRValue (`include/ir/IRValue.h`)

Discriminated union over six kinds:

```
Temporary   — compiler-generated temp  (t0, t1, ...)
Variable    — user-declared name
IntConst    — integer literal
FloatConst  — double literal
Label       — jump target name
Undefined   — placeholder / uninitialized
```

### IRInstruction (`include/ir/IRInstruction.h`)

Quadruple: `(IROp op, IRValue arg1, IRValue arg2, IRValue result)`

Additional fields:
- `vector<IRValue> callArgs` — for CALL instructions
- `vector<pair<string,IRValue>> phiSources` — for PHI nodes
- `bool isDead` — DCE marker
- `string comment`, `int lineHint`

Factory helpers: `makeAssign`, `makeBinop`, `makeUnop`, `makeLabel`, `makeJump`, `makeCJump`, `makeReturn`, `makeCall`, `makePhi`, `makeNop`.

### BasicBlock (`include/ir/IRCFG.h`)

```cpp
struct BasicBlock {
    string label;
    vector<IRInstruction> instrs;
    vector<string> successors, predecessors;
    bool isEntry, isExit, isReachable, isLoopHead, isDead;
    int  loopDepth, idomId, id;
    unordered_set<string> liveIn, liveOut, gen, kill;
    vector<int> domChildren;
    unordered_set<int> domFrontier;
};
```

### IRFunction

Owns `vector<unique_ptr<BasicBlock>>` plus a `blockMap` for O(1) label lookup. Key methods:

| Method | Description |
|--------|-------------|
| `addBlock(label)` | Allocates new block, inserts into map |
| `addEdge(from, to)` | Updates successor/predecessor lists |
| `rebuildEdges()` | Rebuilds edges from terminator instructions |
| `markReachable()` | BFS from entry block |
| `computeDominators()` | Iterative Cooper-Harvey-Kennedy algorithm |
| `computeLiveness()` | Backward dataflow (gen/kill per block) |
| `detectLoops()` | DFS coloring; back-edges → `LoopInfo` list |

### IRModule

Top-level container: `vector<unique_ptr<IRFunction>>` plus globals list `vector<tuple<string,string,IRValue>>`.

---

## IRBuilder

**File:** `src/ir/IRBuilder.cpp`

Walks the AST and emits three-address instructions into the current `BasicBlock`. Uses a per-block `unordered_map<DAGKey, IRValue>` for local CSE during emission (`emitBinopDag`). Commutative operations are normalized (smaller string first) before DAG lookup.

### Key design decisions

- `varMap` maps source variable names to their current `IRValue` (temp or var). Updated on every assignment.
- `dag` is cleared on every `switchTo(newBlock)` call.
- `breakTargets` / `continueTargets` stacks mirror the LLVM codegen stacks.
- `build()` transfers ownership of `IRModule` via `unique_ptr` (no raw-pointer double-free).

### Logical operator lowering

`&&` and `||` generate explicit basic blocks (true-branch, false-branch, merge) and a PHI node — matching the LLVM codegen strategy.

---

## SSA Construction

**File:** `src/optimizer/SSAPass.cpp`

Implements the Cytron et al. (1991) algorithm.

### `toSSA(IRFunction&)`

1. **`computeDomFrontier()`** — Iterative dominator frontier. For each join point `y` (≥ 2 predecessors), walks the dominator tree from each predecessor upward until reaching `idom(y)`, collecting frontier entries. Guard against self-referential `idomId` prevents infinite walks.

2. **`placePhi()`** — Iterated dominance frontier. Worklist starts with blocks that define each variable. For each block in the DF of a worklist entry, inserts a PHI node (one source per predecessor) if not already placed, then adds that block to the worklist.

3. **`rename()`** — DFS over dominator tree (`renameBlock()`). Maintains a stack of versioned names per base variable. On each definition, pushes a new name `base.version`. On each use (non-PHI), substitutes the top-of-stack name. Fills PHI sources at successor boundaries. Pops names when unwinding.

### `fromSSA(IRFunction&)`

For each PHI node `result = phi(v1<bb1>, v2<bb2>, ...)`:
- Inserts `result = v_i` at the end of each predecessor `bbi` (before the terminator)
- Removes the PHI instruction from the current block

---

## Optimization Passes (Native IR)

All passes implement `class IRPass { virtual void run(IRFunction&); }`. Stats are accumulated in `PassStats`.

### ConstantFoldingPass

Iterates instructions until no change. Calls `tryFold()` which handles:
- `BinaryOp(IntConst, IntConst)` → evaluate at compile time
- `BinaryOp(FloatConst/IntConst, FloatConst/IntConst)` → evaluate as double
- `UnaryOp(Const)` → evaluate at compile time
- **Guard**: skips `ASSIGN/COPY` instructions whose `arg1` is already a constant to prevent infinite re-folding.

### ConstantPropagationPass

`buildConstantMap()` identifies variables with exactly one definition that is a constant assignment. `rewriteUses()` substitutes those values everywhere they are used. Repeats until no change.

### CSEPass

Per-block forward pass. Maintains `exprMap: (op, arg1_str, arg2_str) → result_name`. Commutative ops are normalized. On hit: replace instruction with `COPY`. Invalidates entries whose operands were redefined.

### CopyPropagationPass

Maintains `copyOf: name → IRValue` chain. `resolve()` chases the chain transitively (depth-limited to 16). `killCopiesOf(name)` removes entries whose source was redefined.

### DeadCodeEliminationPass

Backward liveness walk. Seeds `live` set from terminators and side-effecting instructions. Propagates backwards: if result is live, add operands to `live`. Removes instructions where result is not live and `canEliminate()` is true.

### StrengthReductionPass

`tryReduce()` matches algebraic identities:
- `x ± 0`, `x * 1`, `x / 1` → `ASSIGN x`
- `x * 0` → `ASSIGN 0`
- `x * 2^n` → `SHL n`, `x / 2^n` → `SHR n`
- `x - x` → `0`, `x == x` → `1`, `x != x` → `0`
- `x * 2` → `ADD x x`

### LICMPass

1. Snapshot `fn.loops` to avoid invalidation during structural changes.
2. For each loop, collect all definitions inside (`collectLoopDefs()`).
3. `isInvariant()`: instruction is invariant if it has no side effects, is not a LOAD/PHI/terminator, and all operands are either constants or defined outside the loop.
4. `getOrCreatePreHeader()`: redirects outside predecessors of the loop header to a new pre-header block.
5. Moves invariant instructions to the pre-header (before the jump). Capped at 32 inner iterations to prevent oscillation.

### InductionVarPass

`detectIVs()`: finds basic IVs (`v = v + c` in a loop body) and derived IVs (`j = scale * basicIV`). `processLoop()` replaces derived IV multiplications with additive increments using the basic IV's step.

### PeepholePass

Sliding window over each block's instruction list:
- **NOP removal**: erase `NOP` and `isDead` instructions
- **Constant branch**: fold `if constant goto L1 else L2` → `goto L_chosen`
- **Double negation**: `neg(neg(x))` → `x`; `lnot(lnot(x))` → `x != 0`
- **Redundant copy**: `t1 = t2; t3 = t1` → `t3 = t2; NOP`
- **Identity ops**: `x = x` → `NOP`
- **Redundant jump**: jump to the immediately following block → `NOP`

### BasicBlockOptPass

Capped at 64 iterations:
1. **`removeUnreachable()`** — BFS from entry; remove unreachable blocks
2. **`mergeBlocks()`** — If block B has one successor S and S has one predecessor, append S into B and remove S
3. **`removeEmptyBlocks()`** — If block has only an unconditional jump, redirect predecessors and mark dead
4. **`threadJumps()`** — If a jump target itself has only an unconditional jump, bypass it

---

## Register Allocator

**File:** `src/optimizer/RegisterAllocator.cpp`

### Phase 1 — Interference graph

`computeLiveness()` computes `liveIn`/`liveOut` per block (backward dataflow). Then, for each block, walks instructions in reverse: the defined temporary interferes with every other currently-live temporary. Adds undirected edges to `graph`.

### Phase 2 — Simplify (Chaitin-Briggs)

Repeatedly picks a node with degree < K and pushes it onto an order stack. If no such node exists, picks a **spill candidate** using a heuristic score:

```
score = degree * 100 / (useCount + 1)
```

Higher score = better spill target (high degree relative to usage).

### Phase 3 — Color

Pops nodes from the stack in reverse simplify order. For each node, finds the lowest color index not used by any already-colored neighbor.

### Phase 4 — Spill code

For each spilled temporary:
- Before each **use**: insert `LOAD result_reload = *slot`; redirect the use to `result_reload`
- After each **definition**: insert `STORE *slot = result`

Iterates up to 5 times (spill code introduces new temporaries that need allocation).

### Use counting

`countUses()` weights each use by `10 * loopDepth` for uses inside loops, to prefer spilling loop-exterior temporaries.

---

## CFG Analyzer

**File:** `src/analysis/CFGAnalyzer.cpp`

Operates on `llvm::Module` (LLVM IR) after codegen.

### `analyze()`

Iterates `llvm::Function` objects. For each:
1. Names every `llvm::BasicBlock` (uses block name if present, else `bb<idx>`)
2. Builds `CFGBlock` structs with instruction count, terminator kind, IR preview (first 5 instructions)
3. Builds edges from LLVM successor iteration
4. Calls `detectBackEdges()` (DFS coloring: white=0, grey=1, black=2) — grey→grey edge is a back-edge
5. Calls `detectDeadBlocks()` (BFS from entry; unreachable = dead)
6. Computes `cyclomaticComplexity = numEdges - numNodes + 2`
7. Records caller→callee pairs for call graph (skipping `printf`, `scanf`, `malloc`, `free`, `memcpy`, `memset`, `puts`)

### DOT generation

`generateFunctionDOT()`: one DOT file per function. Nodes colored by role (entry=green, exit=red, loop header=orange, dead=grey). Back-edges rendered as red dashed arrows.

`generateAllFunctionsDOT()`: subgraph cluster per function, colored by cyclomatic complexity (green ≤ 5, yellow ≤ 10, orange ≤ 20, red > 20).

`generateCallGraphDOT()`: recursive calls in red dashed, external calls in grey dotted.

---

## AST Grapher

**File:** `src/analysis/ASTGrapher.cpp`

Recursive visitor that assigns a unique DOT node ID to each AST node and emits `node [label=... shape=... fillcolor=...]` followed by `parent → child` edges.

`attrsFor(AST*)` maps each concrete AST type to a `{label, fill, shape, textColor, penWidth}` struct via `dynamic_cast` chain.

`computeStats()` / `statsVisit()` counts nodes by category (expressions, statements, declarations, I/O, OOP, comments) and tracks max tree depth.

---

## IR Printer

**File:** `src/ir/IRPrinter.cpp`

`printModule()` → `printFunction()` → `printBlock()` → `printInstruction()`.

`printInstruction()` color-codes by category: red for terminators, magenta for calls, green for PHI nodes, yellow for memory ops, cyan for binary arithmetic.

`printQuadrupleTable()` formats instructions as `(#, OP, ARG1, ARG2, RESULT)` aligned columns.

`dagToDOT(BasicBlock&)` reconstructs the value DAG by examining instruction definitions and operands, then emits a Graphviz DOT.

`interferenceGraphToDOT(IRFunction&, RegisterAllocator&)` emits the interference graph with register-colored nodes.

`computeStats(IRModule&)` aggregates: function count, basic block count, instruction count, phi nodes, branches, calls, memory ops, loop count, max loop depth, temporaries.

---

## Main Driver

**File:** `src/main.cpp`

### `compileSinglePass()`

Central compilation function. Returns `CompileResult` with fields: `parseOk`, `irOk`, `linkOk`, `exitCode`, `errorCount`, `warnCount`, paths to generated files, DOT/PNG lists.

### `compileOne()`

Wrapper that runs a silent probe pass first (keepComments=false, no display). If errors are found, invokes `AutoCorrector`, saves corrected file, runs `compileSinglePass()` on it. Returns result of whichever pass succeeded.

### `runTestSuite()`

Iterates `.mc` files in `testDir` (alphabetically sorted). Calls `compileOne()` for each with `verbose=false`. Prints a formatted summary table.

### `runGraphAnalysis()`

Called after codegen when any graph flag is set. Constructs `CFGAnalyzer` and `ASTGrapher`, generates DOT files, optionally shells out to `dot -Tpng`.

### ANSI output

Colour constants (`RED`, `GREEN`, `YELLOW`, etc.) applied throughout. All coloured output goes to `stdout` except error messages which go to `stderr`.

---

## Build System

**File:** `CMakeLists.txt`

```cmake
find_package(LLVM REQUIRED CONFIG)
llvm_map_components_to_libnames(LLVM_LIBS
    core support irreader passes analysis
    transformutils scalaropts instcombine ipo vectorize
    objcarcopts target x86codegen x86asmparser
    [aarch64codegen aarch64asmparser]   # if available
)
target_link_libraries(Quail_Compiler PRIVATE ${LLVM_LIBS})
```

AArch64 availability is detected via `llvm-config --targets-built`. If present, `QUAIL_HAS_AARCH64` is defined and the AArch64 codegen/asmparser libraries are linked.

Post-build commands copy `test/` into the build directory and create `out/`.

### Build commands

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

---

## Known Limitations & Extension Points

| Limitation | Where to fix |
|-----------|-------------|
| No virtual dispatch / vtable | `CodeGen.cpp` — add `i8**` vtable pointer as field 0; generate vtable globals; replace direct calls with indirect loads |
| No constructors | Parser: add `constructor` keyword; CodeGen: call `ClassName_constructor` after alloca |
| `public`/`private` not enforced | `TypeChecker.cpp` — track visibility in `ClassInfo`; emit error on cross-boundary access |
| Arrays inside class fields not supported | CodeGen: inline array as struct member; change GEP index arithmetic |
| No closures or lambdas | Would require heap-allocated environment structs |
| No generics / templates | Would require monomorphisation pass |
| IR pipeline and LLVM pipeline separate | Could lower IR instructions to LLVM MachineInstr for unified backend |
| No `do-while` | Parser: add `DO` token; emit as `body; while(cond)` pattern |
| No `%` operator in IR pipeline | `IRBuilder.cpp` `binopFor()` maps `%` → `IROp::MOD`; `ConstantFoldingPass` handles it; just needs test coverage |
| Switch fall-through in IR builder | `IRBuilder::genStmt` SwitchAST not yet implemented (falls through to expression path) |
| LICM may move instructions with undetected aliasing | Would need alias analysis integration |
| Register allocator does not coalesce copies | Add Briggs-style coalescing: merge non-interfering COPY sources |
