int abs(int n) {
    if (n < 0) {
        return -n;
    }
    return n;
}

int main() {
    int a;
    int b;
    a = -15;
    b = abs(a);
    return b;
}
