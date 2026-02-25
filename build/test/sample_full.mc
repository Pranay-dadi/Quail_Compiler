/*
 * sample_full.mc
 * Quail Compiler v2.0 demo
 * Tests: comments, arrays, loops, recursion, conditionals
 */

// ── Utility: compute the nth Fibonacci number ──────────────────
// This function uses a simple iterative approach (no recursion)
// so the optimizer can apply loop-invariant code motion + unrolling.
int fibonacci(int n) {
    // Edge cases: fib(0) = 0, fib(1) = 1
    if (n <= 1) {
        return n;
    }

    int a;   // previous value
    int b;   // current value
    int tmp; // swap register
    int i;   // loop counter

    a = 0;
    b = 1;
    i = 2;

    /* Main Fibonacci loop.
       Each iteration: tmp = a + b; a = b; b = tmp; */
    while (i <= n) {
        tmp = a + b;
        a = b;
        b = tmp;
        i++;
    }

    return b; // nth Fibonacci
}

// ── Sum all elements in an integer array ───────────────────────
int sum_array(int len) {
    int arr[8];        // fixed-size array, up to 8 elements

    /* Populate: arr[k] = k * 2 */
    int k;
    k = 0;
    while (k < len) {
        arr[k] = k * 2;
        k++;
    }

    // Accumulate the sum
    int total;
    int j;
    total = 0;
    j = 0;
    while (j < len) {
        total = total + arr[j]; // add element
        j++;
    }

    return total;
}

// ── Count-down loop using for ──────────────────────────────────
// Returns the product of numbers from 1..n (factorial)
// Uses a for loop so the optimizer can vectorize.
int factorial(int n) {
    int result;
    result = 1;

    /* for-loop factorial */
    int i;
    for (i = 1; i <= n; i++) {
        result = result * i;
    }

    /* clamp to avoid overflow in IR test */
    if (result > 1000) {
        result = 1000;
    }

    return result;
}

// ── Main entry point ───────────────────────────────────────────
int main() {
    /* --- Test fibonacci --- */
    int fib10;
    fib10 = fibonacci(10);  // fib(10) = 55

    /* --- Test sum_array with 6 elements ---
       sum of [0,2,4,6,8,10] = 30 */
    int s;
    s = sum_array(6);

    /* --- Test factorial --- */
    int f;
    f = factorial(7);       // 5040, clamped to 1000

    // Combine results to produce a non-trivial return value
    // so the optimizer cannot simply dead-code-eliminate everything.
    int result;
    result = fib10 + s + f;

    // Conditional bonus: if result is large enough, cap at 255
    // (useful for testing the conditional branch optimizer)
    if (result > 255) {
        result = 255;
    }

    return result; // exit code visible via ; echo $?
}
