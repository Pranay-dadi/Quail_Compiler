# Quail Compiler  v3.0 — OOP Edition

A hand-written compiler for a C-like language with **classes and objects**, targeting LLVM IR.

---

## What's new in v3.0

| Feature | Syntax |
|---|---|
| Class declaration | `class Point { int x; int y; }` |
| Method declaration | `int getX() { return this.x; }` |
| Object instantiation | `Point p;` |
| Member field access | `p.x` |
| Member field assignment | `p.x = 10;` |
| Method call | `p.getX()` |
| `this` pointer | `this.x = v;`  `this.method()` |
| `void` return type | `void reset() { this.count = 0; }` |
| Access modifiers | `public` / `private` (parsed; not enforced) |

---

## Full language feature table

| Feature | Example |
|---|---|
| Arithmetic | `a + b * 2 - c / d` |
| Variables | `int x;  x = 42;` |
| If / else | `if (x > 0) { … } else { … }` |
| While loop | `while (i < 10) { i++; }` |
| For loop | `for (i = 0; i < n; i++) { … }` |
| Break / Continue | `break;`  `continue;` |
| Logical ops | `a && b`  `a \|\| b`  `!a` |
| Comparison | `==  !=  <  >  <=  >=` |
| Arrays | `int arr[10];  arr[0] = 1;` |
| Functions | `int add(int a, int b) { return a+b; }` |
| Recursion | `int fib(int n) { return fib(n-1)+fib(n-2); }` |
| Unary minus | `-x` |
| Post-increment | `i++` |
| Scoped blocks | `{ int tmp; … }` |
| **Classes** | `class Foo { int x; }` |
| **Objects** | `Foo obj;` |
| **Member access** | `obj.x` |
| **Member assign** | `obj.x = 5;` |
| **Method call** | `obj.method(arg)` |
| **this** | `this.x = v;`  `return this.x;` |
| **void return** | `void reset() { … }` |
| **public/private** | `public int get() { … }` |

---

## OOP language reference

### Class declaration

```
class ClassName {
    [public|private] int   fieldName;
    [public|private] float fieldName2;

    [public|private] ReturnType methodName(ParamType paramName, ...) {
        // body — 'this' refers to the current object
        this.fieldName = paramName;
        return this.fieldName;
    }
}
```

- Fields are stored as a **stack-allocated LLVM struct** (`alloca %ClassName`).  
- Methods are compiled to `ClassName_methodName(%ClassName* this_arg, ...)`.  
- `this.field` is a GEP into the struct; `this.method()` is an indirect call.  
- `public` / `private` are parsed and recorded but **not enforced** at compile time.

### Object instantiation

```
ClassName varName;        // allocates struct on the stack
varName.field = value;    // stores into GEP offset
int x = varName.field;    // loads from GEP offset
varName.method(arg);      // calls ClassName_method(varName_ptr, arg)
```

### Inside a method body

```
this.field          // read a field of the current object
this.field = expr;  // write a field of the current object
this.method(args);  // call another method on the same object
```

---

## Build

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

---

## Usage

### Compile a single file

```bash
# Generate LLVM IR only
./Quail_Compiler test/21_class_basic.mc

# Show token table, AST, class registry, symbol table
./Quail_Compiler --debug test/21_class_basic.mc

# Compile + link + run native binary
./Quail_Compiler --build test/21_class_basic.mc

# Run with O0 (no optimization) to inspect raw IR
./Quail_Compiler --O0 test/21_class_basic.mc

# Show IR diff before/after optimization
./Quail_Compiler --show-ir-diff test/21_class_basic.mc
```

### Run all tests

```bash
# IR only  (all 30 tests)
./Quail_Compiler --test-all

# Build + run all tests with exit codes
./Quail_Compiler --test-all --build

# Disable autocorrect for strict error checking
./Quail_Compiler --test-all --build --no-autocorrect
```

### Options reference

| Flag | Description |
|---|---|
| `--debug` | Token table + full AST + class registry |
| `--build` | Link native binary via llc + clang and run it |
| `--O0` | No optimization |
| `--O1` | Basic: mem2reg, instcombine, GVN |
| `--O2` | Standard pipeline (default) |
| `--O3` | Aggressive pipeline |
| `--show-ir-diff` | Print IR before and after optimization |
| `--no-autocorrect` | Disable automatic syntax error correction |
| `--testdir <dir>` | Test directory (default: `test/`) |
| `--out <dir>` | Output directory (default: `out/`) |

---

## Pipeline

```
source.mc
   │
   ▼  Lexer (OOP tokens: class, this, public, private, void, .)
   │
   ▼  Parser → AST (ClassDeclAST, ObjectDeclAST,
   │                MemberAccessAST, MemberAssignAST,
   │                MethodCallAST, ThisAccessAST, ThisAssignAST)
   │
   ▼  CodeGen → LLVM StructType per class
   │            methods → ClassName_method(%ClassName* this, ...)
   │            objects → alloca %ClassName on stack
   │            GEP for field read / write
   │
   ▼  Optimizer (O0–O3)
   │
   ▼  llc → .o    (--build)
   │
   ▼  clang → native binary
```

