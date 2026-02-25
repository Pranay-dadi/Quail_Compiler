int add(int a, int b) {
    return a + b;
}

int multiply(int a, int b) {
    return a * b;
}

int square(int n) {
    return multiply(n, n);
}

int main() {
    int result;
    result = add(square(3), square(4));
    return result;
}
