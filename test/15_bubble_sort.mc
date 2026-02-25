int swap(int a, int b) {
    int tmp;
    tmp = a;
    a = b;
    b = tmp;
    return a;
}

int main() {
    int arr[5];
    int i;
    int j;
    int tmp;
    arr[0] = 64;
    arr[1] = 34;
    arr[2] = 25;
    arr[3] = 12;
    arr[4] = 22;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4 - i; j++) {
            if (arr[j] > arr[j + 1]) {
                tmp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = tmp;
            }
        }
    }
    return arr[0];
}