---

## Test suite (30 tests)

### Original tests (01–20)

| # | File | Feature |
|---|---|---|
| 01 | `01_arithmetic.mc` | Arithmetic, operator precedence |
| 02 | `02_if_else.mc` | If-else, max() |
| 03 | `03_while_loop.mc` | While loop, post-increment |
| 04 | `04_for_loop.mc` | For loop, factorial |
| 05 | `05_nested_loops.mc` | Nested for loops |
| 06 | `06_break_continue.mc` | break / continue |
| 07 | `07_logical_ops.mc` | `&&` `\|\|` `!` |
| 08 | `08_comparisons.mc` | `==` `!=` `>=` `<=` |
| 09 | `09_arrays.mc` | Array declare/write/read |
| 10 | `10_functions.mc` | Multiple functions |
| 11 | `11_recursion_fib.mc` | Recursive Fibonacci |
| 12 | `12_recursion_factorial.mc` | Recursive factorial |
| 13 | `13_unary.mc` | Unary minus, abs() |
| 14 | `14_nested_if.mc` | Deeply nested if-else |
| 15 | `15_bubble_sort.mc` | Bubble sort |
| 16 | `16_scoping.mc` | Block scoping |
| 17 | `17_gcd.mc` | GCD (Euclidean) |
| 18 | `18_power.mc` | Iterative power |
| 19 | `19_post_increment.mc` | Post-increment return |
| 20 | `20_complex.mc` | Combined features |

### OOP tests (21–30)

| # | File | OOP feature tested | Expected exit |
|---|---|---|---|
| 21 | `21_class_basic.mc` | Class, field, getter/setter methods | 42 |
| 22 | `22_class_fields.mc` | Multiple fields, arithmetic method | 30 |
| 23 | `23_two_objects.mc` | Two independent object instances | 7 |
| 24 | `24_method_calls_method.mc` | `this.method()` inside a method | 100 |
| 25 | `25_method_logic.mc` | if/else inside method, abs() | 15 |
| 26 | `26_method_loop.mc` | Loop inside method, accumulator | 55 |
| 27 | `27_access_modifiers.mc` | `public` / `private` modifiers | 77 |
| 28 | `28_class_and_functions.mc` | Class + free function interop | 25 |
| 29 | `29_two_classes.mc` | Two different classes in one program | 12 |
| 30 | `30_oop_complex.mc` | Stack class + arrays + loops | 60 |

---

## Output files

| File | Description |
|---|---|
| `<stem>.ll` | LLVM IR — always produced |
| `<stem>.o` | Object file — with `--build` |
| `<stem>` | Native executable — with `--build` |
| `<stem>_corrected.mc` | Auto-corrected source (when errors detected) |

---

## LLVM IR for OOP constructs

Given:
```
class Point { int x; int y; }
Point p;
p.x = 10;
int v = p.x;
p.getX();
```

The compiler emits approximately:
```llvm
%Point = type { i32, i32 }           ; struct for the class

%p = alloca %Point                   ; object on stack

; p.x = 10
%p.x.ptr = getelementptr %Point, %Point* %p, i32 0, i32 0
store i32 10, i32* %p.x.ptr

; int v = p.x
%v_load = load i32, i32* %p.x.ptr

; p.getX() → compiled as Point_getX(%Point* %p)
%call = call i32 @Point_getX(%Point* %p)
```

---

## Known limitations

- No heap allocation (`new`/`delete`) — all objects live on the stack.  
- No inheritance or virtual dispatch.  
- `public` / `private` are parsed but access control is not enforced.  
- No constructor syntax; use an explicit `init()` method instead.  
- Arrays inside class fields are not yet supported (use local arrays + object for index tracking, as shown in test 30).

## Expected exit codes (with --build)

| Test | Expected |
|---|---|
| 01_arithmetic | 16 (`10 + 3*2`) |
| 02_if_else | 42 |
| 03_while_loop | 55 (1+2+…+10) |
| 04_for_loop | 120 (5!) |
| 05_nested_loops | 16 (4×4) |
| 06_break_continue | 50 (1+2+3+4+6+7+8+9+10, skip 5, stop at 11) |
| 07_logical_ops | 7 (bits 0+1+2 set) |
| 08_comparisons | 15 (all four comparisons pass) |
| 09_arrays | 150 (10+20+30+40+50) |
| 10_functions | 25 (3²+4²) |
| 11_recursion_fib | 55 (fib(10)) |
| 12_recursion_factorial | 96 (7! = 5040 mod 256 = 96 on 8-bit exit) |
| 13_unary | 15 |
| 14_nested_if | 3 (score 82 → B) |
| 15_bubble_sort | 12 (smallest element after sort) |
| 16_scoping | 31 (10+20+1) |
| 17_gcd | 6 (gcd(48,18)) |
| 18_power | 0 (256 mod 256 = 0) |
| 19_post_increment | 18 (7+5+6) |
| 20_complex | varies |

> Note: Linux `exit()` codes are 8-bit (0–255). Values > 255 wrap around.
