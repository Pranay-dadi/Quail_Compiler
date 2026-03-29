// Test 21: Basic class — field access and a getter method
// Expected exit code: 42

class Box {
    int value;

    int getValue() {
        return this.value;
    }

    void setValue(int v) {
        this.value = v;
    }
}

int main() {
    Box b;
    b.setValue(42);
    int result = b.getValue();
    return result;
}

