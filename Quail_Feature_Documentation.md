# Quail Compiler v5.0 — Feature Documentation

---

## Table of Contents

1. [Overview](#overview)
2. [Language Reference](#language-reference)
3. [Compiler Flags & CLI](#compiler-flags--cli)
4. [IR Pipeline](#ir-pipeline)
5. [Analysis & Graph Outputs](#analysis--graph-outputs)
6. [Auto-Correction](#auto-correction)
7. [Test Suite](#test-suite)
8. [Quick Examples](#quick-examples)

---

## Overview

Quail is a statically-typed, C-like compiled language. Source files use the `.mc` extension. The compiler lowers Quail source through lexing, parsing, type-checking, LLVM IR codegen, optimization, and (optionally) native linking. A parallel hand-written three-address IR pipeline performs independent analysis, SSA construction, and graph-coloring register allocation.

**Toolchain dependencies**

| Tool | Purpose |
|------|---------|
| LLVM 14+ | IR backend, optimization, object emit |
| clang / gcc | Native linking |
| llc | IR → object file |
| graphviz (`dot`) | Rendering DOT graphs to PNG |
| aarch64-linux-gnu-gcc | AArch64 cross-linking (optional) |

---

## Language Reference

### Primitive Types

| Type | Width | Notes |
|------|-------|-------|
| `int` | 32-bit signed | default integer |
| `float` | 64-bit double | IEEE 754 |
| `void` | — | function return only |
| `string` | `i8*` pointer | heap or rodata |
| `const int` / `const float` | same as base | read-only; must be initialized |

### Variables

```quail
int x;              // declaration, zero-initialized
int y = 42;         // declaration + init
float pi = 3.14;
const int MAX = 100;// read-only; assignment causes compile error
```

### Pointers

```quail
int* p = &x;        // take address
*p = 10;            // dereference-assign
int v = *p;         // dereference-read
```

Arrow access on class object pointers:

```quail
MyClass* obj = new MyClass;
obj->field = 5;
int v = obj->field;
```

### Strings

```quail
string s = "hello";
int len = strlen(s);
int cmp = strcmp(s, "world");  // 0 if equal
strcat(dest, src);             // appends src into dest
```

### Arrays

```quail
int arr[10];        // stack-allocated, size must be a literal integer
arr[0] = 1;
int v = arr[0];
```

---

### Operators

| Category | Operators |
|----------|-----------|
| Arithmetic | `+  -  *  /  %` |
| Comparison | `==  !=  <  >  <=  >=` |
| Logical | `&&  \|\|  !` |
| Unary | `-expr`  `!expr`  `expr++` (post) |
| Address / deref | `&var`  `*expr` |
| Member | `obj.field`  `ptr->field` |
| Assignment | `=` |

Operator precedence (high → low): `*  /` → `+  -` → `<  >  <=  >=` → `==  !=` → `&&` → `||`

---

### Control Flow

#### if / else

```quail
if (x > 0) {
    println(x);
} else {
    println(0);
}
```

#### while

```quail
while (i < 10) {
    i++;
}
```

#### for

```quail
for (i = 0; i < n; i++) {
    arr[i] = i * 2;
}
```

#### break / continue

```quail
for (i = 0; i < 10; i++) {
    if (i == 5) break;
    if (i == 3) continue;
    println(i);
}
```

#### switch / case / default

```quail
switch (x) {
    case 1:
        println(1);
        break;
    case 2:
        println(2);
        break;
    default:
        println(0);
        break;
}
```

Case values must be **integer constants**. Fall-through is supported (omit `break`).

---

### Functions

```quail
int add(int a, int b) {
    return a + b;
}

void greet() {
    println("hello");
    return;
}

// recursion
int fib(int n) {
    if (n <= 1) { return n; }
    return fib(n - 1) + fib(n - 2);
}
```

- Functions must be declared before use (forward declarations are not supported; define in order).
- `void` functions may omit `return`.

---

### Classes

```quail
class Animal {
    public int age;
    public int weight;

    public void setAge(int a) {
        this.age = a;
    }

    public int getAge() {
        return this.age;
    }
}
```

**Object instantiation (stack)**

```quail
Animal dog;
dog.setAge(3);
int a = dog.getAge();
```

**Heap allocation**

```quail
Animal* dog = new Animal;
dog->age = 3;
delete dog;
```

**Inheritance**

```quail
class Dog extends Animal {
    public int breed;

    public void bark() {
        println(this.age);
    }

    public void init(int a, int b) {
        super.setAge(a);   // call parent method
        this.breed = b;
    }
}
```

- Single inheritance only (`extends`).
- `super.method(args)` calls the parent's implementation.
- `override` keyword is accepted but not enforced.
- `public` / `private` are parsed and recorded; access control is **not** enforced at compile time.
- All fields are stored in a flat `StructType`; inherited fields come first.

**`this` inside methods**

```quail
public int sum() {
    return this.x + this.y;
}

public void callOther() {
    this.reset();   // call sibling method
}
```

---

### I/O

```quail
print(x);          // printf("%d", x) — no newline
println(x);        // printf("%d\n", x) — with newline
println("text");   // string literal
println(a, b, c);  // multiple values, separate print calls

scan(x);           // scanf("%d", &x)
scan(x, y, z);     // reads multiple variables
```

`float` variables are scanned with `%lf` automatically.

---

### Heap Allocation

```quail
// Class objects
MyClass* p = new MyClass;
delete p;

// Scalar values
int* n = new int;
*n = 42;
delete n;
```

`new` calls `malloc`; `delete` calls `free`. No constructors — initialize fields explicitly.

---

## Compiler Flags & CLI

### Usage

```bash
./Quail_Compiler [OPTIONS] <file.mc>
./Quail_Compiler --test-all [OPTIONS]
```

### Compilation Options

| Flag | Description |
|------|-------------|
| `--debug` | Full token table, AST dump, class registry |
| `--build` | Compile → object → link native binary → run |
| `--arm` | Cross-compile to AArch64 (needs `aarch64-linux-gnu-gcc`) |
| `--x86` | Target x86-64 (default) |
| `--O0` | Optimization disabled |
| `--O1` | Basic: mem2reg, instcombine, GVN |
| `--O2` | Standard LLVM pipeline (default) |
| `--O3` | Aggressive LLVM pipeline |
| `--show-ir-diff` | Print IR before and after optimization |
| `--no-autocorrect` | Disable automatic syntax error correction |
| `--no-warn` | Suppress unused variable warnings |

### Path Options

| Flag | Description |
|------|-------------|
| `--testdir <dir>` | Test directory (default: `test/`) |
| `--out <dir>` | Output directory (default: `out/`) |

### Graph & Analysis Options

| Flag | Description |
|------|-------------|
| `--cfg` | Per-function CFG as DOT file |
| `--cfg-all` | All functions in one clustered CFG DOT |
| `--call-graph` | Whole-program call graph DOT |
| `--ast-graph` | AST visualization DOT file |
| `--ast-stats` | AST node counts by category |
| `--complexity` | Cyclomatic complexity per function |
| `--dead-code` | Unreachable basic-block detection |
| `--render` | Auto-render DOT → PNG via graphviz |
| `--graph` | Enable **all** analysis outputs above |

### IR Pipeline Options (Quail Native IR)

| Flag | Description |
|------|-------------|
| `--ir` | Print three-address IR listing |
| `--ir-quads` | Print quadruple table `(op, arg1, arg2, result)` |
| `--ir-ssa` | Print SSA form snapshot (before optimization) |
| `--ir-report` | Optimization pass statistics table |
| `--ir-regalloc` | Register allocation report |
| `--ir-stats` | IR metrics: blocks, temporaries, loops |
| `--ir-cfg` | Save IR CFG as DOT files |
| `--ir-dag` | Save value DAG per block as DOT files |
| `--ir-liveness` | Annotate IR with `liveIn`/`liveOut` sets |
| `--ir-render` | Render IR DOT files to PNG via graphviz |
| `--ir-O0` through `--ir-O3` | IR optimization level (default: O2) |
| `--ir-regs <N>` | Registers for allocation (default: 8) |
| `--ir-all` | Enable all IR outputs |

---

## IR Pipeline

Quail includes a hand-written three-address code IR that operates **independently of LLVM**. It is built directly from the AST.

### Pipeline Stages

```
AST
 └─► IRBuilder        → IRModule (three-address code)
      └─► SSAPass      → toSSA() — phi-node insertion + renaming
           └─► Optimization passes (per iteration)
                ├─ ConstantFolding
                ├─ ConstantPropagation
                ├─ CSE (Common Subexpression Elimination)
                ├─ CopyPropagation
                ├─ StrengthReduction
                ├─ LICM (Loop Invariant Code Motion)
                ├─ InductionVarElimination
                ├─ DeadCodeElimination
                ├─ Peephole
                └─ BasicBlockOpt
           └─► SSAPass      → fromSSA() — phi removal via copies
                └─► RegisterAllocator  → Chaitin-Briggs coloring
```

### Instruction Set (IROp)

| Category | Opcodes |
|----------|---------|
| Assignment | `ASSIGN`, `COPY` |
| Integer arithmetic | `ADD SUB MUL DIV MOD NEG` |
| Float arithmetic | `FADD FSUB FMUL FDIV FNEG` |
| Bitwise | `AND OR XOR NOT SHL SHR` |
| Integer compare | `EQ NEQ LT GT LEQ GEQ` |
| Float compare | `FEQ FNEQ FLT FGT FLEQ FGEQ` |
| Logical | `LAND LOR LNOT` |
| Type conversion | `INT_TO_FLOAT FLOAT_TO_INT` |
| Memory | `LOAD STORE ADDR_OF ARRAY_LOAD ARRAY_STORE` |
| Control flow | `LABEL JUMP CJUMP CJUMP_TRUE CJUMP_FALSE` |
| Functions | `PARAM CALL CALL_VOID RETURN RETURN_VOID` |
| Heap | `ALLOC FREE` |
| SSA | `PHI` |
| Misc | `NOP COMMENT` |

### Optimization Pass Descriptions

| Pass | What it does |
|------|-------------|
| **ConstantFolding** | Evaluates constant expressions at compile time: `3 + 4 → 7` |
| **ConstantPropagation** | Substitutes known constant values into their uses |
| **CSE** | Replaces duplicate computations with copies of the first result |
| **CopyPropagation** | Eliminates intermediate copy temporaries |
| **StrengthReduction** | Replaces `x*2^n` with `x<<n`, removes identity ops (`x+0`, `x*1`) |
| **LICM** | Hoists loop-invariant computations to a pre-header block |
| **InductionVar** | Eliminates derived induction variable multiplications |
| **DCE** | Removes definitions whose results are never used |
| **Peephole** | Removes NOPs, folds constant branches, eliminates double negation |
| **BasicBlockOpt** | Merges blocks, removes unreachable blocks, threads jumps |

### Register Allocator

Uses **Chaitin-Briggs** graph coloring:

1. **Build** — interference graph from liveness analysis
2. **Simplify** — repeatedly remove nodes with degree < K
3. **Spill** — if stuck, pick highest-degree/lowest-use node as spill candidate
4. **Color** — assign registers by popping the simplification stack
5. **Spill code** — insert `LOAD`/`STORE` pairs for spilled temporaries

Use `--ir-regs <N>` to change K (default: 8). Special registers `rSP` and `rFP` are never allocated.

---

## Analysis & Graph Outputs

All graph outputs are written to the `out/` directory (or `--out <dir>`).

### CFG (Control Flow Graph)

Generated by `CFGAnalyzer` from the LLVM module after codegen.

- **Color coding**: green = entry, red = exit, orange = loop header, grey = dead code
- **Back-edges** shown as red dashed arrows
- Each node shows instruction count and terminator kind (`ret`, `br`, `cond_br`)
- With `--cfg`: one DOT file per function (detailed, with IR preview)
- With `--cfg-all`: all functions as subgraph clusters in one file

### Call Graph

Shows caller → callee relationships. Recursive calls are highlighted in red. External runtime functions (`printf`, `malloc`, etc.) are excluded.

### AST Graph

Walks the AST and produces a color-coded Graphviz DOT. Node color legend:

| Color | Node type |
|-------|-----------|
| Dark blue | Program / Function / Class roots |
| Light blue | Literals, Variables |
| Yellow/Gold | Binary / Logical / Unary expressions |
| Green | Declarations |
| Orange | Control flow (if / while / for / return / break) |
| Teal | I/O (print / scan) |
| Purple | OOP (class / method / this) |
| Grey | Comments |

### Cyclomatic Complexity

`M = E − N + 2P` (E = edges, N = nodes, P = 1 for a single connected component).

| M | Risk label |
|---|-----------|
| ≤ 5 | LOW |
| ≤ 10 | MODERATE |
| ≤ 20 | HIGH |
| > 20 | VERY HIGH |

---

## Auto-Correction

When compilation errors are detected, the AutoCorrector attempts to fix them before a second compilation pass. Fixes are logged with before/after lines.

| Error type | Automatic fix |
|-----------|--------------|
| Missing `;` | Inserts `;` before any inline comment |
| Missing `}` | Inserts `}` after the offending line |
| Missing `{` | Appends `{` to the keyword line |
| Missing `)` or `]` | Appends the closing delimiter |
| Missing `:` after `case`/`default` | Appends `:` |
| Unterminated string literal | Closes with `"` |
| Unterminated block comment | Closes with `*/` |
| Unknown character | Removes the character |
| Keyword typo (`reutrn`, `whlie`) | Levenshtein-distance suggestion and replacement |
| Undeclared variable | Inserts `int name;` at the top of `main()` |
| Write to `const` variable | Changes `const` → `int` in the declaration |

Use `--no-autocorrect` to disable. The corrected source is saved as `<stem>_corrected.mc`.

---

## Test Suite

Run with `--test-all`. Files must be in `test/` (or `--testdir`).

| # | File | Primary feature |
|---|------|-----------------|
| 01 | `01_arithmetic.mc` | Arithmetic, precedence |
| 02 | `02_if_else.mc` | if-else |
| 03 | `03_while_loop.mc` | while, post-increment |
| 04 | `04_for_loop.mc` | for, factorial |
| 05 | `05_nested_loops.mc` | Nested loops |
| 06 | `06_break_continue.mc` | break, continue |
| 07 | `07_logical_ops.mc` | &&, \|\|, ! |
| 08 | `08_comparisons.mc` | ==, !=, <=, >= |
| 09 | `09_arrays.mc` | Arrays |
| 10 | `10_functions.mc` | Multiple functions |
| 11 | `11_recursion_fib.mc` | Recursive Fibonacci |
| 12 | `12_recursion_factorial.mc` | Recursive factorial |
| 13 | `13_unary.mc` | Unary minus |
| 14 | `14_nested_if.mc` | Deeply nested if |
| 15 | `15_bubble_sort.mc` | Bubble sort + arrays |
| 16 | `16_scoping.mc` | Block scoping |
| 17 | `17_gcd.mc` | GCD |
| 18 | `18_power.mc` | Iterative power |
| 19 | `19_post_increment.mc` | Post-increment return value |
| 20 | `20_complex.mc` | Combined features |
| 21 | `21_class_basic.mc` | Class, field, getter/setter |
| 22 | `22_class_fields.mc` | Multiple fields |
| 23 | `23_two_objects.mc` | Two independent instances |
| 24 | `24_method_calls_method.mc` | `this.method()` chaining |
| 25 | `25_method_logic.mc` | if/else inside method |
| 26 | `26_method_loop.mc` | Loop inside method |
| 27 | `27_access_modifiers.mc` | public/private modifiers |
| 28 | `28_class_and_functions.mc` | Class + free function interop |
| 29 | `29_two_classes.mc` | Two classes in one program |
| 30 | `30_oop_complex.mc` | Stack class + arrays + loops |

> Linux exit codes are 8-bit (0–255); values > 255 wrap around.

---

## Quick Examples

```bash
# Compile to LLVM IR (default)
./Quail_Compiler test/11_recursion_fib.mc

# Build and run native binary
./Quail_Compiler --build test/04_for_loop.mc

# Full debug output
./Quail_Compiler --debug test/21_class_basic.mc

# IR pipeline — three-address code + SSA + optimization report
./Quail_Compiler --ir --ir-ssa --ir-report test/15_bubble_sort.mc

# All IR outputs with DOT graphs rendered to PNG
./Quail_Compiler --ir-all --ir-render test/30_oop_complex.mc

# Register allocator with 4 registers
./Quail_Compiler --ir --ir-regalloc --ir-regs 4 test/17_gcd.mc

# CFG + call graph + AST graph rendered to PNG
./Quail_Compiler --graph --render test/30_oop_complex.mc

# Cyclomatic complexity report
./Quail_Compiler --complexity test/15_bubble_sort.mc

# Cross-compile to AArch64
./Quail_Compiler --build --arm test/21_class_basic.mc

# Batch: all 30 tests, IR + analysis
./Quail_Compiler --test-all --ir --ir-report

# Batch: build, link, and run all tests
./Quail_Compiler --test-all --build
```

---

## Output Files

| File | Produced when |
|------|--------------|
| `out/<stem>.ll` | Always |
| `out/<stem>.o` | `--build` |
| `out/<stem>` | `--build` (native binary) |
| `out/<stem>_corrected.mc` | Errors found + autocorrect |
| `out/<stem>_cfg_<fn>.dot` | `--cfg` |
| `out/<stem>_cfg_all.dot` | `--cfg-all` |
| `out/<stem>_callgraph.dot` | `--call-graph` |
| `out/<stem>_ast.dot` | `--ast-graph` |
| `out/<stem>_<fn>_cfg.dot` | `--ir-cfg` (IR pipeline CFG) |
| `out/<stem>_<fn>_<bb>_dag.dot` | `--ir-dag` |
| `*.png` | Any of the above + `--render` / `--ir-render` |
