int main() {
    int a;
    int b;
    int result;
    a = 5;
    b = 10;
    result = 0;
    if (a > 0 && b > 0) {
        result = result + 1;
    }
    if (a > 100 || b > 5) {
        result = result + 2;
    }
    if (!a == 0) {
        result = result + 4;
    }
    return result;
}
