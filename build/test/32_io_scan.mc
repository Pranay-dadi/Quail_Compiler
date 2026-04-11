/* ============================================================
   32_io_scan.mc  —  Quail Compiler  (I/O: scan / user input)

   Demonstrates:
     • scan(var)          — read one integer from stdin
     • scan(a, b)         — read multiple values at once
     • combining scan with arithmetic and control flow
   ============================================================ */

int max_of(int a, int b) {
    if (a > b) {
        return a;
    }
    return b;
}

int min_of(int a, int b) {
    if (a < b) {
        return a;
    }
    return b;
}

int abs_val(int x) {
    if (x < 0) {
        return 0 - x;
    }
    return x;
}

int main() {
    int a;
    int b;
    int c;

    println("=== Quail scan() Demo ===");
    println();

    // ── Single scan ───────────────────────────────────────────
    println("Enter three integers (one per line):");
    scan(a);
    scan(b);
    scan(c);

    // ── Multi-value results ───────────────────────────────────
    println();
    println("--- Results ---");
    println("a =", a);
    println("b =", b);
    println("c =", c);
    println();

    println("Sum        :", a + b + c);
    println("Product    :", a * b * c);
    println("Max(a,b)   :", max_of(a, b));
    println("Min(a,b)   :", min_of(a, b));
    println("Max of all :", max_of(max_of(a, b), c));
    println("|a - b|    :", abs_val(a - b));
    println();

    // ── Conditional on scanned input ─────────────────────────
    int total;
    total = a + b + c;
    print("Grade: ");
    if (total >= 90) {
        println("A");
    }
    if (total >= 75) {
        println("B");
    }
    if (total >= 60) {
        println("C");
    } else {
        println("F");
    }

    println("=== Done ===");
    return total;
}
