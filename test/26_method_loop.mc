// Test 26: Method with a loop — accumulate sum 1..n
// Expected exit code: 55  (sum 1+2+...+10)

class Accumulator {
    int total;

    void reset() {
        this.total = 0;
    }

    void add(int v) {
        this.total = this.total + v;
    }

    int result() {
        return this.total;
    }
}

int main() {
    Accumulator acc;
    acc.reset();
    int i;
    i = 1;
    while (i <= 10) {
        acc.add(i);
        i++;
    }
    return acc.result();
}

