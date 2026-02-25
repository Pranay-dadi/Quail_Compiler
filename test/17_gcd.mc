int gcd(int a, int b) {
    int tmp;
    while (b != 0) {
        tmp = b;
        b = a - (a / b) * b;
        a = tmp;
    }
    return a;
}

int main() {
    return gcd(48, 18);
}
