/* ============================================================
   31_io_print.mc  —  Quail Compiler  (I/O: print / println)

   Demonstrates:
     • println("string")       — print string with newline
     • print("string")         — print string without newline
     • println(intExpr)        — print integer expression
     • println(floatExpr)      — print float expression
     • print(a, b, c)          — multiple values on one call
     • println()               — blank line
   ============================================================ */

int double_it(int n) {
    return n * 2;
}

int main() {
    int   age;
    float gpa;
    int   score;

    age   = 20;
    gpa   = 3.85;
    score = double_it(21);

    // ── Basic string output ───────────────────────────────────
    println("=== Quail I/O Demo ===");
    println();

    // ── Inline print then newline ─────────────────────────────
    print("Name  : ");
    println("Quail Student");

    print("Age   : ");
    println(age);

    print("GPA   : ");
    println(gpa);

    // ── Multiple values in one println ───────────────────────
    println("Score (doubled):", score);

    // ── Arithmetic directly inside println ────────────────────
    println("Age next year:", age + 1);
    println("GPA floored  :", age / 7);

    // ── Escape sequences inside strings ──────────────────────
    println();
    println("Tab demo:");
    print("Column A\tColumn B\tColumn C");
    println();

    // ── Nested string messages ────────────────────────────────
    println();
    print("Pass? ");
    if (score > 30) {
        println("YES");
    } else {
        println("NO");
    }

    println();
    println("=== Done ===");
    return score;
}
