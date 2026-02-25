int factorial(int n) {
    int result;
    result = 1            /* missing semicolon */
    int i;
    i = 1;
    while (i <= n) {
        result = result * i    /* missing semicolon */
        i++;
    }
    return result         /* missing semicolon */
}

int sum_array(int arr, int len) {
    int total;
    total = 0;
    int k;
    k = 0;
    while (k < len) {
        total = total + k     /* missing semicolon */
        k++;
    }
    return total;
}

int main() {
    int x
    x = 5;
    int y;
    y = factorial(x)      /* missing semicolon */
    int arr[4];
    arr[0] = 1;
    arr[1] = 2;
    arr[2] = 3;
    arr[3] = 4;
    int s;
    s = sum_array(arr[0], 4);
    if (y > 100) {
        y = 100;
    }
    return y;
}
