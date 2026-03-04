// Test 25: Method with if/else logic — absolute value
// Expected exit code: 15  (abs(-15) = 15)

class MathHelper {
    int value;

    void init(int v) {
        this.value = v;
    }

    int abs() {
        if (this.value < 0) {
            return this.value * -1;
        } else {
            return this.value;
        }
    }

    int max(int other) {
        if (this.value > other) {
            return this.value;
        } else {
            return other;
        }
    }
}

int main() {
    MathHelper m;
    m.init(-15);
    int result = m.abs();
    return result;
}
