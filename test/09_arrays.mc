int main() {
    int arr[5];
    int i;
    int sum;
    sum = 0;
    arr[0] = 10;
    arr[1] = 20;
    arr[2] = 30;
    arr[3] = 40;
    arr[4] = 50;
    for (i = 0; i < 5; i++) {
        sum = sum + arr[i];
    }
    return sum;
}
