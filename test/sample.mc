int add(int a, int b) {
    int c;
    c = a + b;
    return c;
}

int main() {
    int x;
    x = add(3, 4);

    int arr[5];
    arr[0] = x;

    if (x > 5) {
        x = x - 1;
    }

    while (x > 0) {
        x = x - 1;
    }

    return x;
}
