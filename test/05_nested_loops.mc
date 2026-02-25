int main() {
    int count;
    int i;
    int j;
    count = 0;
    for (i = 1; i <= 4; i++) {
        for (j = 1; j <= 4; j++) {
            count = count + 1;
        }
    }
    return count;
}
