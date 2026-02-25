int main() {
    int sum;
    int i;
    sum = 0;
    for (i = 1; i <= 20; i++) {
        if (i == 11) {
            break;
        }
        if (i == 5) {
            continue;
        }
        sum = sum + i;
    }
    return sum;
}
