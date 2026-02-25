int clamp(int val, int lo, int hi) {
    if (val < lo) {
        return lo;
    }
    if (val > hi) {
        return hi;
    }
    return val;
}

int sumArray(int n) {
    int arr[10];
    int i;
    int total;
    total = 0;
    for (i = 0; i < n; i++) {
        arr[i] = i * i;
    }
    for (i = 0; i < n; i++) {
        total = total + clamp(arr[i], 1, 20);
    }
    return total;
}

int main() {
    int a;
    int b;
    int i;
    a = sumArray(6);
    b = 0;
    i = 1;
    while (i <= 5) {
        if (i != 3 && i != 5) {
            b = b + i;
        }
        i++;
    }
    return a + b;
}
