int max(int a, int b) {
    if (a > b) {
        return a;
    } else {
        return b;
    }
}

int main() {
    int x;
    int y;
    int result;
    x = 42;
    y = 17;
    result = max(x, y);
    return result;
}
