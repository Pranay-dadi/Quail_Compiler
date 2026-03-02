# Quail Compiler

A hand-written compiler for a C-like language that targets LLVM IR.

Type this command to use llvm -
sudo apt update
sudo apt install -y \
  llvm \
  llvm-dev \
  llvm-runtime \
  llvm-tools \
  clang

## Language Features

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
| Arrays | `int arr[10];  arr[0] = 1;  arr[0]` |
| Functions | `int add(int a, int b) { return a+b; }` |
| Recursion | `int fib(int n) { … return fib(n-1)+fib(n-2); }` |
| Unary minus | `-x` |
| Post-increment | `i++` |
| Scoped blocks | `{ int tmp; … }` |

## Build

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Usage

### Compile a single file
```bash
# Generate LLVM IR only
./Quail_Compiler test/01_arithmetic.mc

# Generate IR + produce and run executable
./Quail_Compiler --build test/01_arithmetic.mc

# Show coloured token table + AST
./Quail_Compiler --debug test/01_arithmetic.mc
```

### Run all 20 tests at once
```bash
# IR only (fastest)
./Quail_Compiler --test-all

# Build + run every test and show exit codes
./Quail_Compiler --test-all --build

# Build + run every test and show exit codes and disable autocorrect feature
./Quail_Compiler --test-all --build --no-autocorrect 

# Custom directories
./Quail_Compiler --test-all --build --testdir ../test --out ../out
```

Or use the helper script from the project root:
```bash
chmod +x run_tests.sh
./run_tests.sh             # IR only
./run_tests.sh --build     # build + run
```

## Output files

All outputs land in `out/` (or the directory given with `--out`):

| File | Description |
|---|---|
| `<stem>.ll` | LLVM IR — always produced |
| `<stem>.o` | Object file — with `--build` |
| `<stem>` | Executable — with `--build` |

## Test suite (20 tests)

| # | File | Feature tested |
|---|---|---|
| 01 | `01_arithmetic.mc` | Basic arithmetic, operator precedence |
| 02 | `02_if_else.mc` | If-else, function calls, max() |
| 03 | `03_while_loop.mc` | While loop, post-increment |
| 04 | `04_for_loop.mc` | For loop, factorial product |
| 05 | `05_nested_loops.mc` | Nested for loops |
| 06 | `06_break_continue.mc` | Break and continue |
| 07 | `07_logical_ops.mc` | `&&` `\|\|` `!` operators |
| 08 | `08_comparisons.mc` | `==` `!=` `>=` `<=` |
| 09 | `09_arrays.mc` | Array declare, write, read, loop |
| 10 | `10_functions.mc` | Multiple functions, nested calls |
| 11 | `11_recursion_fib.mc` | Recursive Fibonacci |
| 12 | `12_recursion_factorial.mc` | Recursive factorial |
| 13 | `13_unary.mc` | Unary minus, abs() |
| 14 | `14_nested_if.mc` | Deeply nested if-else (grade) |
| 15 | `15_bubble_sort.mc` | Arrays + nested loops (bubble sort) |
| 16 | `16_scoping.mc` | Block scoping, shadow variables |
| 17 | `17_gcd.mc` | GCD via Euclidean algorithm |
| 18 | `18_power.mc` | Iterative power function |
| 19 | `19_post_increment.mc` | Post-increment return value |
| 20 | `20_complex.mc` | Combined: arrays, functions, loops, logic |

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

## Pipeline

```
source.mc
   │
   ▼  Lexer       → token stream
   │
   ▼  Parser      → AST
   │
   ▼  CodeGen     → LLVM IR  (.ll)
   │
   ▼  llc         → object   (.o)
   │
   ▼  clang       → binary
```
