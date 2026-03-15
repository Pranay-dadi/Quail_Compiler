// Test 23: Two independent object instances
// Expected exit code: 7  (a.value + b.value = 3 + 4)

class Num {
    int value;

    void set(int v) {
        this.value = v;
    }

    int get() {
        return this.value;
    }
}

int main() {
    Num a;
    Num b;
    a.set(3);
    b.set(4);
    int result = a.get() + b.get();
    return result;
}

